// Copyright 2026 RASPIAUDIO

#include "pi_ai_mic_neopixel.h"
#include "pi_ai_mic_hw.h"

#include "app_conf.h"
#include "v2_doa_display.h"

#include <math.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "rtos_macros.h"
#include "rtos_printf.h"
#include "task.h"
#include "xcore/hwtimer.h"
#include "xcore/interrupt.h"
#include "xcore/port.h"

#define WS2812_TICKS_TOTAL       125u
#define WS2812_T0H_TICKS         40u
#define WS2812_T1H_TICKS         80u
#define WS2812_RESET_TICKS       30000u

static volatile float latest_azimuth_rad = 0.0f;
static volatile float latest_speech_energy = 0.0f;
static volatile uint32_t latest_doa_seq = 0;

static volatile uint8_t latest_ui_state = PI_AI_MIC_UI_INIT;
static volatile uint8_t latest_ui_progress = 0;
static volatile bool latest_mic_mute = false;

void pi_ai_mic_ui_set_state(uint8_t state)
{
    if (state < PI_AI_MIC_UI_STATE_COUNT) {
        latest_ui_state = state;
    }
}

uint8_t pi_ai_mic_ui_get_state(void)
{
    return latest_ui_state;
}

void pi_ai_mic_ui_set_progress(uint8_t progress)
{
    latest_ui_progress = progress > 100u ? 100u : progress;
}

uint8_t pi_ai_mic_ui_get_progress(void)
{
    return latest_ui_progress;
}

void pi_ai_mic_set_mic_mute(bool muted)
{
    latest_mic_mute = muted;
}

bool pi_ai_mic_get_mic_mute(void)
{
    return latest_mic_mute;
}

void pi_ai_mic_neopixel_publish_doa(float azimuth_rad, float speech_energy)
{
#if PI_AI_MIC_DOA_NEOPIXEL_ENABLE
    latest_azimuth_rad = azimuth_rad;
    latest_speech_energy = speech_energy;
    latest_doa_seq++;
#else
    (void) azimuth_rad;
    (void) speech_energy;
#endif
}

#if PI_AI_MIC_DOA_NEOPIXEL_ENABLE && ON_TILE(1)
static uint8_t clamp_u8(unsigned value)
{
    return (uint8_t)(value > 255u ? 255u : value);
}

static uint8_t scale_u8(uint8_t value, unsigned brightness)
{
    return (uint8_t)(((unsigned)value * brightness + 127u) / 255u);
}

static uint8_t add_sat_u8(uint8_t a, unsigned b)
{
    unsigned sum = (unsigned)a + b;
    return clamp_u8(sum);
}

static unsigned ring_distance(unsigned a, unsigned b)
{
    unsigned count = PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT;
    unsigned forward = (a >= b) ? (a - b) : (a + count - b);
    unsigned backward = count - forward;

    return (forward < backward) ? forward : backward;
}

static void color_wheel(uint8_t pos, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    if (pos < 85u) {
        *red = (uint8_t)(255u - pos * 3u);
        *green = (uint8_t)(pos * 3u);
        *blue = 0u;
    } else if (pos < 170u) {
        pos = (uint8_t)(pos - 85u);
        *red = 0u;
        *green = (uint8_t)(255u - pos * 3u);
        *blue = (uint8_t)(pos * 3u);
    } else {
        pos = (uint8_t)(pos - 170u);
        *red = (uint8_t)(pos * 3u);
        *green = 0u;
        *blue = (uint8_t)(255u - pos * 3u);
    }
}

static uint32_t interrupt_state_get(void)
{
    uint32_t state;

    asm volatile(
        "getsr r11, %1\n"
        "mov %0, r11"
        : "=r"(state)
        : "n"(XS1_SR_IEBLE_MASK)
        : "r11"
    );

    return state;
}

static void interrupt_restore(uint32_t state)
{
    if (state) {
        interrupt_unmask_all();
    }
}

static void ws2812_send_bit(port_t port, port_timestamp_t *time, unsigned bit)
{
    port_timestamp_t start = *time;
    port_timestamp_t high_ticks = bit ? WS2812_T1H_TICKS : WS2812_T0H_TICKS;

    port_out_at_time(port, start, 1u);
    port_out_at_time(port, start + high_ticks, 0u);

    *time = start + WS2812_TICKS_TOTAL;
}

