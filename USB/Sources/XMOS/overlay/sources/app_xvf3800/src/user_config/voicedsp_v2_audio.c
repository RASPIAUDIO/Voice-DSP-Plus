#include "voicedsp_v2_audio.h"
#include <stddef.h>

static int write_reg(vdsp_v2_audio *a, uint8_t reg, uint8_t value)
{
    return a->control->write(a->control->context, VDSP_V2_DAC_ADDR, reg, value) ? -1 : 0;
}
static int read_reg(vdsp_v2_audio *a, uint8_t reg, uint8_t *value)
{
    return a->control->read(a->control->context, VDSP_V2_DAC_ADDR, reg, value) ? -1 : 0;
}
static bool usable(const vdsp_v2_audio *a)
{
    return a && a->control && a->control->initialized && a->control->write &&
           a->control->read && a->wait_ms;
}
static int poll(vdsp_v2_audio *a, uint8_t reg, uint8_t mask, uint8_t value, unsigned ms)
{
    uint8_t flags;
    for (unsigned elapsed = 0; ; elapsed += 5) {
        if (read_reg(a, reg, &flags)) return -1;
        if ((flags & mask) == value) return 0;
        if (elapsed >= ms) return -3;
        a->wait_ms(a->wait_context, 5);
    }
}

static int fail(vdsp_v2_audio *a, int error)
{
    /* Best effort, never a claim that disconnected hardware is now off. A page
     * select must ACK before any cleanup register from that page is written.
     */
    if (a->control->initialized)
        (void)vdsp_v2_control_amp(a->control, false, false, false);
    if (!write_reg(a, 0, 0)) (void)write_reg(a, 64, 0x0c);
    if (!write_reg(a, 0, 1)) {
        (void)write_reg(a, 31, 0x16);
        (void)write_reg(a, 32, 0x06);
    }
    a->initialized = false;
    a->muted = true;
    a->route = VDSP_AUDIO_UNKNOWN;
    a->last_error = error;
    return error;
}

static int status_fault(vdsp_v2_audio *a, unsigned reg, unsigned value, unsigned expected)
{
    a->fault_reg = reg;
    a->fault_value = value;
    a->fault_expected = expected;
    return fail(a, -4);
}

int vdsp_v2_audio_init(vdsp_v2_audio *a, const vdsp_v2_audio_config *c)
{
    unsigned j, mdac, dosr;
    uint8_t format;
    int ret;
    if (!usable(a) || !c || !c->clocks_running ||
        (c->sample_rate != 16000 && c->sample_rate != 48000) ||
        (c->word_bits != 16 && c->word_bits != 24 && c->word_bits != 32) ||
        (c->source != VDSP_DIN_PRIMARY && c->source != VDSP_DIN_SECONDARY)) return -2;
    a->initialized = false;
    a->muted = true;
    a->route = VDSP_AUDIO_UNKNOWN;
    a->last_error = 0;
    a->volume_half_db = -60;
    a->external_gain_db = 0;
    a->internal_gain_db = 0;
    if (vdsp_v2_control_amp(a->control, false, false, false)) return fail(a, -1);
    if (write_reg(a, 0, 0) || write_reg(a, 1, 1)) return fail(a, -1);
    a->wait_ms(a->wait_context, 1);
    ret = poll(a, 1, 1, 0, 50);
    if (ret) return fail(a, ret);

    j = c->sample_rate == 16000 ? 24u : 8u;
    mdac = c->sample_rate == 16000 ? 6u : 4u;
    dosr = c->sample_rate == 16000 ? 256u : 128u;
    format = c->word_bits == 16 ? 0 : c->word_bits == 24 ? 0x20 : 0x30;
    /* 64*fs BCLK -> P=1,R=4,J=24/8 -> 98.304 MHz. D MSB then LSB,
     * consecutively as required by the codec. GPIO1 is DIN_SEC, NEVER CLKOUT.
     */
    if (write_reg(a, 4, 0x07) || write_reg(a, 6, (uint8_t)j) ||
        write_reg(a, 7, 0) || write_reg(a, 8, 0) || write_reg(a, 5, 0x94)) return fail(a, -1);
    a->wait_ms(a->wait_context, 10);
    if (write_reg(a, 11, 0x84) || write_reg(a, 12, (uint8_t)(0x80 | mdac)) ||
        write_reg(a, 13, (uint8_t)(dosr >> 8)) || write_reg(a, 14, (uint8_t)dosr) ||
        write_reg(a, 27, format) || write_reg(a, 28, 0) ||
        write_reg(a, 31, 0x24) || write_reg(a, 51, 0x04) ||
        write_reg(a, 32, (uint8_t)c->source) || write_reg(a, 60, 1) ||
        write_reg(a, 63, 0x14) || write_reg(a, 64, 0x0c) ||
        write_reg(a, 65, (uint8_t)a->volume_half_db) ||
        write_reg(a, 66, (uint8_t)a->volume_half_db) || write_reg(a, 116, 0) ||
        write_reg(a, 0, 1) || write_reg(a, 30, 0) || write_reg(a, 31, 0x16) ||
        write_reg(a, 32, 0x06) || write_reg(a, 33, 0x4e) || write_reg(a, 34, 0x70) ||
        write_reg(a, 35, 0x44) || write_reg(a, 36, 0x7f) || write_reg(a, 37, 0x7f) ||
        write_reg(a, 38, 0x7f) || write_reg(a, 39, 0x7f) ||
        write_reg(a, 40, 0x02) || write_reg(a, 41, 0x02) ||
        write_reg(a, 42, 0) || write_reg(a, 43, 0) || write_reg(a, 44, 0x06) ||
        write_reg(a, 0, 0)) return fail(a, -1);
    ret = poll(a, 37, 0xbb, 0, 500);
    if (ret) return fail(a, ret);
    a->initialized = true;
    a->route = VDSP_AUDIO_OFF;
    return 0;
}

