#include "app_conf.h"
#include "vdsp_usb_link.h"
#include "vdsp_usb_profile.h"
#include "voicedsp_v2_host_boot.h"
#include "pi_ai_mic_neopixel.h"
#include "platform/driver_instances.h"
#include "FreeRTOS.h"
#include "task.h"
#include "rtos_macros.h"
#include <string.h>
/* The SDK RTOS assert port references this even with logging disabled. */
int xcore_utils_printf(const char *format, ...) { (void)format; return 0; }
volatile uint32_t vdsp_usb_profile_requested;
volatile uint32_t vdsp_usb_profile_applied;
#if ON_TILE(0)
int vdsp_usb_link(unsigned profile, unsigned ui, vdsp_usb_reply *reply)
{
    static uint32_t sequence;
    vdsp_usb_request q = {++sequence, profile, ui};
    rtos_intertile_t *ctx = get_intertile_ctx();
    rtos_intertile_tx(ctx, VDSP_USB_LINK_PORT, &q, sizeof(q));
    TickType_t begin = xTaskGetTickCount();
    while (xTaskGetTickCount() - begin < pdMS_TO_TICKS(250)) {
        void *buf = NULL;
        size_t n = rtos_intertile_rx(ctx, VDSP_USB_LINK_PORT, &buf, pdMS_TO_TICKS(25));
        bool match = n == sizeof(*reply) && buf && ((vdsp_usb_reply *)buf)->sequence == q.sequence;
        if (match) memcpy(reply, buf, n);
        if (buf) rtos_osal_free(buf);
        if (match) return 0;
    }
    return -1;
}
#else
static void responder(void *unused)
{
    (void)unused;
    rtos_intertile_t *ctx = get_intertile_ctx();
    pi_ai_mic_ui_set_state(PI_AI_MIC_UI_INIT);
    for (;;) {
        void *buf = NULL;
        size_t n = rtos_intertile_rx(ctx, VDSP_USB_LINK_PORT, &buf, RTOS_OSAL_WAIT_FOREVER);
        if (buf && n == sizeof(vdsp_usb_request)) {
            vdsp_usb_request q;
            memcpy(&q, buf, n);
            if (q.profile == 4 || q.profile == 6) vdsp_usb_profile_requested = q.profile;
            if (q.ui < PI_AI_MIC_UI_STATE_COUNT) pi_ai_mic_ui_set_state(q.ui);
            vdsp_usb_reply r = {q.sequence, vdsp_v2_host_i2s_frames, vdsp_usb_profile_applied};
            rtos_intertile_tx(ctx, VDSP_USB_LINK_PORT, &r, sizeof(r));
        }
        if (buf) rtos_osal_free(buf);
    }
}
void vdsp_v2_runtime_start(void)
{
    configASSERT(xTaskCreate(responder, "USB UI", RTOS_THREAD_STACK_SIZE(responder),
                            NULL, appconfTEST_TASK_PRIORITY, NULL) == pdPASS);
}
#endif