static void ws2812_send_byte(port_t port, port_timestamp_t *time, uint8_t value)
{
    for (int bit = 7; bit >= 0; bit--) {
        ws2812_send_bit(port, time, (value >> bit) & 0x01u);
    }
}

static void ws2812_send_rgb(port_t port, port_timestamp_t *time,
                            uint8_t red, uint8_t green, uint8_t blue)
{
    ws2812_send_byte(port, time, green);
    ws2812_send_byte(port, time, red);
    ws2812_send_byte(port, time, blue);
}

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} ws2812_pixel_t;

static void ws2812_send_frame(port_t port, hwtimer_t timer,
                              const ws2812_pixel_t *pixels, unsigned count)
{
    uint32_t interrupt_state = interrupt_state_get();

    interrupt_mask_all();
    port_sync(port);
    port_timestamp_t time = port_get_trigger_time(port) + 100u;

    // Keep all color computation out of the timing-critical WS2812 stream.
    for (unsigned led = 0; led < count; led++) {
        ws2812_send_rgb(port, &time,
                        pixels[led].red,
                        pixels[led].green,
                        pixels[led].blue);
    }

    // Wait for the last scheduled edge before allowing an RTOS preemption.
    port_sync(port);
    interrupt_restore(interrupt_state);
    hwtimer_delay(timer, WS2812_RESET_TICKS);
}

static int doa_led_index(float azimuth_rad)
{
    const float two_pi = 6.28318530717958647692f;
    float normalized = fmodf(azimuth_rad, two_pi);

    if (normalized < 0.0f) {
        normalized += two_pi;
    }

#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    if (pi_ai_mic_doa_neopixel_mirror()) {
        normalized = fmodf((0.5f * two_pi) - normalized, two_pi);
        if (normalized < 0.0f) {
            normalized += two_pi;
        }
    }
#elif PI_AI_MIC_DOA_NEOPIXEL_MIRROR_90_270
    normalized = fmodf((0.5f * two_pi) - normalized, two_pi);
    if (normalized < 0.0f) {
        normalized += two_pi;
    }
#endif

    int index = (int)((normalized * (float)PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT / two_pi) + 0.5f);
    if (index >= PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT) {
        index = 0;
    }
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    if (pi_ai_mic_doa_neopixel_reverse() && index != 0) {
        index = PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT - index;
    }
#elif PI_AI_MIC_DOA_NEOPIXEL_REVERSE_ORDER
    if (index != 0) {
        index = PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT - index;
    }
#endif
    return index;
}

static void render_boot_frame(port_t port, hwtimer_t timer, unsigned frame)
{
    uint32_t interrupt_state = interrupt_state_get();
    interrupt_mask_all();

    port_sync(port);
    port_timestamp_t time = port_get_trigger_time(port) + 100u;

    unsigned active = frame % PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT;
    unsigned brightness = PI_AI_MIC_DOA_NEOPIXEL_BRIGHTNESS * 2u;
    if (brightness > 56u) {
        brightness = 56u;
    }

    for (unsigned led = 0; led < PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT; led++) {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t hue = (uint8_t)(((led * 256u) / PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT +
                                  frame * 7u) & 0xffu);
        unsigned dist = ring_distance(led, active);

        color_wheel(hue, &red, &green, &blue);
        red = scale_u8(red, brightness);
        green = scale_u8(green, brightness);
        blue = scale_u8(blue, brightness);

        if (dist == 0u) {
            red = add_sat_u8(red, brightness);
            green = add_sat_u8(green, brightness);
            blue = add_sat_u8(blue, brightness);
        } else if (dist == 1u) {
            red = add_sat_u8(red, brightness / 4u);
            green = add_sat_u8(green, brightness / 4u);
            blue = add_sat_u8(blue, brightness / 4u);
        }

        ws2812_send_rgb(port, &time, red, green, blue);
    }

    interrupt_restore(interrupt_state);
    hwtimer_delay(timer, WS2812_RESET_TICKS);
}

