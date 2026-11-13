#include "voicedsp_v2_audio.h"
#include "voicedsp_v2_equalizer.h"
#include "voicedsp_v2_commands.h"
#include "voicedsp_v2_gesture.h"
#include "vdsp_usb_link.h"
#include "vdsp_usb_state.h"
#include "vdsp_usb_profile.h"
#include "vdsp_usb_filter.h"
#include "vdsp_usb_boot.h"
#include "pi_ai_mic_neopixel.h"
#include "FreeRTOS.h"
#include "rtos_macros.h"
#include "task.h"
#include <string.h>

#if ON_TILE(0)
#define VDSP_BUTTON_LONG_MS 4000
#include "voicedsp_v2_buttons.inc"
extern int vdsp_usb_i2c_transfer(vdsp_v2_i2c *, uint8_t, const uint8_t *, size_t, uint8_t *, size_t);
VDSP_FPTR("vdsp_wait_ms")
static void usb_wait(void *ctx, unsigned ms)
{ (void)ctx; vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1)); }
VDSP_FPTR("vdsp_gesture_now")
static uint32_t usb_now(void *ctx)
{ (void)ctx; return (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS; }
VDSP_FPTR("vdsp_gesture_transfer")
static int usb_gesture_transfer(void *ctx, uint8_t addr, const uint8_t *tx, size_t nt, uint8_t *rx, size_t nr)
{ return vdsp_usb_i2c_transfer(ctx, addr, tx, nt, rx, nr); }

static void usb_pack32(uint8_t *p, uint32_t v)
{ for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8*i)); }

