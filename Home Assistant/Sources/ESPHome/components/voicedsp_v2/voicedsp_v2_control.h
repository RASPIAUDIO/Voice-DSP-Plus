#ifdef __cplusplus
extern "C" {
#endif
/* Voice DSP+ V2.02 offline bring-up core. Transport must serialize bus access.
 * NXP PCAL6416A Rev 7.1, table 6. No V1 PCAL6408 register compatibility.
 */
#ifndef VOICEDSP_V2_CONTROL_H
#define VOICEDSP_V2_CONTROL_H
#include <stdbool.h>
#include <stdint.h>
#include "voicedsp_v2_attributes.h"

#define VDSP_V2_PCAL_ADDR 0x20u
#define VDSP_V2_AMP_ENABLE 0x10u
#define VDSP_V2_AMP_SD_N 0x80u

typedef int (*vdsp_write_fn)(void *, uint8_t, uint8_t, uint8_t);
typedef int (*vdsp_read_fn)(void *, uint8_t, uint8_t, uint8_t *);
typedef struct {
    void *context;
    VDSP_FPTR("vdsp_reg_write") vdsp_write_fn write;
    VDSP_FPTR("vdsp_reg_read") vdsp_read_fn read;
    bool initialized;
    bool amp_enabled;
} vdsp_v2_control;

typedef enum {
    VDSP_MIC_UNKNOWN = 0,
    VDSP_MIC_INTERNAL_SQUARE,
    VDSP_MIC_EXTERNAL_SQUARE,
    VDSP_MIC_EXTERNAL_LINEAR
} vdsp_v2_mic_array;

typedef struct {
    bool valid;
    uint8_t port0;
    uint8_t port1;
    vdsp_v2_mic_array microphone;
    bool buck_pg;
    bool amp_sd_n;
    uint8_t buttons_pressed;
    bool gesture_irq;
} vdsp_v2_sample;

/* Never asserts reset/BOOT_SEL or drives AMP_SD_N. Only P0.4 becomes output.
 * Host recovery/reset sequences and interrupt dispatch remain separate work.
 */
int vdsp_v2_control_init(vdsp_v2_control *device);
int vdsp_v2_control_sample(vdsp_v2_control *device, vdsp_v2_sample *sample);
int vdsp_v2_control_amp(vdsp_v2_control *device, bool enable,
                        bool audio_ready, bool power_approved);
vdsp_v2_mic_array vdsp_v2_decode_microphones(bool valid, uint8_t port0, uint8_t port1);
#endif

#ifdef __cplusplus
}
#endif