static void render_ring(port_t port, hwtimer_t timer)
{
#if PI_AI_MIC_DOA_NEOPIXEL_SELFTEST
    static unsigned selftest_index = 0;
    unsigned selftest_active = selftest_index % PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT;
    uint8_t white = clamp_u8(PI_AI_MIC_DOA_NEOPIXEL_BRIGHTNESS);
    uint32_t interrupt_state = interrupt_state_get();

    interrupt_mask_all();

    port_sync(port);
    port_timestamp_t time = port_get_trigger_time(port) + 100u;

    for (int led = 0; led < PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT; led++) {
        uint8_t level = (led == (int)selftest_active) ? white : 0;
        ws2812_send_rgb(port, &time, level, level, level);
    }
    selftest_index = (selftest_active + 1u) % PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT;

    interrupt_restore(interrupt_state);
    hwtimer_delay(timer, WS2812_RESET_TICKS);
    return;
#endif

    uint32_t seq = latest_doa_seq;
    float azimuth = latest_azimuth_rad;
    float energy = latest_speech_energy;
    int active = -1;
    unsigned brightness = PI_AI_MIC_DOA_NEOPIXEL_BRIGHTNESS;

    static v2_doa_display_t display;
    /* The accepted V2 mapper has no mirror/reversal. Transform its angle,
       not the beamformer geometry or microphone lane order. */
    static uint32_t observed_seq, update_ms;
    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (seq != observed_seq) { observed_seq = seq; update_ms = now_ms; }
    active = v2_doa_display_step(&display,
        seq && now_ms - update_ms <= 500u ? azimuth : NAN, energy, now_ms,
        PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT);

    /* Hold brightness as well as direction during short speech gaps. */

    uint32_t interrupt_state = interrupt_state_get();
    interrupt_mask_all();

    port_sync(port);
    port_timestamp_t time = port_get_trigger_time(port) + 100u;

    for (int led = 0; led < PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT; led++) {
        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;

        if (active >= 0) {
            int prev = (active + PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT - 1) %
                       PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT;
            int next = (active + 1) % PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT;

            if (led == active) {
                green = clamp_u8(brightness);
                blue = clamp_u8(brightness / 4u);
            } else if (led == prev || led == next) {
                green = clamp_u8(brightness / 6u);
                blue = clamp_u8(brightness / 16u);
            }
        }

        ws2812_send_rgb(port, &time, red, green, blue);
    }

    interrupt_restore(interrupt_state);
    hwtimer_delay(timer, WS2812_RESET_TICKS);
}

static void render_ui_frame(port_t port, hwtimer_t timer,
                            uint8_t state, uint8_t progress, unsigned frame)
{
    ws2812_pixel_t pixels[PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT] = {0};
    unsigned count = PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT;
    unsigned brightness = PI_AI_MIC_DOA_NEOPIXEL_BRIGHTNESS;
    unsigned slow_pos = (frame / 2u) % count;
    unsigned fast_pos = frame % count;
    unsigned reverse_pos = (count - (frame % count)) % count;
    unsigned pulse_phase = frame % 20u;
    unsigned pulse = pulse_phase <= 10u ? pulse_phase : 20u - pulse_phase;
    unsigned progress_leds = (progress * count + 99u) / 100u;
    unsigned twinkle_b = (slow_pos + count / 3u) % count;
    unsigned twinkle_c = (slow_pos + (2u * count) / 3u) % count;
    for (unsigned led = 0; led < count; led++) {
        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;
        unsigned level;

        switch (state) {
            case PI_AI_MIC_UI_INIT:
                if (led == slow_pos) {
                    green = clamp_u8(brightness);
                    blue = clamp_u8(brightness);
                }
                break;

            case PI_AI_MIC_UI_BLE_IMPROV:
                if (led == slow_pos) {
                    blue = clamp_u8(brightness);
                    green = clamp_u8(brightness / 3u);
                }
                break;

            case PI_AI_MIC_UI_NO_HA:
                level = (brightness * (3u + pulse)) / 13u;
                red = clamp_u8(level);
                green = clamp_u8(level / 3u);
                break;

            case PI_AI_MIC_UI_WAITING:
                blue = clamp_u8(brightness / 16u);
                if (led == slow_pos) {
                    blue = clamp_u8(brightness);
                    green = clamp_u8(brightness / 3u);
                } else if (led == twinkle_b) {
                    blue = clamp_u8((brightness * (5u + pulse)) / 15u);
                    green = clamp_u8(brightness / 8u);
                } else if (led == twinkle_c) {
                    blue = clamp_u8(brightness / 2u);
                }
                break;

            case PI_AI_MIC_UI_LISTENING:
                if (led == fast_pos) {
                    green = clamp_u8(brightness);
                    blue = clamp_u8(brightness / 2u);
                }
                break;

            case PI_AI_MIC_UI_THINKING:
                if (led == fast_pos || led == ((fast_pos + count / 2u) % count)) {
                    red = clamp_u8(brightness / 2u);
                    blue = clamp_u8(brightness);
                }
                break;

            case PI_AI_MIC_UI_REPLYING:
                if (led == reverse_pos) {
                    green = clamp_u8(brightness);
                    blue = clamp_u8(brightness / 5u);
                }
                break;

            case PI_AI_MIC_UI_ERROR:
            case PI_AI_MIC_UI_FLASH_ERROR:
                red = clamp_u8((brightness * pulse) / 10u);
                break;

            case PI_AI_MIC_UI_MUTED:
                if (led == 0u || led == count / 2u) {
                    red = clamp_u8(brightness);
                }
                break;

            case PI_AI_MIC_UI_JACK_IN:
                red = clamp_u8(brightness / 2u);
                blue = clamp_u8(brightness);
                break;

            case PI_AI_MIC_UI_JACK_OUT:
                green = clamp_u8(brightness);
                break;

            case PI_AI_MIC_UI_FLASHING:
                if (progress_leds > 0u && led < progress_leds) {
                    blue = clamp_u8(brightness);
                } else if (progress_leds == 0u && led == slow_pos) {
                    blue = clamp_u8(brightness);
                }
                break;

            case PI_AI_MIC_UI_FLASH_OK:
                green = clamp_u8(brightness);
                blue = clamp_u8(brightness / 5u);
                break;

            default:
                break;
        }

        pixels[led].red = red;
        pixels[led].green = green;
        pixels[led].blue = blue;
    }

    ws2812_send_frame(port, timer, pixels, count);
}