int vdsp_v2_audio_stop(vdsp_v2_audio *a)
{
    int ret;
    if (!usable(a) || !a->initialized) return -1;
    if (vdsp_v2_control_amp(a->control, false, false, false) ||
        write_reg(a, 0, 0) || write_reg(a, 64, 0x0c)) return fail(a, -1);
    if (a->route != VDSP_AUDIO_OFF) {
        ret = poll(a, 38, 0x11, 0x11, 200);
        if (ret) return fail(a, ret);
    }
    if (write_reg(a, 0, 1) || write_reg(a, 40, 0x02) || write_reg(a, 41, 0x02) ||
        write_reg(a, 42, 0) || write_reg(a, 43, 0) ||
        write_reg(a, 31, 0x16) || write_reg(a, 32, 0x06) ||
        write_reg(a, 36, 0x7f) || write_reg(a, 37, 0x7f) ||
        write_reg(a, 38, 0x7f) || write_reg(a, 39, 0x7f) || write_reg(a, 0, 0)) return fail(a, -1);
    ret = poll(a, 37, 0x33, 0, 2000);
    if (ret) return fail(a, ret);
    if (write_reg(a, 63, 0x14)) return fail(a, -1);
    ret = poll(a, 37, 0x88, 0, 500);
    if (ret) return fail(a, ret);
    a->route = VDSP_AUDIO_OFF;
    a->muted = true;
    a->last_error = 0;
    return 0;
}

int vdsp_v2_audio_check(vdsp_v2_audio *a)
{
    uint8_t flags, temp, expected;
    vdsp_v2_sample sample;
    int ret;
    if (!usable(a) || !a->initialized) return -1;
    if (write_reg(a, 0, 0) || read_reg(a, 46, &flags)) return fail(a, -1);
    if (flags & 0xc0) return status_fault(a, 46, flags, 0);
    expected = a->route == VDSP_AUDIO_OFF ? 0 :
               a->route == VDSP_AUDIO_DAC_SPEAKERS ? 0x99 : 0xaa;
    if (read_reg(a, 37, &flags)) return fail(a, -1);
    /* A reset/brownout need not set the short-circuit flags. */
    if ((flags & 0xbb) != expected) return status_fault(a, 37, flags, expected);
    if (a->route == VDSP_AUDIO_DAC_SPEAKERS) {
        if (read_reg(a, 3, &temp)) return fail(a, -1);
        if (!(temp & 2)) return status_fault(a, 3, temp, 2);
    }
    if (a->route == VDSP_AUDIO_HT326) {
        ret = vdsp_v2_control_sample(a->control, &sample);
        if (ret) return fail(a, -1);
        if (!sample.amp_sd_n || !(sample.port0 & VDSP_V2_AMP_ENABLE)) return fail(a, -4);
    }
    return 0;
}

