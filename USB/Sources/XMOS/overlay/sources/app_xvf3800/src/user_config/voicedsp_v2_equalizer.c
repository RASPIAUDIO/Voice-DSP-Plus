#include "app_conf.h"
#include "rtos_macros.h"
#if ON_TILE(1)
#include "voicedsp_v2_equalizer.h"
#include <math.h>
#include <stddef.h>

#define Q15_MAX (32767.0f / 32768.0f)

typedef struct { float n0, n1, n2, d1, d2, scale; } biquad;

static biquad identity(void)
{
    return (biquad){Q15_MAX, 0, 0, 0, 0, 1};
}

/* Same shelf equations, numerator normalization and TI sign/half conventions
 * as archive/.../voice_dsp_plus.cpp, shelf_biquad/encode_biquad. */
static biquad shelf(float gain, float frequency, bool high)
{
    float a, omega, cosine, alpha, t, b0, b1, b2, a0, a1, a2, peak;
    biquad result;
    if (fabsf(gain) < 0.01f) return identity();
    a = powf(10.0f, gain / 40.0f);
    omega = 2.0f * 3.14159265358979323846f * frequency / 48000.0f;
    cosine = cosf(omega);
    alpha = sinf(omega) * sqrtf(2.0f) / 2.0f;
    t = 2.0f * sqrtf(a) * alpha;
    if (high) {
        b0 = a * ((a + 1) + (a - 1) * cosine + t);
        b1 = -2 * a * ((a - 1) + (a + 1) * cosine);
        b2 = a * ((a + 1) + (a - 1) * cosine - t);
        a0 = (a + 1) - (a - 1) * cosine + t;
        a1 = 2 * ((a - 1) - (a + 1) * cosine);
        a2 = (a + 1) - (a - 1) * cosine - t;
    } else {
        b0 = a * ((a + 1) - (a - 1) * cosine + t);
        b1 = 2 * a * ((a - 1) - (a + 1) * cosine);
        b2 = a * ((a + 1) - (a - 1) * cosine - t);
        a0 = (a + 1) + (a - 1) * cosine + t;
        a1 = -2 * ((a - 1) + (a + 1) * cosine);
        a2 = (a + 1) + (a - 1) * cosine - t;
    }
    result = (biquad){b0 / a0, b1 / (2 * a0), b2 / a0,
                      -a1 / (2 * a0), -a2 / a0, 1};
    peak = fmaxf(fabsf(result.n0), fmaxf(fabsf(result.n1), fabsf(result.n2)));
    if (peak > Q15_MAX) {
        result.scale = Q15_MAX / peak;
        result.n0 *= result.scale;
        result.n1 *= result.scale;
        result.n2 *= result.scale;
    }
    return result;
}

static void encode(biquad b, uint8_t *output)
{
    const float values[5] = {b.n0, b.n1, b.n2, b.d1, b.d2};
    for (unsigned i = 0; i < 5; ++i) {
        float clipped = fmaxf(-1.0f, fminf(Q15_MAX, values[i]));
        uint16_t value = (uint16_t)(int16_t)lroundf(clipped * 32768.0f);
        output[2 * i] = (uint8_t)(value >> 8);
        output[2 * i + 1] = (uint8_t)value;
    }
}

int vdsp_v2_equalizer_generate(float bass_db, float treble_db,
                             vdsp_v2_equalizer_coefficients *output)
{
    biquad filters[3];
    float attenuation, headroom, gain;
    if (!output || !isfinite(bass_db) || !isfinite(treble_db) ||
        bass_db < -6 || bass_db > 6 || treble_db < -6 || treble_db > 6) return -2;
    filters[0] = shelf(bass_db, 120, false);
    filters[1] = shelf(treble_db, 6000, true);
    filters[2] = identity();
    attenuation = 20.0f * log10f(filters[0].scale * filters[1].scale);
    headroom = fmaxf(0, fmaxf(bass_db, treble_db));
    gain = fmaxf(-24, fminf(12, -attenuation - headroom));
    for (unsigned i = 0; i < 3; ++i) encode(filters[i], output->coefficients[i]);
    output->gain_half_db = (int)floorf(gain * 2);
    return 0;
}

