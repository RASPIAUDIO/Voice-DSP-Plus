#ifndef VDSP_USB_STATE_H
#define VDSP_USB_STATE_H
#include <stdint.h>
#include <stdbool.h>
typedef struct {
    bool muted, have_transition;
    uint32_t transition_ms;
    unsigned attenuation;
} vdsp_usb_gestures;
/* now must be sampled after the sensor step, not at loop entry. */
static inline bool vdsp_usb_gesture_fresh(uint32_t now, uint32_t event_ms)
{ return (uint32_t)(now - event_ms) <= 250u; }
/* PCAL P1.0=UP, P1.1=DOWN. Keep mute and optional crossover selection. */
static inline bool vdsp_usb_buttons_apply(uint8_t pressed, uint8_t short_press,
                                        uint8_t long_press, uint8_t command[4])
{
    uint8_t old_level = command[2], old_route = command[1];
    if (pressed & 1) command[2] = command[2] >= 6 ? command[2] - 6 : 0;
    if (long_press & 2) command[1] = command[1] == 3 ? 4 : 3;
    else if (short_press & 2) command[2] = command[2] <= 114 ? command[2] + 6 : 120;
    return old_level != command[2] || old_route != command[1];
}
/* Return 1 mute transition, 2 level change, 0 ignored. Unsigned subtraction
 * intentionally handles the millisecond counter wrapping. */
static inline int vdsp_usb_gesture(vdsp_usb_gestures *s, uint16_t raw, uint32_t now)
{
    if (!(raw == 1 || raw == 2 || raw == 4 || raw == 8 || raw == 16 || raw == 32)) return 0;
    if (s->have_transition && (uint32_t)(now - s->transition_ms) < 1000u) return 0;
    if (s->muted || raw == 16) {
        s->muted = !s->muted;
        s->transition_ms = now;
        s->have_transition = true;
        return 1;
    }
    if (raw == 2) {
        s->attenuation = s->attenuation >= 6 ? s->attenuation - 6 : 0;
        return 2;
    }
    if (raw == 1) {
        s->attenuation = s->attenuation <= 114 ? s->attenuation + 6 : 120;
        return 2;
    }
    return 0;
}
#endif
