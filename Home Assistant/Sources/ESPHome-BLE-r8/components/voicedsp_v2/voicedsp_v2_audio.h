#ifdef __cplusplus
extern "C" {
#endif
/* Single-owner audio controller. Serialize EACH WHOLE call with all other bus
 * users, including page-based DAC accesses. No ISR calls or concurrent masters.
 * TI TLV320DAC3101 SLAS666B; V2.02 PCAL6416/HT326 wiring.
 */
#ifndef VOICEDSP_V2_AUDIO_H
#define VOICEDSP_V2_AUDIO_H
#include "voicedsp_v2_control.h"

#define VDSP_V2_DAC_ADDR 0x18u

typedef enum {
    VDSP_AUDIO_UNKNOWN = 0, VDSP_AUDIO_OFF, VDSP_AUDIO_LINE,
    VDSP_AUDIO_HT326, VDSP_AUDIO_DAC_SPEAKERS
} vdsp_v2_audio_route;
typedef enum { VDSP_DIN_PRIMARY = 0, VDSP_DIN_SECONDARY = 1 } vdsp_v2_audio_source;

typedef struct {
    unsigned sample_rate;       /* 16000 or 48000, BCLK MUST be 64 * fs. */
    unsigned word_bits;         /* I2S 16, 24 or 32 within two 32-bit slots. */
    vdsp_v2_audio_source source;
    bool clocks_running;       /* Platform obligation, not inferred from PG. */
} vdsp_v2_audio_config;

typedef void (*vdsp_wait_ms_fn)(void *, unsigned);
typedef struct {
    vdsp_v2_control *control;
    VDSP_FPTR("vdsp_wait_ms") vdsp_wait_ms_fn wait_ms;
    void *wait_context;
    bool initialized;
    bool muted;
    vdsp_v2_audio_route route;
    int volume_half_db;         /* -127..0; linked level or louder channel of an EQ pair. */
    unsigned external_gain_db;  /* 0..9 analog HPL/HPR PGA, HT326 route only. */
    unsigned internal_gain_db;  /* Legacy field; normal level requests now leave this at zero. */
    int last_error;
    unsigned fault_reg, fault_value, fault_expected; /* Last codec status mismatch. */
} vdsp_v2_audio;

/* Return 0, -1 I2C/state error, -2 bad argument/power refusal, -3 timeout,
 * -4 codec/amp fault. Failure invalidates initialized state; no automatic retry.
 * On failed shutdown the actual outputs may remain active: UNKNOWN is deliberate.
 */
int vdsp_v2_audio_init(vdsp_v2_audio *, const vdsp_v2_audio_config *);
int vdsp_v2_audio_select(vdsp_v2_audio *, vdsp_v2_audio_route, bool power_approved);
int vdsp_v2_audio_mute(vdsp_v2_audio *, bool);
int vdsp_v2_audio_volume(vdsp_v2_audio *, int half_db);
int vdsp_v2_audio_stop(vdsp_v2_audio *);
int vdsp_v2_audio_check(vdsp_v2_audio *);
#endif

#ifdef __cplusplus
}
#endif
