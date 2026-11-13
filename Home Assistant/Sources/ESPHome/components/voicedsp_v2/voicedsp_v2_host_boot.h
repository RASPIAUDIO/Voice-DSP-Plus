#ifdef __cplusplus
extern "C" {
#endif
#ifndef VOICEDSP_V2_HOST_BOOT_H
#define VOICEDSP_V2_HOST_BOOT_H
#include "voicedsp_v2_control.h"
#include <stddef.h>
#define VDSP_HOST_BOOT_SIZE 16
typedef struct {
    uint8_t packet[VDSP_HOST_BOOT_SIZE], accepted[VDSP_HOST_BOOT_SIZE];
    unsigned received, read_index, rate_khz;
    bool writing, overflow, committed;
} vdsp_v2_host_boot;
uint16_t vdsp_v2_boot_crc(const uint8_t *, size_t);
int vdsp_v2_host_boot_init(vdsp_v2_host_boot *, unsigned rate_khz);
void vdsp_v2_host_boot_write_begin(vdsp_v2_host_boot *);
bool vdsp_v2_host_boot_receive(vdsp_v2_host_boot *, uint8_t);
void vdsp_v2_host_boot_stop(vdsp_v2_host_boot *);
void vdsp_v2_host_boot_read_begin(vdsp_v2_host_boot *);
uint8_t vdsp_v2_host_boot_read(vdsp_v2_host_boot *);
/* Per-tile record, copied across the existing pre-RTOS geometry channel. */
extern uint8_t vdsp_v2_host_record[VDSP_HOST_BOOT_SIZE];
extern volatile uint32_t vdsp_v2_host_i2s_frames;
#endif

#ifdef __cplusplus
}
#endif
