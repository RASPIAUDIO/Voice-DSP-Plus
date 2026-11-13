/* Portable TLV320DAC3101 EQ, derived from archived VoiceDSPPlus.
 * Serialize the WHOLE apply call on the existing sole bus-owner worker.
 * No route selection, HT326 enable, preset policy or platform I/O here.
 */
#ifndef VOICEDSP_V2_EQUALIZER_H
#define VOICEDSP_V2_EQUALIZER_H
#include "voicedsp_v2_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VDSP_V2_EQ_SAMPLE_RATE 48000u
#define VDSP_V2_EQ_FILTERS 3u
#define VDSP_V2_EQ_BYTES_PER_FILTER 10u

typedef struct {
    /* A: 120 Hz bass, B: 6 kHz treble, C: identity. N0,N1,N2,D1,D2,
     * signed Q1.15, MSB first. Identical coefficients for both channels. */
    uint8_t coefficients[VDSP_V2_EQ_FILTERS][VDSP_V2_EQ_BYTES_PER_FILTER];
    int gain_half_db; /* V1 numerator compensation minus boost headroom. */
} vdsp_v2_equalizer_coefficients;

/* Pure calculation; finite fractional dB in [-6,6]. Returns -2 for invalid
 * input, leaving output untouched. Uses the archived <0.01 dB flat deadband.
 * V1 headroom is NOT a no-clipping bound after Q1.15 rounding; see EQUALIZER.md.
 * This is an EQ, not a limiter or an acoustic/speaker safety qualification. */
int vdsp_v2_equalizer_generate(float bass_db, float treble_db,
                             vdsp_v2_equalizer_coefficients *output);

typedef struct {
    vdsp_v2_equalizer_coefficients channels[2]; /* DAC left, DAC right. */
    int volume_half_db[2]; /* Independent compensation, never positive. */
} vdsp_v2_equalizer_pair;

#define VDSP_V2_CROSSOVER_DEFAULT_HZ 8500.0f
#define VDSP_V2_CROSSOVER_MIN_HZ 6000.0f
#define VDSP_V2_CROSSOVER_MAX_HZ 10000.0f
#define VDSP_V2_CROSSOVER_MUTE_LEFT 1u
#define VDSP_V2_CROSSOVER_MUTE_RIGHT 2u
#define VDSP_V2_CROSSOVER_CONFIG_DEFAULT \
    {VDSP_V2_CROSSOVER_DEFAULT_HZ, 0.0f, 0.0f, 0u}

typedef struct {
    float cutoff_hz; /* Finite, inclusive 6000..10000 Hz; no implicit default. */
    float left_trim_db, right_trim_db; /* Finite, inclusive -24..0 dB. */
    unsigned mute_mask; /* Bit 0: left, bit 1: right; other bits rejected. */
} vdsp_v2_crossover_config;

/* Pure 48 kHz PRB_P1 LR4: two identical Butterworth sections per channel,
 * left lowpass / right highpass, third stage Q1.15 identity (32767/32768).
 * TI encoding: N0=b0, N1=b1/2, N2=b2, D1=-a1/2, D2=-a2, MSB first.
 * Numerators include trims, rounded downward, with a post-quantization
 * unity-response bound per section; gain_half_db is always zero. Both DAC
 * volumes equal base (-127..0 half-dB), so even base=-127 preserves trims.
 * Mute zeros both LR4 numerators, not just volume; it survives DAC unmute.
 * Exact highpass DC zeros are preserved. No shelves or positive compensation.
 * generate() is prepare() at base=0. Return 0 or -2, output unchanged on error.
 * No routing, summing, bus access or hardware qualification. Retain the user's
 * 4.7 uF series tweeter capacitor; its acoustic/electrical response is not
 * modeled here. The response bound is not a transient/overflow limiter. */
int vdsp_v2_crossover_generate(const vdsp_v2_crossover_config *config,
                             vdsp_v2_equalizer_pair *output);
int vdsp_v2_crossover_prepare(const vdsp_v2_crossover_config *config,
                            int base_volume_half_db,
                            vdsp_v2_equalizer_pair *output);

/* Serialized sole-bus-owner call, only while route OFF and HT326 disabled.
 * Uses the same safe transaction and error codes as equalizer_apply below;
 * verifies actual 48 kHz/PRB_P1, writes both banks, stays OFF/muted. Required
 * applied output changes only on success. Invalid config/rate/base/non-OFF
 * route is -2 without I/O. Null/unusable audio is -1. Caller must reapply on
 * level/profile changes and retain the pair, including coefficient mutes.
 * Full-range bypass remains equalizer_apply_pair_off(...,0,0,0,0,...). */
