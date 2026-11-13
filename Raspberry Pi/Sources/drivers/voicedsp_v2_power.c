#include "voicedsp_v2_power.h"

static const uint16_t currents[] = {500,700,1000,1250,1500,1750,2000,2250,
                                   2500,2750,3000,3250,3500,4000,4500,5000};
static const uint16_t voltages[] = {0,5000,9000,12000,15000,18000,20000};

int vdsp_v2_power_decode(uint8_t s0, uint8_t s1, uint32_t now, vdsp_v2_power_sample *s)
{
    unsigned voltage = s0 >> 4, response = (s1 >> 3) & 7;
    if (!s) return -1;
    *s = (vdsp_v2_power_sample){.valid = false};
    s->raw_status0 = s0;
    s->raw_status1 = s1;
    s->response = (uint8_t)response;
    if (voltage > 6 || response == 2 || response > 5) return -2;
    s->valid = true;
    s->captured_ms = now;
    s->attached = (s1 & 0x40) != 0;
    s->cc2 = s->attached && (s1 & 0x80) != 0;
    s->kind = s->attached ? VDSP_POWER_ATTACHED_UNKNOWN : VDSP_POWER_UNATTACHED;
    if (!s->attached) return 0; /* Never retain a stale contract after detach. */
    if (voltage) {
        s->kind = VDSP_POWER_PD;
        s->contract_mv = voltages[voltage];
        s->contract_ma = currents[s0 & 15];
        s->contract_mw = (uint32_t)s->contract_mv * s->contract_ma / 1000;
    } else if (s1 & 4) {
        static const uint16_t legacy[] = {0,1500,2400,3000};
        s->kind = VDSP_POWER_5V;
        s->legacy_ma = legacy[s1 & 3];
        s->legacy_current_known = (s1 & 3) != 0;
        /* Default current is unspecified here, not automatically 500 or 900 mA. */
    }
    return 0;
}

static int status(vdsp_v2_power *d, uint8_t *s0, uint8_t *s1)
{
    return d->read(d->context, VDSP_V2_HUSB_ADDR, 0, s0) ||
           d->read(d->context, VDSP_V2_HUSB_ADDR, 1, s1) ? -1 : 0;
}

int vdsp_v2_power_poll(vdsp_v2_power *d, uint32_t now)
{
    uint8_t a0, a1, b0, b1;
    int ret = -1;
    if (!d) return -1;
    d->current = (vdsp_v2_power_sample){.valid = false};
    if (d->read && !status(d, &a0, &a1) && !status(d, &b0, &b1)) {
        ret = a0 == b0 && a1 == b1 ? vdsp_v2_power_decode(a0, a1, now, &d->current) : -3;
    }
    d->last_error = ret;
    if (!ret) d->last_good = d->current;
    else if (d->errors < UINT32_MAX) ++d->errors;
    return ret;
}

int vdsp_v2_power_read_caps(vdsp_v2_power *d, vdsp_v2_power_caps *caps)
{
    uint8_t a0, a1, b0, b1, raw[6];
    vdsp_v2_power_sample sample;
    if (!caps) return -1;
    *caps = (vdsp_v2_power_caps){.valid = false};
    if (!d || !d->read || status(d, &a0, &a1)) return -1;
    for (unsigned i = 0; i < 6; ++i)
        if (d->read(d->context, VDSP_V2_HUSB_ADDR, (uint8_t)(i + 2), &raw[i])) return -1;
    if (status(d, &b0, &b1)) return -1;
    if (a0 != b0 || a1 != b1) return -3;
    if (vdsp_v2_power_decode(a0, a1, 0, &sample)) return -2;
    if (!sample.attached) return -3;
    for (unsigned i = 0; i < 6; ++i) if (raw[i] & 0x70) return -2;
    for (unsigned i = 0; i < 6; ++i) {
        if (raw[i] & 0x80) {
            caps->present_mask |= (uint8_t)(1u << i);
            caps->current_ma[i] = currents[raw[i] & 15];
        }
    }
    caps->valid = true;
    return 0;
}

bool vdsp_v2_power_fresh(const vdsp_v2_power_sample *s, uint32_t now, uint32_t max_age)
{
    return s && s->valid && max_age < UINT32_MAX / 2 &&
           (uint32_t)(now - s->captured_ms) <= max_age;
}
