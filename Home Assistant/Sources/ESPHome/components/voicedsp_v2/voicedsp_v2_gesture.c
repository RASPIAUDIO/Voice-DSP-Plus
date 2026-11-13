#include "voicedsp_v2_gesture.h"
#include "voicedsp_v2_gesture_table.h"
#include <string.h>

enum { WAIT_POWER, WAKE_WAIT, IDENTIFY, CONFIGURE, BANK1, CHECK_ENABLE,
       BANK0, DRAIN, RUNNING, BACKOFF, FAILED };
enum { ADDR = 0x73, RETRY_MS = 10000, HEALTH_MS = 1000,
       COOLDOWN_MS = 650 };

static int transfer_reg(vdsp_v2_gesture *s, uint8_t reg, uint8_t operation,
                        const uint8_t *tx, size_t nt, uint8_t *rx, size_t nr)
{
    s->snapshot.last_register = reg;
    s->snapshot.last_operation = operation;
    s->last_result = s->transfer(s->context, ADDR, tx, nt, rx, nr);
    s->snapshot.last_transport_result = s->last_result;
    return s->last_result;
}
static int write_reg(vdsp_v2_gesture *s, uint8_t reg, uint8_t value)
{
    const uint8_t tx[] = {reg, value};
    return transfer_reg(s, reg, VDSP_GESTURE_IO_WRITE, tx, 2, NULL, 0);
}
static int read_reg(vdsp_v2_gesture *s, uint8_t reg, uint8_t *value, size_t n)
{
    int result = transfer_reg(s, reg, VDSP_GESTURE_IO_POINTER, &reg, 1, NULL, 0);
    return result ? result : transfer_reg(s, reg, VDSP_GESTURE_IO_READ, NULL, 0, value, n);
}
static void mark_health(vdsp_v2_gesture *s, uint32_t started_ms)
{
    if (!s->configured || !s->snapshot.ready) return;
    s->snapshot.health_sampled_ms = started_ms;
    s->snapshot.health_valid =
        (uint32_t)(s->now(s->context) - started_ms) <= VDSP_GESTURE_STALE_MS;
}
static void fail(vdsp_v2_gesture *s, bool terminal)
{
    if (!terminal && s->last_result == VDSP_GESTURE_CANCELLED) {
        /* No transaction was issued. Resume this phase after ownership returns. */
        s->snapshot.valid = false;
        s->snapshot.health_valid = false;
        s->armed = false;
        return;
    }
    ++s->snapshot.errors;
    s->snapshot.failure_ms = s->now(s->context);
    s->snapshot.failure_stage = s->snapshot.stage;
    s->snapshot.failure_register = s->snapshot.last_register;
    s->snapshot.failure_operation = s->snapshot.last_operation;
    s->snapshot.failure_transport_result = s->snapshot.last_transport_result;
    s->snapshot.failure_kind = !terminal ? VDSP_GESTURE_FAULT_TRANSPORT :
        s->snapshot.stage == IDENTIFY ? VDSP_GESTURE_FAULT_ID : VDSP_GESTURE_FAULT_DISABLED;
    s->snapshot.ready = s->snapshot.valid = false;
    s->snapshot.health_valid = false;
    s->configured = s->armed = false;
    s->snapshot.stage = terminal || s->snapshot.attempts >= 3 ? FAILED : BACKOFF;
    s->phase_ms = s->now(s->context);
}
void vdsp_v2_gesture_init(vdsp_v2_gesture *s, void *context,
                         vdsp_gesture_transfer transfer, vdsp_gesture_now now)
{
    memset(s, 0, sizeof(*s));
    s->context = context; s->transfer = transfer; s->now = now;
    s->phase_ms = now ? now(context) : 0;
    s->snapshot.stage = transfer && now ? WAIT_POWER : FAILED;
}