int vdsp_v2_equalizer_prepare_pair(float bass_left, float treble_left,
                                 float bass_right, float treble_right,
                                 int base_volume_half_db,
                                 vdsp_v2_equalizer_pair *output)
{
    vdsp_v2_equalizer_pair pair = {0};
    const float bass[2] = {bass_left, bass_right};
    const float treble[2] = {treble_left, treble_right};
    if (!output || base_volume_half_db < -127 || base_volume_half_db > 0) return -2;
    for (unsigned ch = 0; ch < 2; ++ch) {
        if (vdsp_v2_equalizer_generate(bass[ch], treble[ch], &pair.channels[ch])) return -2;
        const bool shaped = fabsf(bass[ch]) >= 0.01f || fabsf(treble[ch]) >= 0.01f;
        int volume = base_volume_half_db + pair.channels[ch].gain_half_db - (shaped ? 4 : 0);
        if (volume < -127) volume = -127;
        if (volume > 0) volume = 0;
        pair.volume_half_db[ch] = volume;
    }
    *output = pair;
    return 0;
}

/* For x=cos(w), |den|^2-|num|^2 is quadratic. Check its exact minimum
 * on [-1,1], not a frequency grid, after rounding the feedback coefficients.
 * Units are Q15 integers represented as double (no integer overflow). */
static bool crossover_unity_bound(int numerator, int feedback1, int feedback2,
                                  bool high)
{
    const double q = 32768.0, a1 = -2.0 * feedback1, a2 = -feedback2;
    const double n = 4.0 * numerator * numerator;
    const double c0 = (q - a2) * (q - a2) + a1 * a1 - n;
    const double c1 = 2.0 * a1 * (q + a2) + (high ? 2.0 : -2.0) * n;
    const double c2 = 4.0 * q * a2 - n;
    if (c0 + c1 + c2 < 0 || c0 - c1 + c2 < 0) return false;
    if (c2 > 0 && fabs(c1) < 2.0 * c2 && c0 - c1 * c1 / (4.0 * c2) < 0)
        return false;
    return true;
}

int vdsp_v2_crossover_prepare(const vdsp_v2_crossover_config *config,
                            int base_volume_half_db,
                            vdsp_v2_equalizer_pair *output)
{
    vdsp_v2_equalizer_pair pair = {0};
    double omega, cosine, alpha, a0;
    int d1, d2;
    if (!config || !output || base_volume_half_db < -127 || base_volume_half_db > 0 ||
        !isfinite(config->cutoff_hz) || config->cutoff_hz < VDSP_V2_CROSSOVER_MIN_HZ ||
        config->cutoff_hz > VDSP_V2_CROSSOVER_MAX_HZ ||
        !isfinite(config->left_trim_db) || !isfinite(config->right_trim_db) ||
        config->left_trim_db < -24 || config->left_trim_db > 0 ||
        config->right_trim_db < -24 || config->right_trim_db > 0 ||
        (config->mute_mask & ~3u)) return -2;

    omega = 2.0 * 3.14159265358979323846 * config->cutoff_hz / VDSP_V2_EQ_SAMPLE_RATE;
    cosine = cos(omega);
    alpha = sin(omega) / sqrt(2.0); /* Butterworth Q = 1/sqrt(2). */
    a0 = 1.0 + alpha;
    d1 = (int)lround(32768.0 * cosine / a0);
    d2 = (int)lround(-32768.0 * (1.0 - alpha) / a0);
    /* Strict Jury conditions for the actual quantized denominator. */
    if (32768 - 2 * d1 - d2 <= 0 || 32768 + 2 * d1 - d2 <= 0 ||
        32768 + d2 <= 0) return -2;

    for (unsigned ch = 0; ch < 2; ++ch) {
        const bool high = ch == 1;
        const double trim = high ? config->right_trim_db : config->left_trim_db;
        int n = (int)floor(32768.0 * (1.0 + (high ? cosine : -cosine)) / (2.0 * a0));
        /* Preserve the double DC/Nyquist zero with exactly tied numerator
         * integers. Never compensate a quantization loss with digital gain. */
        while (n > 0 && !crossover_unity_bound(n, d1, d2, high)) --n;
        n = (int)floor(n * pow(10.0, trim / 40.0)); /* Half the trim per section. */
        if (config->mute_mask & (1u << ch)) n = 0;
        const biquad section = {(float)n / 32768.0f,
            (float)(high ? -n : n) / 32768.0f, (float)n / 32768.0f,
            (float)d1 / 32768.0f, (float)d2 / 32768.0f, 1};
        encode(section, pair.channels[ch].coefficients[0]);
        encode(section, pair.channels[ch].coefficients[1]);
        encode(identity(), pair.channels[ch].coefficients[2]);
        pair.volume_half_db[ch] = base_volume_half_db;
    }
    *output = pair;
    return 0;
}

