// Copyright 2026 RASPIAUDIO

#include "pi_ai_mic_neopixel.h"
#include "pi_ai_mic_hw.h"

#include "app_conf.h"

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

    if (seq != 0u && isfinite(azimuth)) {
        active = doa_led_index(azimuth);
    }

    if (!(energy > 0.0f)) {
        brightness = brightness / 3u;
        if (brightness < 2u) {
            brightness = 2u;
        }
    }

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

    for (;;) {
        render_ring(port, timer);
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
