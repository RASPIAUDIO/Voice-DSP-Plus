#ifdef __cplusplus
extern "C" {
#endif
#ifndef VOICEDSP_V2_POWER_H
#define VOICEDSP_V2_POWER_H
#include "voicedsp_v2_control.h"
#define VDSP_V2_HUSB_ADDR 0x08u

typedef enum { VDSP_POWER_UNKNOWN, VDSP_POWER_UNATTACHED, VDSP_POWER_ATTACHED_UNKNOWN,
               VDSP_POWER_5V, VDSP_POWER_PD } vdsp_v2_power_kind;
typedef struct {
    bool valid, attached, cc2, legacy_current_known;
    vdsp_v2_power_kind kind;
    uint8_t raw_status0, raw_status1, response;
    uint16_t contract_mv, contract_ma, legacy_ma;
    uint32_t contract_mw, captured_ms;
} vdsp_v2_power_sample;
typedef struct {
    void *context;
    VDSP_FPTR("vdsp_reg_read") vdsp_read_fn read;
    vdsp_v2_power_sample current, last_good;
    uint32_t errors;
    int last_error;
} vdsp_v2_power;
typedef struct {
    bool valid;
    uint8_t present_mask;
    uint16_t current_ma[6]; /* 5,9,12,15,18,20 V; only present bits meaningful. */
} vdsp_v2_power_caps;

/* HUSB238 Register Information rev1.1. No write/renegotiation capability here.
 * now_ms is monotonic modulo 2^32. Serialize whole calls with other bus users.
 * Returns -1 bus/argument, -2 reserved encoding, -3 changing snapshot.
 */
int vdsp_v2_power_decode(uint8_t status0, uint8_t status1, uint32_t now_ms, vdsp_v2_power_sample *);
int vdsp_v2_power_poll(vdsp_v2_power *, uint32_t now_ms);
int vdsp_v2_power_read_caps(vdsp_v2_power *, vdsp_v2_power_caps *);
bool vdsp_v2_power_fresh(const vdsp_v2_power_sample *, uint32_t now_ms, uint32_t max_age_ms);
#endif

#ifdef __cplusplus
}
#endif