int vdsp_v2_crossover_generate(const vdsp_v2_crossover_config *config,
                             vdsp_v2_equalizer_pair *output)
{
    return vdsp_v2_crossover_prepare(config, 0, output);
}

#else
#include "voicedsp_v2_equalizer.h"
#include <stddef.h>
#include "vdsp_usb_parametric_eq.inc"
static int wr(vdsp_v2_audio *a, uint8_t reg, uint8_t value)
{
    return a->control->write(a->control->context, VDSP_V2_DAC_ADDR, reg, value) ? -1 : 0;
}

static int rd(vdsp_v2_audio *a, uint8_t reg, uint8_t *value)
{
    return a->control->read(a->control->context, VDSP_V2_DAC_ADDR, reg, value) ? -1 : 0;
}

static int fail(vdsp_v2_audio *a, int error)
{
    /* Same evidenced shutdown registers as audio.c. Never write a page's
     * registers after a failed page select, and never claim physical OFF. */
    if (a->control->initialized)
        (void)vdsp_v2_control_amp(a->control, false, false, false);
    if (!wr(a, 0, 0)) (void)wr(a, 64, 0x0c);
    if (!wr(a, 0, 1)) {
        (void)wr(a, 31, 0x16);
        (void)wr(a, 32, 0x06);
    }
    (void)wr(a, 0, 0);
    a->initialized = false;
    a->muted = true;
    a->route = VDSP_AUDIO_UNKNOWN;
    a->last_error = error;
    return error;
}

static int write_bank(vdsp_v2_audio *a, uint8_t page,
                      const vdsp_v2_equalizer_coefficients *eq)
{
    if (wr(a, 0, page)) return -1;
    for (unsigned channel = 0; channel < 2; ++channel) {
        for (unsigned filter = 0; filter < 3; ++filter) {
            unsigned start = 2 + 64 * channel + 10 * filter;
            /* TI SLAS666B 6.4.2.4: MSB immediately followed by LSB. No
             * intervening page select, read, wait or other bus user. */
            for (unsigned byte = 0; byte < 10; ++byte)
                if (wr(a, (uint8_t)(start + byte), eq[channel].coefficients[filter][byte])) return -1;
        }
    }
    return 0;
}

