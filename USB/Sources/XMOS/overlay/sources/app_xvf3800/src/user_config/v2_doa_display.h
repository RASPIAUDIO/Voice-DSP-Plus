#ifndef V2_DOA_DISPLAY_H
#define V2_DOA_DISPLAY_H
#include <math.h>
#include <stdint.h>

/* Display only: microphone coordinates and BeClear steering stay unchanged. */
typedef struct {
    float angle, candidate;
    uint32_t last_ms, valid_ms, candidate_ms;
    int initialized, pending, led;
} v2_doa_display_t;

static inline float v2_doa_wrap(float a) {
    const float tau = 6.28318530718f;
    a = fmodf(a, tau);
    return a < 0 ? a + tau : a;
}
static inline float v2_doa_delta(float a, float b) {
    return v2_doa_wrap(a - b + 3.14159265359f) - 3.14159265359f;
}
static inline float v2_doa_pcb_angle(float old_angle) {
    return v2_doa_wrap(1.57079632679f - old_angle);
}

/* 500 ms circular smoothing, 300 ms large-jump confirmation, 1 s hold.
 * Reject isolated jumps instead of animating a sweep to a false source.
 * A 3 degree hysteresis keeps a source on a LED boundary from flickering.
 */
static inline int v2_doa_display_step(v2_doa_display_t *s, float old_angle,
                                     float energy, uint32_t now, int count) {
    const float pi = 3.14159265359f;
    const float tau = 2.0f * pi;
    uint32_t dt = now - s->last_ms;
    s->last_ms = now;
    if (dt > 200) dt = 200;
    if (count <= 0) return -1;
    if (isfinite(old_angle) && isfinite(energy) && energy > 5000.0f) {
        float a = v2_doa_pcb_angle(old_angle);
        s->valid_ms = now;
        if (!s->initialized) {
            s->angle = a;
            s->initialized = 1;
            s->pending = 0;
            s->led = -1;
        } else {
            float d = v2_doa_delta(a, s->angle);
            if (fabsf(d) > pi / 4.0f) {
                if (!s->pending || fabsf(v2_doa_delta(a, s->candidate)) > pi / 9.0f) {
                    s->candidate = a;
                    s->candidate_ms = now;
                    s->pending = 1;
                }
                if (now - s->candidate_ms < 300) return s->led;
            } else {
                s->pending = 0;
            }
            s->angle = v2_doa_wrap(s->angle + d * ((float)dt / (500.0f + dt)));
        }
    } else {
        s->pending = 0;
        if (!s->initialized || now - s->valid_ms > 1000) {
            s->initialized = 0;
            s->led = -1;
            return -1;
        }
    }
    if (s->led < 0 || fabsf(v2_doa_delta(s->angle, s->led * tau / count)) >
            tau / (2.0f * count) + pi / 60.0f) {
        s->led = ((int)(s->angle * count / tau + 0.5f)) % count;
    }
    return s->led;
}
#endif