int vdsp_v2_audio_select(vdsp_v2_audio *a, vdsp_v2_audio_route route, bool power_approved)
{
    uint8_t outputs;
    int ret;
    if (!usable(a) || !a->initialized) return -1;
    if (route < VDSP_AUDIO_OFF || route > VDSP_AUDIO_DAC_SPEAKERS) return -2;
    ret = vdsp_v2_audio_stop(a);
    if (ret || route == VDSP_AUDIO_OFF) return ret;
    if ((route == VDSP_AUDIO_HT326 || route == VDSP_AUDIO_DAC_SPEAKERS) && !power_approved) {
        a->last_error = -2;
        return -2;
    }
    if (write_reg(a, 0, 1)) return fail(a, -1);
    if (route == VDSP_AUDIO_DAC_SPEAKERS) {
        if (a->internal_gain_db > 6) return fail(a, -2);
        uint8_t pad = 0x80;  /* Unity analog path; level is explicit DAC attenuation. */
        if (write_reg(a, 38, pad) || write_reg(a, 39, pad) ||
            write_reg(a, 42, 0x04) || write_reg(a, 43, 0x04) ||
            write_reg(a, 32, 0xc6)) return fail(a, -1);
        outputs = 0x11;
    } else {
        /* HT326 has its own fixed analog gain. Do not retain the headphone
         * path's -9 dB attenuation before that amplifier (P1 R36/R37). */
        uint8_t attenuation = route == VDSP_AUDIO_HT326 ? 0x80 : 0x92;
        if (a->external_gain_db > 9) return fail(a, -2);
        uint8_t pga = (uint8_t)(0x06 | (route == VDSP_AUDIO_HT326 ? a->external_gain_db << 3 : 0));
        if (write_reg(a, 36, attenuation) || write_reg(a, 37, attenuation) ||
            write_reg(a, 40, pga) || write_reg(a, 41, pga) ||
            write_reg(a, 31, 0xd6)) return fail(a, -1);
        outputs = 0x22;
    }
    if (write_reg(a, 0, 0)) return fail(a, -1);
    ret = poll(a, 37, 0x33, outputs, 2000);
    if (ret) return fail(a, ret);
    if (write_reg(a, 63, 0xd4)) return fail(a, -1);
    ret = poll(a, 37, 0xbb, (uint8_t)(outputs | 0x88), 500);
    if (ret) return fail(a, ret);
    a->route = route;
    if (route == VDSP_AUDIO_HT326) {
        if (vdsp_v2_control_amp(a->control, true, true, power_approved)) return fail(a, -1);
        /* Engineering settling allowance, to qualify with the assembled amp. */
        a->wait_ms(a->wait_context, 100);
    }
    ret = vdsp_v2_audio_check(a);
    if (ret) return ret;
    return 0; /* USB owner explicitly unmutes after the complete transaction. */
}

int vdsp_v2_audio_mute(vdsp_v2_audio *a, bool mute)
{
    int ret;
    if (!usable(a) || !a->initialized) return -1;
    if (!mute && a->route == VDSP_AUDIO_OFF) return -2;
    if (!mute) {
        ret = vdsp_v2_audio_check(a);
        if (ret) return ret;
    }
    if (write_reg(a, 0, 0) || write_reg(a, 64, mute ? 0x0c : 0)) return fail(a, -1);
    if (a->route != VDSP_AUDIO_OFF) {
        ret = poll(a, 38, 0x11, 0x11, 200);
        if (ret) return fail(a, ret);
    }
    a->muted = mute;
    return 0;
}