void vdsp_v2_gesture_step(vdsp_v2_gesture *s, bool owner_ready, bool irq)
{
    if (!s->now || !s->transfer) return;
    uint32_t now = s->now(s->context);
    if (!owner_ready) {
        s->snapshot.valid = false;
        s->snapshot.health_valid = false;
        s->armed = false;
        return;
    }
    if (s->snapshot.health_valid &&
        (uint32_t)(now - s->snapshot.health_sampled_ms) > VDSP_GESTURE_STALE_MS)
        s->snapshot.health_valid = false;
    if (s->snapshot.valid && (uint32_t)(now - s->snapshot.sampled_ms) > VDSP_GESTURE_STALE_MS) {
        s->snapshot.valid = false;
        s->armed = false;
    }
    uint8_t data[2] = {0};
    switch (s->snapshot.stage) {
    case WAIT_POWER:
        if ((uint32_t)(now - s->phase_ms) < 2) return;
        ++s->snapshot.attempts;
        /* Wake-up may NACK; only this first read is allowed to fail harmlessly. */
        (void)read_reg(s, 0x00, data, 1);
        if (s->last_result == VDSP_GESTURE_CANCELLED) { --s->snapshot.attempts; return; }
        s->phase_ms = s->now(s->context);
        s->snapshot.stage = WAKE_WAIT;
        return;
    case WAKE_WAIT:
        if ((uint32_t)(now - s->phase_ms) < 1) return;
        if (write_reg(s, 0xef, 0)) { fail(s, false); return; }
        s->snapshot.stage = IDENTIFY;
        return;
    case IDENTIFY:
        if (read_reg(s, 0x00, data, 2)) { fail(s, false); return; }
        s->snapshot.part_id = (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
        if (s->snapshot.part_id != 0x7620) { fail(s, true); return; }
        s->index = 0;
        s->snapshot.stage = CONFIGURE;
        return;
    case CONFIGURE:
        for (unsigned count = 0; count < 4 && s->index < VDSP_GESTURE_TABLE_COUNT; ++count) {
            const uint8_t *entry = vdsp_gesture_table[s->index];
            if (write_reg(s, entry[0], entry[1])) { fail(s, false); return; }
            ++s->index;
            if ((uint32_t)(s->now(s->context) - now) >= 5) break;
        }
        if (s->index == VDSP_GESTURE_TABLE_COUNT) s->snapshot.stage = BANK1;
        return;
    case BANK1:
        if (write_reg(s, 0xef, 1)) { fail(s, false); return; }
        mark_health(s, now);
        s->snapshot.stage = CHECK_ENABLE;
        return;
    case CHECK_ENABLE:
        if (read_reg(s, 0x72, data, 1)) { fail(s, false); return; }
        /* A reset/unconfigured sensor is not silently treated as healthy. */
        if (!(data[0] & 1)) { fail(s, true); return; }
        mark_health(s, now);
        s->snapshot.stage = BANK0;
        return;
    case BANK0:
        if (write_reg(s, 0xef, 0)) { fail(s, false); return; }
        mark_health(s, now);
        s->health_ms = s->now(s->context);
        s->snapshot.stage = s->configured ? RUNNING : DRAIN;
        return;
    case BACKOFF:
        if ((uint32_t)(now - s->phase_ms) >= RETRY_MS) {
            s->phase_ms = now;
            s->snapshot.stage = WAIT_POWER;
        }
        return;
    case FAILED:
        return;
    default:
        break;
    }
    if (s->snapshot.stage == RUNNING) {
        if ((uint32_t)(now - s->health_ms) >= HEALTH_MS) {
            s->snapshot.stage = BANK1;
            return;
        }
        uint32_t age = now - s->last_poll_ms;
        if (age < 20 || (!irq && age < 100)) return;
    }
    bool drain = s->snapshot.stage == DRAIN;
    s->last_poll_ms = now;
    /* Reading both flags consumes the result. Never retry or emit partial data. */
    if (read_reg(s, 0x43, data, 2)) { fail(s, false); return; }
    ++s->snapshot.reads;
    s->snapshot.sampled_ms = now;
    s->snapshot.raw = (uint16_t)(data[0] | ((uint16_t)(data[1] & 1) << 8));
    s->snapshot.ready = s->configured = true;
    mark_health(s, now);
    s->snapshot.stage = RUNNING;
    s->snapshot.valid = (uint32_t)(s->now(s->context) - now) <= VDSP_GESTURE_STALE_MS;
    if (!s->snapshot.valid) { s->armed = false; return; }
    if (!s->snapshot.raw) { s->armed = true; return; }
    if (drain || !s->armed || (s->have_event && (uint32_t)(now - s->last_event_ms) < COOLDOWN_MS)) {
        ++s->snapshot.suppressed;
        return;
    }
    s->have_event = true;
    s->last_event_ms = now;
    s->snapshot.event_ms = now;
    s->snapshot.event_raw = s->snapshot.raw;
    ++s->snapshot.sequence;
}

const char *vdsp_v2_gesture_name(uint16_t raw)
{
    switch (raw) {
    case 0x001: return "sensor_left";
    case 0x002: return "sensor_right";
    case 0x004: return "sensor_down";
    case 0x008: return "sensor_up";
    case 0x010: return "sensor_forward";
    case 0x020: return "sensor_backward";
    case 0x040: return "clockwise";
    case 0x080: return "counterclockwise";
    case 0x100: return "wave";
    default: return raw ? "combined" : "none";
    }
}