static void pi_ai_mic_neopixel_task(void *arg)
{
    (void) arg;

    port_t port = PI_AI_MIC_DOA_NEOPIXEL_PORT;
    hwtimer_t timer = hwtimer_alloc();

    port_enable(port);
    port_write_control_word(port, XS1_SETC_DRIVE_DRIVE);
    port_out(port, 0u);
    hwtimer_delay(timer, WS2812_RESET_TICKS);

    rtos_printf("PI_AI_MIC DOA NeoPixel task start: port=X1D13/XS1_PORT_1F leds=%u period_ms=%u\n",
                (unsigned)PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT,
                (unsigned)PI_AI_MIC_DOA_NEOPIXEL_PERIOD_MS);

#if PI_AI_MIC_DOA_NEOPIXEL_BOOT_ANIM_ENABLE && !PI_AI_MIC_DOA_NEOPIXEL_SELFTEST
    for (unsigned frame = 0; frame < PI_AI_MIC_DOA_NEOPIXEL_BOOT_ANIM_FRAMES; frame++) {
        render_boot_frame(port, timer, frame);
        vTaskDelay(pdMS_TO_TICKS(PI_AI_MIC_DOA_NEOPIXEL_PERIOD_MS));
    }
#endif

    unsigned ui_frame = 0;
    uint8_t previous_state = 0xFFu;
    for (;;) {
        uint8_t state = latest_ui_state;
        if (latest_mic_mute && state == PI_AI_MIC_UI_DOA_IDLE) {
            state = PI_AI_MIC_UI_MUTED;
        }
        if (state != previous_state) {
            previous_state = state;
            ui_frame = 0;
        }
        if (state == PI_AI_MIC_UI_DOA_IDLE) {
            render_ring(port, timer);
        } else {
            render_ui_frame(port, timer, state, latest_ui_progress, ui_frame);
        }
        ui_frame++;
        vTaskDelay(pdMS_TO_TICKS(PI_AI_MIC_DOA_NEOPIXEL_PERIOD_MS));
    }
}
#endif

int pi_ai_mic_neopixel_start_task(void)
{
#if PI_AI_MIC_DOA_NEOPIXEL_ENABLE && ON_TILE(1)
    BaseType_t task_ok = xTaskCreate(
        pi_ai_mic_neopixel_task,
        "PI_AI_MIC doa ring",
        RTOS_THREAD_STACK_SIZE(pi_ai_mic_neopixel_task),
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    if (task_ok != pdPASS) {
        rtos_printf("PI_AI_MIC DOA NeoPixel task create failed\n");
        return -1;
    }
    return 0;
#else
    return 0;
#endif
}
