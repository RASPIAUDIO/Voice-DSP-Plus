#pragma once
#include <cstdint>
namespace esphome::voicedsp_v2 {
enum class BootBusState { QUIET, ACTIVE, SDA_STALLED, CLOCK_LOW };
// Passive observation only. A stalled SDA is distinct from legitimate activity
// or clock stretching. The caller may attempt one standard clear only afterwards.
template<class Read, class Now, class Wait>
BootBusState observe_boot_bus(Read read, Now now, Wait wait) {
  const bool sda = read(0), scl = read(1);
  const uint32_t start = now();
  while (static_cast<uint32_t>(now() - start) < 100000u) {
    if (read(0) != sda || read(1) != scl) return BootBusState::ACTIVE;
    wait(2);
  }
  if (!scl) return BootBusState::CLOCK_LOW;
  return sda ? BootBusState::QUIET : BootBusState::SDA_STALLED;
}
}  // namespace esphome::voicedsp_v2
