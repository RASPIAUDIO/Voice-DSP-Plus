#ifdef __cplusplus
extern "C" {
#endif
#ifndef VOICEDSP_V2_HOST_H
#define VOICEDSP_V2_HOST_H
#include "voicedsp_v2_host_boot.h"
#include "voicedsp_v2_policy.h"

/* Single owner, whole-call serialization, never from an ISR. Every transport
 * callback MUST have a finite platform timeout, return 0 only for exact lengths,
 * and never retry writes implicitly. Combined transfers use repeated START;
 * standalone writes end in STOP. No general scan/recovery clock pulses.
 */
typedef struct {
    void *context;
    int (*transfer)(void *,uint8_t,const uint8_t *,size_t,uint8_t *,size_t);
    uint32_t (*now_ms)(void *);
    vdsp_wait_ms_fn wait_ms;
    int (*start_silent_clocks)(void *,unsigned);
    bool (*cancelled)(void *); /* Optional cooperative cancellation, no I2C. */
} vdsp_v2_host_ops;
typedef enum { VDSP_HOST_NEW, VDSP_HOST_READY, VDSP_HOST_FAULT } vdsp_v2_host_stage;

/* Runtime read diagnostics only; never used to relax audio authorization. */
typedef struct {
    unsigned stage, command, status, attempts;
    int error;
    uint32_t elapsed_ms, frames_before, frames_after, ticks_before, ticks_after;
} vdsp_v2_runtime_trace;
typedef int (*vdsp_v2_output_prepare_fn)(void *,vdsp_v2_audio *,unsigned rate,
                                        unsigned attenuation);

typedef struct {
    vdsp_v2_host_ops ops;
    vdsp_v2_control control;
    vdsp_v2_audio audio;
    vdsp_v2_power power;
    vdsp_v2_policy policy;
    vdsp_v2_sample io;
    vdsp_v2_route_decision decision;
    vdsp_v2_host_stage stage;
    uint8_t record[16];
    unsigned rate;
    bool initialized, claimed_bus, session_valid, clocks;
    int last_error;
    uint32_t refreshed_ms, inputs_ms;
    vdsp_v2_runtime_trace runtime_probe, runtime_failure;
    uint32_t runtime_failures;
    /* VDSP_V2_PERSISTENT_PREPARE_V1; single owner, context must remain alive. */
    vdsp_v2_output_prepare_fn output_prepare;
    void *output_prepare_context;
    unsigned output_attenuation;
    vdsp_v2_audio_route output_prepared_route;
    bool output_prepared_valid;
} vdsp_v2_host;

/* paired_image_installed is an explicit setup prerequisite, NOT auto-detection.
 * Wrong-mode probing can contend with USB .5 before identity is read.
 * No hardware access in init. Limits must be supplied by the platform/product.
 */
int vdsp_v2_host_init(vdsp_v2_host *,const vdsp_v2_host_ops *,unsigned rate,
                      const vdsp_v2_policy_limits *,bool paired_image_installed);
/* Starts once, takes a fresh host cookie, waits boundedly for bootstrap/runtime,
 * initializes safe PCAL + DAC reset, then enters READY with all audio OFF.
 * Failure is latched. New bootstrap requires controlled cold restart.
 */
int vdsp_v2_host_start(vdsp_v2_host *,uint32_t cookie);
/* Reattach after an ESP-only restart to the paired .6 runtime. Validates its
 * version, CRC/config record, current geometry and advancing clocks; retains
 * the XMOS cookie. Never writes bootstrap/QSPI or enables an audio output.
 * Only use on a fresh NEW host; failures are terminal for this instance.
 */
int vdsp_v2_host_resume(vdsp_v2_host *);
/* Only explicit request can reinitialize a failed audio driver. Firmware/cookie
 * mismatch is terminal, not recoverable through request. Volume is 0..127 half-dB
 * attenuation. Calling context must marshal commands; no concurrent calls.
 */
int vdsp_v2_host_request(vdsp_v2_host *,vdsp_v2_audio_route,unsigned attenuation);
/* Optional sole-owner preparation after audio_stop and before route enable.
 * The callback may set coefficients/volumes but MUST leave audio OFF, muted,
 * initialized and the external amp disabled. Failure inhibits the request.
 * Persisted until explicit STOP, replacement request or host init. Context
 * MUST outlive polls. Select filters from host.decision.route (effective), not
 * policy.requested. Repeated for effective route changes/reinitialization,
 * never unchanged polls. Policy OFF defers preparation until activation.
 * NULL replaces a previous callback with the normal linked volume path.
 */
int vdsp_v2_host_request_prepared(vdsp_v2_host *,vdsp_v2_audio_route,unsigned attenuation,
                                  vdsp_v2_output_prepare_fn,void *context);
int vdsp_v2_host_poll(vdsp_v2_host *);
/* Best-effort shutdown; a dead bus returns an error and physical state UNKNOWN. */
int vdsp_v2_host_stop(vdsp_v2_host *);
#endif

#ifdef __cplusplus
}
#endif
