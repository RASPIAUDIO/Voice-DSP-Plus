#include "v2_ui_runtime.h"
#include <cmath>
#include <cstring>

namespace esphome::voicedsp_v2 {
namespace {
constexpr uint8_t CAPS[] = {'V', '2', 'U', 'I', 1, 15, 24, 0};
constexpr uint8_t COMMANDS[] = {11, 14, 15};
constexpr uint8_t MAXIMUM[] = {1, 14, 100};
constexpr int TRANSPORT = -1, SESSION = -2, SIGNATURE = -3, READBACK = -4, LEVELS = -5;
}

bool UiRuntime::allowed_(const vdsp_v2_host &h) const {
  return h.ops.transfer && h.ops.now_ms && h.ops.wait_ms && h.stage == VDSP_HOST_READY &&
      h.session_valid && h.clocks && h.io.valid && !h.last_error &&
      (!h.ops.cancelled || !h.ops.cancelled(h.ops.context)) &&
      static_cast<uint32_t>(h.ops.now_ms(h.ops.context) - h.refreshed_ms) <= 250;
}

bool UiRuntime::current_(const vdsp_v2_host &h) const {
  return allowed_(h) && have_record_ && std::memcmp(record_, h.record, sizeof(record_)) == 0;
}

void UiRuntime::invalidate(int error) {
  snapshot_ = {};
  snapshot_.error = error;
  active_ = false;
  // Keep write attempts and failures for this cookie. A transient host failure
  // must not turn an ambiguous write into an automatic write retry.
}

void UiRuntime::fail_(int error) {
  invalidate(error);
  failed_ = true;
  failure_error_ = error;
  retry_wait_ = false;
}

int UiRuntime::read_(vdsp_v2_host &h, uint8_t command, uint8_t *data, size_t count, uint8_t resource) {
  if (!count || count > 48) return TRANSPORT;
  uint8_t header[] = {resource, static_cast<uint8_t>(command | 0x80), static_cast<uint8_t>(count + 1)};
  uint8_t reply[49]{};
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    if (!current_(h)) return SESSION;
    if (h.ops.transfer(h.ops.context, 0x2c, header, 3, reply, count + 1)) return TRANSPORT;
    if (!current_(h)) return SESSION;
    if (!reply[0]) { std::memcpy(data, reply + 1, count); return 0; }
    if (reply[0] != 64 || attempt == 2) return reply[0];
    h.ops.wait_ms(h.ops.context, 2);
  }
  return 64;
}

int UiRuntime::write_(vdsp_v2_host &h, uint8_t command, uint8_t value) {
  if (!snapshot_.available || !current_(h)) return SESSION;
  const uint8_t packet[] = {0x30, command, 1, value};
  if (h.ops.transfer(h.ops.context, 0x2c, packet, sizeof(packet), nullptr, 0)) return TRANSPORT;
  h.ops.wait_ms(h.ops.context, 2);
  if (!current_(h)) return SESSION;
  uint8_t status = 0xff;
  // The write ends in STOP; its completion status is a separate read. Never retry.
  if (h.ops.transfer(h.ops.context, 0x2c, nullptr, 0, &status, 1)) return TRANSPORT;
  if (!current_(h)) return SESSION;
  return status;
}

