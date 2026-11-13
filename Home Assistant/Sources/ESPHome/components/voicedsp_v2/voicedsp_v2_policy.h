#ifdef __cplusplus
extern "C" {
#endif
#ifndef VOICEDSP_V2_POLICY_H
#define VOICEDSP_V2_POLICY_H
#include "voicedsp_v2_audio.h"
#include "voicedsp_v2_power.h"
typedef enum {
    VDSP_REASON_REQUEST, VDSP_REASON_JACK, VDSP_REASON_DEBOUNCE,
    VDSP_REASON_IO_UNKNOWN, VDSP_REASON_CLOCK, VDSP_REASON_POWER_UNKNOWN,
    VDSP_REASON_POWER_LOW, VDSP_REASON_PG_LOW, VDSP_REASON_LATCHED
} vdsp_v2_route_reason;
typedef struct {
    uint32_t power_max_age_ms, jack_debounce_ms;
    uint32_t ht326_input_mw, dac_speaker_input_mw;
    uint16_t ht326_min_mv, dac_speaker_min_mv;
} vdsp_v2_policy_limits;
typedef struct {
    vdsp_v2_policy_limits limits;
    vdsp_v2_audio_route requested, last_decision;
    bool initialized, jack_seen, jack_candidate, inhibited;
    uint32_t jack_since_ms;
} vdsp_v2_policy;
typedef struct {
    vdsp_v2_audio_route route; /* Requested decision, NOT a physical output state. */
    vdsp_v2_route_reason reason;
    bool power_approved;
} vdsp_v2_route_decision;

int vdsp_v2_policy_init(vdsp_v2_policy *, const vdsp_v2_policy_limits *);
/* Explicit command clears an inhibit; callers still must reinitialize failed drivers. */
int vdsp_v2_policy_request(vdsp_v2_policy *, vdsp_v2_audio_route);
vdsp_v2_route_decision vdsp_v2_policy_update(vdsp_v2_policy *, const vdsp_v2_sample *,
    const vdsp_v2_power_sample *, bool clocks_ready, uint32_t now_ms);
#endif

#ifdef __cplusplus
}
#endif
