#include "voicedsp_v2_buttons.h"
#include <string.h>

static void cancel(vdsp_v2_buttons *s, vdsp_v2_button_events *e)
{
    for (unsigned i = 0; i < 4; ++i)
        if (s->button[i].stable) e->cancelled |= (uint8_t)(1u << i);
    memset(s, 0, sizeof(*s));
}

vdsp_v2_button_events vdsp_v2_buttons_update(vdsp_v2_buttons *s, uint32_t now,
                                           uint32_t sampled, bool valid, uint8_t mask)
{
    vdsp_v2_button_events e = {0};
    if (!valid || (uint32_t)(now - sampled) > VDSP_BUTTON_MAX_GAP_MS) {
        cancel(s, &e);
        return e;
    }
    if (s->available && (uint32_t)(sampled - s->last_sample_ms) > VDSP_BUTTON_MAX_GAP_MS)
        cancel(s, &e);
    e.available = true;
    if (!s->available) {
        s->available = true;
        s->last_sample_ms = sampled;
        for (unsigned i = 0; i < 4; ++i) {
            s->button[i].candidate = (mask & (1u << i)) != 0;
            s->button[i].changed_ms = sampled;
        }
        return e;
    }
    if (sampled == s->last_sample_ms) return e;
    s->last_sample_ms = sampled;
    for (unsigned i = 0; i < 4; ++i) {
        vdsp_v2_button *b = &s->button[i];
        uint8_t bit = (uint8_t)(1u << i);
        bool down = (mask & bit) != 0;
        if (down != b->candidate) {
            b->candidate = down;
            b->changed_ms = sampled;
        }
        if (!b->armed) {
            if (!down && (uint32_t)(sampled - b->changed_ms) >= VDSP_BUTTON_DEBOUNCE_MS)
                b->armed = true;
            continue;
        }
        if (b->candidate != b->stable &&
            (uint32_t)(sampled - b->changed_ms) >= VDSP_BUTTON_DEBOUNCE_MS) {
            b->stable = b->candidate;
            if (b->stable) {
                b->pressed_ms = sampled;
                b->long_sent = false;
                e.pressed |= bit;
            } else {
                e.released |= bit;
                if (!b->long_sent) e.short_press |= bit;
            }
        }
        /* A pending release must not create a long press during debounce. */
        if (b->stable && b->candidate && !b->long_sent &&
            (uint32_t)(sampled - b->pressed_ms) >= VDSP_BUTTON_LONG_MS) {
            b->long_sent = true;
            e.long_press |= bit;
        }
    }
    return e;
}
