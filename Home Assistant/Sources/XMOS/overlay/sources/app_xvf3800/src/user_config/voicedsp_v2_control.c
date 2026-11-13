#include "voicedsp_v2_control.h"
#include <stddef.h>

vdsp_v2_mic_array vdsp_v2_decode_microphones(bool valid, uint8_t p0, uint8_t p1)
{
    if (!valid) return VDSP_MIC_UNKNOWN;
    if (p1 & 0x10u) return VDSP_MIC_INTERNAL_SQUARE;
    return (p0 & 0x40u) ? VDSP_MIC_EXTERNAL_SQUARE : VDSP_MIC_EXTERNAL_LINEAR;
}

int vdsp_v2_control_init(vdsp_v2_control *d)
{
    /* Release shared/reset lines before changing latches, even after an older
     * firmware left output directions behind. P0.4 has a physical pull-down.
     * Set safe latches before re-enabling only the amp output. No unsolicited IRQ.
     */
    static const uint8_t setup[][2] = {
        {0x06, 0xff}, {0x07, 0xff},
        {0x02, 0x00}, {0x03, 0x00}, {0x4f, 0x00},
        {0x04, 0x00}, {0x05, 0x00},
        {0x44, 0x00}, {0x45, 0x00},
        {0x48, 0x00}, {0x49, 0x0f},
        {0x46, 0x00}, {0x47, 0x0f},
        {0x4a, 0xff}, {0x4b, 0xff},
        {0x06, 0xef}
    };
    unsigned i;
    if (!d || !d->write || !d->read) return -1;
    d->initialized = false;
    d->amp_enabled = false;
    for (i = 0; i < sizeof(setup) / sizeof(setup[0]); ++i) {
        if (d->write(d->context, VDSP_V2_PCAL_ADDR, setup[i][0], setup[i][1])) return -1;
    }
    d->initialized = true;
    return 0;
}

int vdsp_v2_control_sample(vdsp_v2_control *d, vdsp_v2_sample *s)
{
    uint8_t p0, p1;
    if (!s) return -1;
    *s = (vdsp_v2_sample){0};
    if (!d || !d->initialized || !d->read) return -1;
    if (d->read(d->context, VDSP_V2_PCAL_ADDR, 0x00, &p0) ||
        d->read(d->context, VDSP_V2_PCAL_ADDR, 0x01, &p1)) return -1;
    s->valid = true;
    s->port0 = p0;
    s->port1 = p1;
    s->microphone = vdsp_v2_decode_microphones(true, p0, p1);
    s->buck_pg = (p1 & 0x80u) != 0;
    s->amp_sd_n = (p0 & VDSP_V2_AMP_SD_N) != 0;
    s->buttons_pressed = (uint8_t)(~p1 & 0x0fu);
    s->gesture_irq = (p1 & 0x20u) == 0;
    return 0;
}

int vdsp_v2_control_amp(vdsp_v2_control *d, bool enable, bool audio_ready, bool power_approved)
{
    if (!d || !d->initialized || !d->write) return -1;
    if (enable && (!audio_ready || !power_approved)) return -2;
    if (d->write(d->context, VDSP_V2_PCAL_ADDR, 0x02, enable ? VDSP_V2_AMP_ENABLE : 0)) {
        /* Communication failure makes the actual output state unknown.
         * Invalidate the whole device rather than claim a successful shutdown.
         */
        d->initialized = false;
        return -1;
    }
    d->amp_enabled = enable;
    return 0;
}
