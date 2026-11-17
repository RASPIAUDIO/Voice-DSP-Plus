#ifndef VOICEDSP_V2_BUTTONS_H
#define VOICEDSP_V2_BUTTONS_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { VDSP_BUTTON_DEBOUNCE_MS = 30, VDSP_BUTTON_LONG_MS = 800,
       VDSP_BUTTON_MAX_GAP_MS = 250 };
typedef struct {
    uint32_t changed_ms, pressed_ms;
    bool candidate, stable, armed, long_sent;
} vdsp_v2_button;
typedef struct {
    vdsp_v2_button button[4]; /* P1.0 up, P1.1 down, P1.2 left, P1.3 right. */
    uint32_t last_sample_ms;
    bool available;
} vdsp_v2_buttons;
typedef struct {
    uint8_t pressed, released, short_press, long_press, cancelled;
    bool available;
} vdsp_v2_button_events;

/* Zero-initialize state. mask is active-high, already inverted from PCAL.
 * sampled_ms is the start of the successful PCAL read, NOT publish time.
 * Only fresh samples advance the filter. Missing data cancels, never clicks.
 */
vdsp_v2_button_events vdsp_v2_buttons_update(vdsp_v2_buttons *, uint32_t now_ms,
                                           uint32_t sampled_ms, bool valid,
                                           uint8_t mask);
#ifdef __cplusplus
}
#endif
#endif
