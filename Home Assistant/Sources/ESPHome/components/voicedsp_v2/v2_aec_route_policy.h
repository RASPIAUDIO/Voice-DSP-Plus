#pragma once

namespace esphome::voicedsp_v2 {
inline unsigned aec_route_key(bool external, bool internal) {
  return external ? 2u : internal ? 1u : 0u;
}
inline unsigned aec_route_profile(unsigned route) {
  // Reuse the unchanged Voice Preserve coefficients for the internal speakers.
  // Line/headphones/OFF retain the original V2 baseline.
  return route==1 || route==2 ? 4u : 6u;
}
}