int vdsp_v2_audio_volume(vdsp_v2_audio *a, int half_db)
{
    bool restore;
    int ret;
    if (!usable(a) || !a->initialized) return -1;
    if (half_db < -127 || half_db > 0) return -2;
    restore = !a->muted;
    ret = vdsp_v2_audio_mute(a, true);
    if (ret) return ret;
    if (write_reg(a, 0, 0) || write_reg(a, 65, (uint8_t)half_db) ||
        write_reg(a, 66, (uint8_t)half_db)) return fail(a, -1);
    a->volume_half_db = half_db;
    return restore ? vdsp_v2_audio_mute(a, false) : 0;
}

/* Appended to the preserved audio driver only in the .5 candidate. Reuses its
 * static DAC helpers. The .4 source and artifacts remain byte-for-byte intact.
 */
#include "voicedsp_v2_guard.h"
static int guarded_poll(vdsp_v2_audio *a, uint8_t mask, uint8_t value, unsigned ms,
    vdsp_v2_audio_route route, VDSP_FPTR("vdsp_authorize") vdsp_authorize_fn authorize, void *context)
{
    for (unsigned elapsed = 0; ; elapsed += 5) {
        uint8_t flags;
        if (!authorize(context, route)) return -5;
        if (read_reg(a, 37, &flags)) return -1;
        if ((flags & mask) == value) return 0;
        if (elapsed >= ms) return -3;
        a->wait_ms(a->wait_context, 5);
    }
}
int vdsp_v2_audio_guarded_select(vdsp_v2_audio *a, vdsp_v2_audio_route route,
    VDSP_FPTR("vdsp_authorize") vdsp_authorize_fn authorize, void *context)
{
    uint8_t outputs;
    int ret;
    if (!usable(a) || !a->initialized || !authorize) return -1;
    if (route < VDSP_AUDIO_OFF || route > VDSP_AUDIO_DAC_SPEAKERS) return -2;
    ret = vdsp_v2_audio_stop(a);
    if (ret || route == VDSP_AUDIO_OFF) return ret;
    if (!authorize(context, route)) return fail(a, -5);
    if (write_reg(a, 0, 1)) return fail(a, -1);
    if (route == VDSP_AUDIO_DAC_SPEAKERS) {
        if (a->internal_gain_db > 6) return fail(a, -2);
        uint8_t pad = 0x80;  /* Unity analog path; level is explicit DAC attenuation. */
        if (write_reg(a, 38, pad) || write_reg(a, 39, pad) ||
            write_reg(a, 42, 0x04) || write_reg(a, 43, 0x04) ||
            write_reg(a, 32, 0xc6)) return fail(a, -1);
        outputs = 0x11;
    } else {
        if (a->external_gain_db > 9) return fail(a, -2);
        uint8_t attenuation = route == VDSP_AUDIO_HT326 ? 0x80 : 0x92;
        uint8_t pga = (uint8_t)(0x06 | (route == VDSP_AUDIO_HT326 ? a->external_gain_db << 3 : 0));
        if (write_reg(a, 36, attenuation) || write_reg(a, 37, attenuation) ||
            write_reg(a, 40, pga) || write_reg(a, 41, pga) ||
            write_reg(a, 31, 0xd6)) return fail(a, -1);
        uint8_t left, right;
        if (read_reg(a, 40, &left) || read_reg(a, 41, &right)) return fail(a, -1);
        if ((left & 0x78) != (pga & 0x78) || (right & 0x78) != (pga & 0x78)) return fail(a, -4);
        outputs = 0x22;
    }
    if (write_reg(a, 0, 0)) return fail(a, -1);
    ret = guarded_poll(a, 0x33, outputs, 2000, route, authorize, context);
    if (ret) return fail(a, ret);
    if (write_reg(a, 63, 0xd4)) return fail(a, -1);
    ret = guarded_poll(a, 0xbb, (uint8_t)(outputs | 0x88), 500, route, authorize, context);
    if (ret) return fail(a, ret);
    a->route = route;
    if (route == VDSP_AUDIO_HT326) {
        if (!authorize(context, route)) return fail(a, -5);
        if (vdsp_v2_control_amp(a->control, true, true, true)) return fail(a, -1);
        a->wait_ms(a->wait_context, 100);
    }
    if (!authorize(context, route)) return fail(a, -5);
    ret = vdsp_v2_audio_check(a);
    if (ret) return ret;
    return 0; /* USB owner explicitly unmutes after the complete transaction. */
}
