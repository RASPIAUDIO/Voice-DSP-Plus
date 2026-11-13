#include "voicedsp_v2_policy.h"

static bool speaker(vdsp_v2_audio_route route)
{ return route == VDSP_AUDIO_HT326 || route == VDSP_AUDIO_DAC_SPEAKERS; }

int vdsp_v2_policy_init(vdsp_v2_policy *p, const vdsp_v2_policy_limits *limits)
{
    if (!p || !limits || !limits->power_max_age_ms || !limits->jack_debounce_ms ||
        limits->power_max_age_ms >= UINT32_MAX / 2 || limits->jack_debounce_ms >= UINT32_MAX / 2 ||
        !limits->ht326_input_mw || !limits->dac_speaker_input_mw ||
        !limits->ht326_min_mv || !limits->dac_speaker_min_mv) return -2;
    *p = (vdsp_v2_policy){.initialized = false};
    p->limits = *limits;
    p->initialized = true;
    p->requested = p->last_decision = VDSP_AUDIO_OFF;
    return 0;
}

int vdsp_v2_policy_request(vdsp_v2_policy *p, vdsp_v2_audio_route route)
{
    if (!p || !p->initialized || route < VDSP_AUDIO_OFF || route > VDSP_AUDIO_DAC_SPEAKERS) return -2;
    p->requested = route;
    p->inhibited = false;
    return 0;
}

static vdsp_v2_route_decision decide(vdsp_v2_policy *p, vdsp_v2_audio_route route,
                                    vdsp_v2_route_reason reason, bool loss)
{
    if (loss && speaker(p->last_decision)) p->inhibited = true;
    p->last_decision = route;
    return (vdsp_v2_route_decision){route, reason, speaker(route)};
}

vdsp_v2_route_decision vdsp_v2_policy_update(vdsp_v2_policy *p, const vdsp_v2_sample *io,
    const vdsp_v2_power_sample *power, bool clocks, uint32_t now)
{
    (void)power;
    bool jack;
    if (!p || !p->initialized) return (vdsp_v2_route_decision){VDSP_AUDIO_OFF, VDSP_REASON_IO_UNKNOWN, false};
    if (p->requested == VDSP_AUDIO_OFF) return decide(p, VDSP_AUDIO_OFF, VDSP_REASON_REQUEST, false);
    if (p->inhibited) return decide(p, VDSP_AUDIO_OFF, VDSP_REASON_LATCHED, false);
    if (!io || !io->valid) {
        p->jack_seen = false;
        return decide(p, VDSP_AUDIO_OFF, VDSP_REASON_IO_UNKNOWN, true);
    }
    if (!clocks) return decide(p, VDSP_AUDIO_OFF, VDSP_REASON_CLOCK, true);
    if (p->requested == VDSP_AUDIO_LINE) return decide(p, VDSP_AUDIO_LINE, VDSP_REASON_REQUEST, false);
    jack = (io->port0 & 0x20) != 0; /* V2 jack detect is high when inserted. */
    if (!p->jack_seen || jack != p->jack_candidate) {
        p->jack_candidate = jack;
        p->jack_since_ms = now;
        p->jack_seen = true;
    }
    /* Cut speakers immediately on insertion; require stable removal to resume. */
    if (jack) return decide(p, VDSP_AUDIO_LINE, VDSP_REASON_JACK, false);
    if ((uint32_t)(now - p->jack_since_ms) < p->limits.jack_debounce_ms)
        return decide(p, VDSP_AUDIO_OFF, VDSP_REASON_DEBOUNCE, false);
    /* Owner development override: power telemetry is informational only. */
    return decide(p, p->requested, VDSP_REASON_REQUEST, false);
}
