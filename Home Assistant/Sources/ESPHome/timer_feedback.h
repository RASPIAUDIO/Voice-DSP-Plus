#pragma once
#include <cstdint>

namespace vdsp_timer_feedback {
struct Selection { int state; unsigned progress; };

// Reuse existing XMOS renderers; these state numbers do not initiate a flash.
inline Selection choose(int base, bool ringing, bool muted, uint32_t now,
                        bool active, uint32_t remaining, uint32_t total) {
  if (muted) return {9, 0};
  if (ringing) return {(now / 750U) % 2U ? 10 : 13, 0};
  if (base == 0 && active && remaining && total) {
    const uint64_t percent = (uint64_t(remaining) * 100U + total - 1U) / total;
    return {12, unsigned(percent > 100U ? 100U : percent)};
  }
  return {base, 0};
}
}  // namespace vdsp_timer_feedback
