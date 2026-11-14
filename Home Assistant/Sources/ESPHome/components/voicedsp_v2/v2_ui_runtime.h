#pragma once
#include <cstdint>
#include "v2_aec_diag.h"
#include "voicedsp_v2_host.h"

namespace esphome::voicedsp_v2 {
struct UiSnapshot {
  bool available{}, mute_valid{}, state_valid{}, progress_valid{}, levels_valid{};
  bool muted{};
  bool style_available{}, style_valid[4]{};
  uint8_t style[4]{};
  uint8_t state{}, progress{};
  uint32_t session_cookie{}, levels_ms{};
  uint32_t acknowledged[7]{};
  float levels[4]{};
  int error{};
  AecSnapshot aec{};
  float azimuths[4]{}, energies[4]{}, geometry[12]{}, selected[2]{};
  bool angles_valid{}, energies_valid{}, geometry_valid{}, selected_valid{};
  uint32_t angles_ms{}, energies_ms{}, selected_ms{};
  int doa_error{};
};

// Invoked only by the existing worker. No scheduler, bus owner or implicit recovery.
class UiRuntime {
 public:
  void step(vdsp_v2_host &host, const uint32_t requests[7], uint32_t aec_request = 0, uint32_t profile_request = 0, unsigned aec_default = 0);
  void invalidate(int error = 0);
  UiSnapshot snapshot() const { return snapshot_; }
  static uint32_t next_request(uint32_t previous, uint8_t value) {
    return 0x80000000u | ((previous + 0x100u) & 0x7fffff00u) | value;
  }

 private:
  bool allowed_(const vdsp_v2_host &host) const;
  bool current_(const vdsp_v2_host &host) const;
  int read_(vdsp_v2_host &host, uint8_t command, uint8_t *data, size_t count, uint8_t resource = 0x30);
  int write_(vdsp_v2_host &host, uint8_t command, uint8_t value);
  void fail_(int error);
  UiSnapshot snapshot_{};
  AecDiag aec_{};
  uint8_t record_[16]{};
  bool have_record_{false}, active_{false}, failed_{false};
  uint32_t attempted_[7]{};
  uint32_t last_levels_ms_{};
  uint32_t last_doa_ms_{};
  unsigned doa_phase_{};
  uint32_t retry_ms_{};
  uint32_t blocked_[7]{};
  int failure_error_{};
  bool retry_wait_{};
};
}  // namespace esphome::voicedsp_v2