void UiRuntime::step(vdsp_v2_host &h, const uint32_t requests[3], uint32_t aec_request, uint32_t profile_request, unsigned aec_default) {
  if (!allowed_(h)) { invalidate(SESSION); return; }
  if (!have_record_ || std::memcmp(record_, h.record, sizeof(record_))) {
    std::memcpy(record_, h.record, sizeof(record_));
    have_record_ = true;
    failed_ = false;
    for (auto &attempt : attempted_) attempt = 0;
    for (auto &blocked : blocked_) blocked = 0;
    retry_wait_ = false;
    invalidate();
  }
  const uint32_t now = h.ops.now_ms(h.ops.context);
  if (failed_) {
    // Unknown firmware/malformed values remain fail-closed for this session.
    if (failure_error_ != TRANSPORT && failure_error_ != SESSION && failure_error_ != 64) return;
    if (!retry_wait_) { retry_ms_ = now; retry_wait_ = true; return; }
    if (static_cast<uint32_t>(now - retry_ms_) < 1000U) return;
    failed_ = false;
    retry_wait_ = false;
    // Re-probe capabilities/readback; attempted writes are NEVER reset here.
  }
  if (!active_) {
    snapshot_ = {};
    snapshot_.session_cookie = static_cast<uint32_t>(record_[10]) | (static_cast<uint32_t>(record_[11]) << 8) |
        (static_cast<uint32_t>(record_[12]) << 16) | (static_cast<uint32_t>(record_[13]) << 24);
    active_ = true;
    last_levels_ms_ = now;
  }
  if (!snapshot_.available) {
    uint8_t caps[8]{};
    const int error = read_(h, 16, caps, sizeof(caps));
    if (error || std::memcmp(caps, CAPS, sizeof(caps))) { fail_(error ? error : SIGNATURE); return; }
    snapshot_.available = true;
    return;
  }
  bool *valid[] = {&snapshot_.mute_valid, &snapshot_.state_valid, &snapshot_.progress_valid};
  for (unsigned index = 0; index < 3; ++index) {
    const uint32_t request = requests[index];
    if (request && blocked_[index] == request) continue;
    if (*valid[index] && snapshot_.acknowledged[index] == request) continue;
    *valid[index] = false;
    const uint8_t desired = static_cast<uint8_t>(request);
    if (request && desired > MAXIMUM[index]) { fail_(READBACK); return; }
    int error = 0;
    if (request && attempted_[index] != request) {
      attempted_[index] = request;
      error = write_(h, COMMANDS[index], desired);
    }
    uint8_t actual = 0xff;
    if (!error) error = read_(h, COMMANDS[index], &actual, 1);
    if (!error && actual > MAXIMUM[index]) error = READBACK;
    if (error) { fail_(error); return; }
    if (index == 0) snapshot_.muted = actual != 0;
    else if (index == 1) snapshot_.state = actual;
    else snapshot_.progress = actual;
    if (request && actual != desired) {
      // Ambiguous/failed write: expose the actual value, quarantine this
      // request, and keep other UI commands alive. Only new intent can write.
      blocked_[index] = request;
      snapshot_.error = READBACK;
      continue;
    }
    snapshot_.acknowledged[index] = request;
    *valid[index] = true;
    return; // At most one control update per worker iteration; mute has priority.
  }
  aec_.step(h, aec_request, profile_request, aec_default);
  snapshot_.aec = aec_.snapshot();
  // One read-only AEC query per slot, on the existing worker after UI writes.
  // IDs are pinned to the archived generated aec_cmds.h, resource 0x21.
  if (static_cast<uint32_t>(now - last_doa_ms_) >= 200) {
    last_doa_ms_ = now;
    const unsigned sequence[] = {0, 1, 3};
    const unsigned kind = snapshot_.geometry_valid ? sequence[doa_phase_++ % 3] : 2;
    const uint8_t commands[] = {75, 80, 74, 11};
    const size_t count = kind == 2 ? 12 : (kind == 3 ? 2 : 4);
    uint8_t bytes[48]{};
    float values[12]{};
    int error = read_(h, commands[kind], bytes, count * 4, kind == 3 ? 0x23 : 0x21);
    if (!error) {
      for (size_t i = 0; i < count; ++i) {
        const uint8_t *p = bytes + 4 * i;
        const uint32_t bits = uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
            (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
        std::memcpy(values + i, &bits, 4);
        // AEC may legitimately report NaN angles for an inactive beam.
        if (std::isinf(values[i]) || (kind == 2 && !std::isfinite(values[i]))) error = -5;
      }
    }
    snapshot_.doa_error = error;
    if (kind == 0) {
      snapshot_.angles_valid = !error;
      if (!error) { std::memcpy(snapshot_.azimuths, values, 16); snapshot_.angles_ms = now; }
    } else if (kind == 1) {
      snapshot_.energies_valid = !error;
      if (!error) { std::memcpy(snapshot_.energies, values, 16); snapshot_.energies_ms = now; }
    } else if (kind == 3) {
      snapshot_.selected_valid = !error;
      if (!error) { std::memcpy(snapshot_.selected, values, 8); snapshot_.selected_ms = now; }
    } else {
      snapshot_.geometry_valid = !error;
      if (!error) std::memcpy(snapshot_.geometry, values, 48);
      // Diagnostic failures do not invalidate audio/UI controls.
      if (error) snapshot_.doa_error = error;
    }
  }
  if (static_cast<uint32_t>(now - snapshot_.levels_ms) > 2500) snapshot_.levels_valid = false;
  if (static_cast<uint32_t>(now - last_levels_ms_) < 1000) return;
  last_levels_ms_ = now;
  uint8_t bytes[16]{};
  const int error = read_(h, 13, bytes, sizeof(bytes));
  snapshot_.levels_valid = false;
  snapshot_.error = error;
  if (error == 64) return; // No stable frame; no retry until the next 1 Hz slot.
  if (error) { fail_(error); return; }
  static_assert(sizeof(float) == 4, "XMOS RMS requires float32");
  for (unsigned mic = 0; mic < 4; ++mic) {
    const auto *p = bytes + 4 * mic;
    uint32_t bits = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                    (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    std::memcpy(&snapshot_.levels[mic], &bits, sizeof(bits));
    if (!std::isfinite(snapshot_.levels[mic]) || snapshot_.levels[mic] < 0) {
      snapshot_.error = LEVELS;
      return;
    }
  }
  snapshot_.levels_ms = h.ops.now_ms(h.ops.context);
  snapshot_.levels_valid = true;
}
}  // namespace esphome::voicedsp_v2