static int apply_prepared(vdsp_v2_audio *a, const vdsp_v2_equalizer_pair *pair)
{
    /* Read-only signature of audio_init's 48 kHz configuration. Never change
     * the processing block or clocks to make an incompatible caller work. */
    static const uint8_t signature[][2] = {
        {4, 0x07}, {5, 0x94}, {6, 8}, {7, 0}, {8, 0},
        {11, 0x84}, {12, 0x84}, {13, 0}, {14, 128}, {60, 1}
    };
    bool restore, off;
    uint8_t value, before;
    int ret;
    if (!a || !a->control || !a->control->initialized || !a->control->read ||
        !a->control->write || !a->wait_ms || !a->initialized) return -1;
    if (a->control->amp_enabled || a->route == VDSP_AUDIO_HT326) return -2;
    if (a->route != VDSP_AUDIO_OFF && a->route != VDSP_AUDIO_LINE &&
        a->route != VDSP_AUDIO_DAC_SPEAKERS) return -1;
    off = a->route == VDSP_AUDIO_OFF;
    restore = !a->muted && !off;

    if (vdsp_v2_control_amp(a->control, false, false, false)) return fail(a, -1);
    ret = vdsp_v2_audio_mute(a, true);
    if (ret) return fail(a, ret);
    ret = vdsp_v2_audio_check(a);
    if (ret) return fail(a, ret);
    for (unsigned i = 0; i < sizeof(signature) / sizeof(signature[0]); ++i) {
        if (rd(a, signature[i][0], &value)) return fail(a, -1);
        if (value != signature[i][1]) return fail(a, -4);
    }
    if (wr(a, 0, 8) || rd(a, 1, &before)) return fail(a, -1);
    /* An outstanding switch has unknown ownership: don't cancel or race it. */
    if (before & 1) return fail(a, -4);
    if (wr(a, 1, 4) || rd(a, 1, &value)) return fail(a, -1);
    if ((value & 7) != (uint8_t)(4 | (before & 2))) return fail(a, -4);
    if (write_bank(a, 8, pair->channels)) return fail(a, -1);
    if (off) {
        /* Powered-down DACs do not switch at a sample boundary. Populate
         * both physical buffers (TI table 6-12), without powering anything. */
        if (write_bank(a, 12, pair->channels)) return fail(a, -1);
    } else {
        if (wr(a, 1, 5)) return fail(a, -1);
        for (unsigned elapsed = 0; ; ++elapsed) {
            if (rd(a, 1, &value)) return fail(a, -1);
            if (!(value & 1)) {
                if ((value & 6) != (uint8_t)(4 | ((before ^ 2) & 2))) return fail(a, -4);
                break;
            }
            if (elapsed == 20) return fail(a, -3);
            a->wait_ms(a->wait_context, 1);
        }
    }
    if (wr(a, 0, 0)) return fail(a, -1);
    ret = vdsp_v2_audio_volume(a, pair->volume_half_db[0]); /* Already muted. */
    if (ret) return fail(a, ret);
    if (pair->volume_half_db[1] != pair->volume_half_db[0]) {
        if (wr(a, 66, (uint8_t)pair->volume_half_db[1])) return fail(a, -1);
        if (pair->volume_half_db[1] > a->volume_half_db)
            a->volume_half_db = pair->volume_half_db[1];
    }
    if (restore) {
        ret = vdsp_v2_audio_mute(a, false);
        if (ret) return fail(a, ret);
    }
    a->last_error = 0;
    return 0;
}

int vdsp_v2_equalizer_apply(vdsp_v2_audio *a, unsigned sample_rate,
                          float bass_db, float treble_db, int base_volume_half_db)
{
    vdsp_v2_equalizer_pair pair = {0};
    int volume;
    if (sample_rate != VDSP_V2_EQ_SAMPLE_RATE || base_volume_half_db < -127 ||
        base_volume_half_db > 0 ||
        vdsp_v2_equalizer_generate(bass_db, treble_db, &pair.channels[0])) return -2;
    pair.channels[1] = pair.channels[0];
    volume = base_volume_half_db + pair.channels[0].gain_half_db;
    if (volume < -127) volume = -127;
    if (volume > 0) volume = 0;
    pair.volume_half_db[0] = pair.volume_half_db[1] = volume;
    return apply_prepared(a, &pair);
}

int vdsp_v2_equalizer_apply_pair_off(vdsp_v2_audio *a, unsigned sample_rate,
                                   float bass_left, float treble_left,
                                   float bass_right, float treble_right,
                                   int base_volume_half_db,
                                   vdsp_v2_equalizer_pair *applied)
{
    vdsp_v2_equalizer_pair pair;
    if (!applied || sample_rate != VDSP_V2_EQ_SAMPLE_RATE ||
        vdsp_v2_equalizer_prepare_pair(bass_left, treble_left, bass_right, treble_right,
                                     base_volume_half_db, &pair)) return -2;
    if (!a) return -1;
    if (a->route != VDSP_AUDIO_OFF) return -2;
    const int error = apply_prepared(a, &pair);
    if (!error) *applied = pair;
    return error;
}

int vdsp_v2_crossover_apply_off(vdsp_v2_audio *a, unsigned sample_rate,
                              const vdsp_v2_crossover_config *config,
                              int base_volume_half_db,
                              vdsp_v2_equalizer_pair *applied)
{
    vdsp_v2_equalizer_pair pair;
    if (!applied || sample_rate != VDSP_V2_EQ_SAMPLE_RATE ||
        vdsp_v2_crossover_prepare(config, base_volume_half_db, &pair)) return -2;
    if (!a) return -1;
    if (a->route != VDSP_AUDIO_OFF) return -2;
    const int error = apply_prepared(a, &pair);
    if (!error) *applied = pair;
    return error;
}


#endif
