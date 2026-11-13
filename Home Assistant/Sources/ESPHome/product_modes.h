#pragma once
#include <array>
#include <algorithm>

namespace vdsp_modes {
// Audio UI state only. No AEC parameters or XMOS commands belong here.
using Settings = std::array<int, 7>;
using LegacyMemory = std::array<int, 21>;
using Memory = std::array<int, 22>;
inline constexpr LegacyMemory legacy_defaults{
    1, 0, 0, -24, 9500, 0, -6,
    2, 1, 0, -24, 9500, 0, -6,
    0, 0, 0, -24, 9500, 0, -6};
inline constexpr Memory defaults{
    1, 0, -6, -24, 9500, 0, -6,
    2, 1, -6, -24, 9500, 0, -6,
    0, 0, -6, -24, 9500, 0, -6, 61};
inline constexpr Memory uninitialized{};
inline void migrate_levels(Memory &current, const LegacyMemory &legacy) {
  if (current[21] == 61) return;
  std::copy(legacy.begin(), legacy.end(), current.begin());
  for (int mode=0; mode<3; ++mode)
    current[mode*7+2] = std::max(-60, std::min(0, legacy[mode*7+2]-6));
  current[21] = 61; // Schema and values persist together; repeated boots do not subtract again.
}
inline bool valid(const Settings &s) {
  return s[0]>=0 && s[0]<=3 && s[1]>=0 && s[1]<=3 &&
      s[2]>=-60 && s[2]<=0 && s[2]%3==0 &&
      s[3]>=-60 && s[3]<=0 && s[3]%3==0 &&
      s[4]>=6000 && s[4]<=10000 && s[4]%250==0 &&
      s[5]>=-24 && s[5]<=0 && s[6]>=-24 && s[6]<=0;
}
inline Settings read(const Memory &m, int mode) {
  Settings s{};
  if (mode<0 || mode>2) { s[0]=-1; return s; }
  std::copy_n(m.begin()+mode*7, 7, s.begin());
  return s;
}
inline bool save(Memory &m, int mode, const Settings &s) {
  if (mode<0 || mode>2 || !valid(s)) return false;
  std::copy(s.begin(), s.end(), m.begin()+mode*7);
  return true;
}
inline Settings effective(Settings s, int mode) {
  if (mode==0) { s[0]=1; s[1]=0; }
  if (mode==1) s[0]=2;
  if (s[0]==2) s[1]=1; // Preserve the accepted mono AEC reference.
  if (s[0]==3) s[1]=0;
  return s;
}
inline const char *route(int n) {
  const char *names[]={"OFF", "Internal stereo", "External speaker", "LINE OUT stereo"};
  return n>=0 && n<4 ? names[n] : "OFF";
}
inline const char *channel(int n) {
  const char *names[]={"Stereo", "Both mono", "Left", "Right"};
  return n>=0 && n<4 ? names[n] : "Stereo";
}
inline const char *name(int n) {
  const char *names[]={"Voice Compact", "Full Speaker", "Custom"};
  return n>=0 && n<3 ? names[n] : "Not applied";
}
}
