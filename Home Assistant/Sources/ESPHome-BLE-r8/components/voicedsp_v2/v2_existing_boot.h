#pragma once
#include <cstdint>

namespace esphome::voicedsp_v2 {
// A claimed session may have committed a boot record or started clocks. Never
// retry it, reset it, or send a second image implicitly after an uncertain error.
template<class Attempt, class Claimed, class Cancel, class Now, class Wait>
int wait_existing_boot(Attempt attempt, Claimed claimed, Cancel cancel, Now now, Wait wait,
                       uint32_t timeout_ms) {
  const uint32_t start = now();
  int result = -3;
  while (static_cast<uint32_t>(now() - start) < timeout_ms) {
    if (cancel()) return -5;
    result = attempt(false);
    if (cancel()) return -5;
    if (!result || claimed()) return result;
    result = attempt(true);
    if (cancel()) return -5;
    if (!result || claimed()) return result;
    wait(100);
  }
  return result;
}

inline bool production_reset_held(uint8_t config, uint8_t output, uint8_t input) {
  return config == 0xe6u && output == 0 && !(input & 1u);
}
}  // namespace esphome::voicedsp_v2
