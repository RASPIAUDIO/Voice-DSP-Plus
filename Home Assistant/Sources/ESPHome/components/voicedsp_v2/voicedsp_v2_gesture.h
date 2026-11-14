#ifndef VOICEDSP_V2_GESTURE_H
#define VOICEDSP_V2_GESTURE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef int (*vdsp_gesture_transfer)(void *, uint8_t, const uint8_t *, size_t, uint8_t *, size_t);
typedef uint32_t (*vdsp_gesture_now)(void *);
/* Transport may return this ONLY when cancelled before issuing the transaction. */
enum { VDSP_GESTURE_CANCELLED = -1001 };
enum { VDSP_GESTURE_STALE_MS = 250 };
enum { VDSP_GESTURE_HEALTH_STALE_MS = 1000 };
enum { VDSP_GESTURE_IO_NONE, VDSP_GESTURE_IO_WRITE,
       VDSP_GESTURE_IO_POINTER, VDSP_GESTURE_IO_READ };
enum { VDSP_GESTURE_FAULT_NONE, VDSP_GESTURE_FAULT_TRANSPORT,
       VDSP_GESTURE_FAULT_ID, VDSP_GESTURE_FAULT_DISABLED };
typedef struct {
    uint32_t sampled_ms, event_ms, sequence, reads, errors, suppressed;
    uint16_t raw, event_raw, part_id;
    uint8_t attempts, stage;
    bool valid, ready;
    /* Health is independent of gesture/event freshness. A successful runtime
     * bank/enable operation can refresh health, NEVER sampled_ms/event_ms. */
    uint32_t health_sampled_ms, failure_ms;
    bool health_valid;
    /* Exact callback result, including 0 and pre-transaction cancellation.
     * Last failure is latched across backoff/success until a new failure/init.
     * Semantic ID/enable failures have transport result 0, not a fake NACK. */
    int last_transport_result, failure_transport_result;
    uint8_t last_register, last_operation;
    uint8_t failure_kind, failure_stage, failure_register, failure_operation;
} vdsp_v2_gesture_snapshot;
typedef struct {
    void *context;
    vdsp_gesture_transfer transfer;
    vdsp_gesture_now now;
    vdsp_v2_gesture_snapshot snapshot;
    uint32_t phase_ms, last_poll_ms, health_ms, last_event_ms;
    unsigned index;
    int last_result;
    bool armed, have_event, configured, recovering;
    unsigned recovery_attempts;
} vdsp_v2_gesture;

/* Single existing bus owner only. transfer MUST return 0 only for exact lengths
 * and have a finite timeout. PAJ reads use STOP between pointer and read.
 * No delays, bus scans or write retries inside the driver. Zero hardware I/O
 * in init. Init writes are batched (<=4, soft 5 ms budget per step).
 */
void vdsp_v2_gesture_init(vdsp_v2_gesture *, void *, vdsp_gesture_transfer, vdsp_gesture_now);
void vdsp_v2_gesture_step(vdsp_v2_gesture *, bool owner_ready, bool irq_asserted);
/* PixArt v1.5 front-view SENSOR axes, not proven PCB/user axes. */
const char *vdsp_v2_gesture_name(uint16_t raw);
#ifdef __cplusplus
}
#endif
#endif
