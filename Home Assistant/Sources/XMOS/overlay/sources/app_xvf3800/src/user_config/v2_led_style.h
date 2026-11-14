#pragma once
#include <stdint.h>

/* Called before the timed WS2812 stream, never while clocking pixels. */
static inline void v2_led_style_pixel(unsigned state, unsigned brightness,
                                     const uint8_t color[3], uint8_t *r,
                                     uint8_t *g, uint8_t *b)
{
    const unsigned protected_state = state == 8 || state == 9 || state >= 12;
    if (protected_state && state != 15 && brightness < 32) brightness = 32;
    if (state == 0 || (state >= 4 && state <= 7) || state == 15) {
        unsigned peak = *r > *g ? *r : *g;
        if (*b > peak) peak = *b;
        *r = (uint8_t)((peak * color[0] + 127) / 255);
        *g = (uint8_t)((peak * color[1] + 127) / 255);
        *b = (uint8_t)((peak * color[2] + 127) / 255);
    }
    *r = (uint8_t)((*r * brightness + 127) / 255);
    *g = (uint8_t)((*g * brightness + 127) / 255);
    *b = (uint8_t)((*b * brightness + 127) / 255);
}