static void usb_runtime_task(void *unused)
{
    (void)unused;
    xmos_bus_context context = {hwtimer_alloc()};
    vdsp_v2_i2c bus = {&context, bus_low, bus_release, bus_level, bus_delay, 1000};
    vdsp_v2_control control = {&bus, register_write, register_read, false, false};
    vdsp_v2_audio audio = {.control=&control, .wait_ms=usb_wait, .muted=true};
    vdsp_v2_audio_config config = {48000, 32, VDSP_DIN_PRIMARY, true};
    vdsp_v2_gesture sensor;
    vdsp_v2_buttons buttons = {0};
    vdsp_usb_gestures actions = {.attenuation=60};
    vdsp_v2_sample io = {0};
    vdsp_usb_reply link = {0};
    unsigned ui = PI_AI_MIC_UI_INIT, profile = 6;
    uint32_t epoch = UINT32_MAX, applied = UINT32_MAX, gesture_seq = 0, frames = 0;
    uint32_t applied_filter = UINT32_MAX;
    uint32_t last_progress = usb_now(NULL), jack_time = 0;
    uint8_t installed[4] = {0};
    bool fault = false, jack_previous = false, jack_seen = false, have_frames = false;
    int result = 0;
    port_enable(PORT_I2C_SCL); port_enable(PORT_I2C_SDA);
    port_write_control_word(PORT_I2C_SCL, XS1_SETC_DRIVE_PULL_UP);
    port_write_control_word(PORT_I2C_SDA, XS1_SETC_DRIVE_PULL_UP);
    bus_release(&context, VDSP_SDA); bus_release(&context, VDSP_SCL);
    result = vdsp_v2_control_init(&control);
    if (result || vdsp_v2_boot_pcal_result || vdsp_v2_boot_dac_reset_result || boot_geometry == VDSP_MIC_UNKNOWN) fault = true;
    vdsp_v2_gesture_init(&sensor, &bus, usb_gesture_transfer, usb_now);
    vdsp_v2_command_ready();
    for (;;) {
        uint32_t now = usb_now(NULL);
        uint8_t command[4];
        uint32_t accepted = vdsp_v2_command_get(command);
        vdsp_v2_crossover_config filter;
        uint32_t filter_revision=vdsp_v2_filter_get(&filter,NULL);
        if (accepted != epoch) {
            epoch = accepted;
            actions.muted = (command[3] & 1) != 0;
            actions.attenuation = command[2];
            /* Explicit commands can retry a fault, never automatically raise volume. */
            if (fault && command[1] == VDSP_AUDIO_OFF) {
                result = vdsp_v2_control_init(&control);
                fault = result != 0;
                audio.initialized = false;
            }
        }
        uint32_t sample_time = usb_now(NULL);
        int sampled = vdsp_v2_control_sample(&control, &io);
        if (sampled || io.microphone != boot_geometry) { result = sampled ? sampled : -10; fault = true; }
        bool jack = !io.valid || (((io.port0 >> 5) & 1u) == PI_AI_MIC_JACK_INSERTED_LEVEL);
        if (!jack_seen || jack != jack_previous) {
            jack_time = now; jack_previous = jack; jack_seen = true;
            applied = UINT32_MAX;
        }
        profile = command[1] == VDSP_AUDIO_HT326 ? 4 : 6;
        vdsp_usb_profile_requested = profile;
        int linked = vdsp_usb_link(profile, ui, &link);
        if (!linked && frames != link.frames) { last_progress = now; frames = link.frames; have_frames = true; }
        bool clocks = !linked && have_frames && now - last_progress < 500;
        /* USB enumeration can take arbitrarily long. Silence/wait, not a
         * latched hardware fault merely because the host has not started. */
        bool profiles = !linked && link.profile == profile && vdsp_usb_profile_applied == profile;
        if (!fault && profiles && clocks && !audio.initialized) result = vdsp_v2_audio_init(&audio, &config);
        if (result) fault = true;
        vdsp_v2_audio_route target = (vdsp_v2_audio_route)command[1];
        if (jack && target != VDSP_AUDIO_OFF) target = VDSP_AUDIO_LINE;
        if (fault || !profiles || !clocks) {
            if (audio.initialized && audio.route != VDSP_AUDIO_OFF) (void)vdsp_v2_audio_stop(&audio);
            applied = UINT32_MAX;
        } else if (audio.initialized && (applied != epoch || audio.route != target)) {
            bool mute_only = applied != UINT32_MAX && applied_filter == filter_revision && audio.route == target &&
                installed[1] == command[1] && installed[2] == command[2] &&
                (installed[3] & 2) == (command[3] & 2);
            if (!mute_only) {
                result = vdsp_v2_audio_stop(&audio);
                vdsp_v2_equalizer_pair eq;
                if (!result && target == VDSP_AUDIO_HT326 && (command[3] & 2)) {
                    result = vdsp_v2_crossover_apply_off(&audio, 48000, &filter, -(int)command[2], &eq);
                } else if (!result) {
                    result = vdsp_v2_equalizer_apply_pair_off(&audio, 48000, 0,0,0,0, -(int)command[2], &eq);
                }
                uint8_t newest[4];
                if (vdsp_v2_command_get(newest) != epoch) { applied=UINT32_MAX; continue; }
                if (!result) result = vdsp_v2_audio_select(&audio, target, true);
            }
            if (!result && target != VDSP_AUDIO_OFF) result = vdsp_v2_audio_mute(&audio, actions.muted);
            if (result) fault = true;
            else { applied = epoch; applied_filter=filter_revision; memcpy(installed,command,4); }
        }
        if (!fault && audio.initialized && vdsp_v2_audio_check(&audio)) { result = audio.last_error; fault = true; }
        vdsp_v2_gesture_step(&sensor, control.initialized, io.valid && io.gesture_irq);
        /* The sensor timestamps events during step(), after the loop began. */
        now = usb_now(NULL);
        if (gesture_seq != sensor.snapshot.sequence) {
            gesture_seq = sensor.snapshot.sequence;
            if (!fault && sensor.snapshot.ready && sensor.snapshot.valid && vdsp_usb_gesture_fresh(now, sensor.snapshot.event_ms)) {
                int changed = vdsp_usb_gesture(&actions, sensor.snapshot.event_raw, now);
                if (changed) {
                    command[2] = (uint8_t)actions.attenuation;
                    command[3] = (command[3] & 2) | (actions.muted ? 1 : 0);
                    (void)vdsp_usb_command_accept_source(command,3);
                }
            }
        }
        vdsp_v2_button_events be = vdsp_v2_buttons_update(&buttons, now,
            sample_time, !fault && !sampled && io.valid, io.buttons_pressed);
        /* Refresh after gestures to avoid overwriting another accepted action. */
        vdsp_v2_command_get(command);
        if (vdsp_usb_buttons_apply(be.pressed, be.short_press, be.long_press, command)) {
            actions.attenuation = command[2];
            (void)vdsp_usb_command_accept_source(command,4);
        }
        ui = fault ? PI_AI_MIC_UI_ERROR : !audio.initialized || !profiles ? PI_AI_MIC_UI_INIT :
             actions.muted ? PI_AI_MIC_UI_MUTED : now - jack_time < 750 ?
             (jack ? PI_AI_MIC_UI_JACK_IN : PI_AI_MIC_UI_JACK_OUT) : PI_AI_MIC_UI_DOA_IDLE;
        uint8_t status[VDSP_V2_STATUS_SIZE] = {0};
        status[0]=2; status[1]=audio.initialized; status[2]=command[1]; status[3]=audio.route;
        status[4]=ui; status[5]=(io.valid ? 1:0)|(io.buck_pg ? 4:0)|(jack ? 8:0)|
            (fault ? 16:0)|(audio.muted ? 32:0)|(clocks ? 64:0)|(profiles ? 128:0);
        status[6]=io.port0; status[7]=io.port1; status[8]=sensor.snapshot.ready;
        status[9]=sensor.snapshot.event_raw; status[10]=sensor.snapshot.event_raw>>8;
        status[11]=profile;
        usb_pack32(status+12,(uint32_t)result); usb_pack32(status+20,applied); usb_pack32(status+24,now);
        usb_pack32(status+28,frames); usb_pack32(status+32,sensor.snapshot.errors);
        usb_pack32(status+36,gesture_seq); status[40]=command[3]; status[41]=link.profile;
        status[42]=vdsp_usb_profile_applied; status[43]=io.microphone; status[44]=command[2];
        status[45]=target;
        vdsp_v2_status_publish(status);
        usb_wait(NULL,20);
    }
}
void vdsp_v2_runtime_start(void)
{
    vdsp_v2_command_start();
    configASSERT(xTaskCreate(usb_runtime_task,"USB audio",RTOS_THREAD_STACK_SIZE(usb_runtime_task),
                            NULL,appconfTEST_TASK_PRIORITY,NULL)==pdPASS);
}
#endif