int vdsp_v2_crossover_apply_off(vdsp_v2_audio *audio, unsigned sample_rate,
                              const vdsp_v2_crossover_config *config,
                              int base_volume_half_db,
                              vdsp_v2_equalizer_pair *applied);

/* Flat driver-identification measurement, NOT a replacement tweeter crossover.
 * Three Q1.15 identity stages per channel; mute_mask bit 0=L, bit 1=R zeros
 * the first stage numerator. Mask 2 isolates L; mask 1 isolates R; 3 silences
 * both. No gain compensation. Common base must be -127..-72 half-dB inclusive
 * (-63.5..-36 dB); louder values are REJECTED, not clamped. Retain the series
 * 4.7 uF tweeter capacitor and parent measurement level/route interlocks.
 * Same OFF-only transaction, required applied output and error codes as
 * crossover_apply_off. Invalid mask/rate/base/route: -2 without I/O; unusable
 * audio: -1. Output unchanged on error. No route or amplifier enable. */
int vdsp_v2_measure_flat_apply_off(vdsp_v2_audio *audio, unsigned sample_rate,
                                 unsigned mute_mask, int base_volume_half_db,
                                 vdsp_v2_equalizer_pair *applied);

/* Pure preparation for two independently equalized mono outputs at 48 kHz.
 * Reuses V1 shelf math per channel. Common pre-EQ base volume -127..0;
 * adds the existing V2 2 dB reserve independently for each shaped channel.
 * Leaves output unchanged on invalid input. No bus access, route selection,
 * amplifier enable or audio summing. The two physical DAC volume registers
 * must be applied separately by a future serialized worker transaction.
 * Does not relax apply()'s active-HT326 refusal. Not an acoustic qualification. */
int vdsp_v2_equalizer_prepare_pair(float bass_left, float treble_left,
                                 float bass_right, float treble_right,
                                 int base_volume_half_db,
                                 vdsp_v2_equalizer_pair *output);

/* Apply independent coefficients/compensation only with route OFF and HT326
 * disabled. Populates both adaptive buffers without powering the DAC/amp.
 * On success, applied receives the exact per-channel state; audio's legacy
 * single volume field reports the louder channel. Caller must retain applied
 * rather than treating that legacy scalar as two equal channel volumes.
 * Same failure handling as apply(); never re-enables any output on success or
 * failure. A later route/level change must preserve/reapply the pair explicitly.
 * Returns -2 for a non-OFF route or invalid gains/rate/base (no bus writes). */
int vdsp_v2_equalizer_apply_pair_off(vdsp_v2_audio *audio, unsigned sample_rate,
                                   float bass_left, float treble_left,
                                   float bass_right, float treble_right,
                                   int base_volume_half_db,
                                   vdsp_v2_equalizer_pair *applied);

/* Requires initialized audio/control, HT326 OFF, and continuous 48 kHz clocks
 * for an active DAC. Checks the existing V2 48 kHz clock/PRB_P1 registers.
 * base_volume_half_db is an EXPLICIT pre-EQ volume (-127..0), not the previous
 * compensated a->volume_half_db. Final volume clamps base+gain to [-127,0].
 * This avoids accumulating compensation on repeated calls. Later ordinary
 * audio_volume calls replace that compensation; the owner must reapply EQ.
 * Success preserves OFF/LINE/DAC_SPEAKERS route and prior mute state, returns
 * page 0, and records the actual digital volume in a->volume_half_db.
 * Return: 0 success; -2 invalid input/HT326 refusal (no I/O); -1 state/bus;
 * -3 bounded timeout; -4 unexpected codec configuration/status.
 * A transaction failure invalidates audio, requests mute/shutdown, and marks
 * route UNKNOWN. Dead-bus shutdown/page restoration is only best effort;
 * muted=true then records safe intent, NOT confirmed physical silence.
 * Explicit reinitialization is required after failure; never auto-retry. */
int vdsp_v2_equalizer_apply(vdsp_v2_audio *audio, unsigned sample_rate,
                          float bass_db, float treble_db, int base_volume_half_db);

#ifdef __cplusplus
}
#endif
#endif
