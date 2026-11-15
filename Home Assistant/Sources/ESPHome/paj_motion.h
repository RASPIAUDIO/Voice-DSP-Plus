#pragma once
#include <cstdint>

namespace vdsp_ui {
// The PAJ reports gestures, not continuous occupancy. Hold each event briefly
// for a readable motion indicator; unsigned subtraction handles timer wrap.
inline bool paj_recent_motion(uint32_t now, uint32_t event_ms, uint16_t event_raw) {
  return event_raw != 0 && static_cast<uint32_t>(now - event_ms) < 2000U;
}
}
