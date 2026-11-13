#pragma once
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include "voicedsp_v2_host.h"
#include "voicedsp_v2_equalizer.h"

struct Settings {
  vdsp_v2_audio_route route{VDSP_AUDIO_OFF};
  unsigned attenuation{60}; // Half dB, explicit -30 dB startup.
  bool full{}, muted{}, mic_muted{}, gestures{true};
  vdsp_v2_crossover_config crossover{9500,0,-6,0};
};

inline unsigned aec_profile_for_route(vdsp_v2_audio_route route, bool legacy_internal=false) {
  // Pi internal double-talk requires the same voice-preserving policy as external.
  return legacy_internal && route==VDSP_AUDIO_DAC_SPEAKERS ? 6u : 4u;
}

// Parse into a copy: malformed commands must never partially alter live settings.
inline bool command(const std::string &line, Settings &state) {
  Settings next=state;
  std::istringstream in(line);
  std::string name, value, extra;
  if (!(in>>name)) return false;
  if(name=="crossover") {
    auto &f=next.crossover;
    if(!(in>>f.cutoff_hz>>f.left_trim_db>>f.right_trim_db) || (in>>extra)) return false;
    vdsp_v2_equalizer_pair check{};
    if(vdsp_v2_crossover_generate(&f,&check)) return false;
    state=next;return true;
  }
  if (!(in>>value) || (in>>extra)) return false;
  if(name=="output") {
    next.full=false;
    if(value=="off") next.route=VDSP_AUDIO_OFF;
    else if(value=="internal") next.route=VDSP_AUDIO_DAC_SPEAKERS;
    else if(value=="external") next.route=VDSP_AUDIO_HT326;
    else if(value=="full") { next.route=VDSP_AUDIO_HT326;next.full=true; }
    else if(value=="line") next.route=VDSP_AUDIO_LINE;
    else return false;
  } else if(name=="volume") {
    try {
      size_t used=0; int db=std::stoi(value,&used);
      if(used!=value.size() || db < -60 || db > 0) return false;
      next.attenuation=unsigned(-db*2);
    } catch(...) { return false; }
  } else if(name=="mute" || name=="mic-mute" || name=="gestures") {
    if(value!="on" && value!="off") return false;
    bool enabled=value=="on";
    if(name=="mute") next.muted=enabled;
    else if(name=="mic-mute") next.mic_muted=enabled;
    else next.gestures=enabled;
  } else return false;
  state=next;
  return true;
}

struct GestureActions {
  bool have_toggle{};
  uint32_t toggled_ms{};
  // Physical axes retained from accepted USB/HA checkpoint: right raises level.
  bool apply(unsigned raw, uint32_t now, Settings &s) {
    if(!s.gestures || (have_toggle && uint32_t(now-toggled_ms)<1000)) return false;
    if(raw!=1 && raw!=2 && raw!=4 && raw!=8 && raw!=16 && raw!=32) return false;
    if(s.muted) { s.muted=false;have_toggle=true;toggled_ms=now;return true; }
    if(raw==16) { s.muted=true;have_toggle=true;toggled_ms=now;return true; }
    if(raw==2) { s.attenuation=s.attenuation>6?s.attenuation-6:0;return true; }
    if(raw==1) { s.attenuation=std::min(120u,s.attenuation+6);return true; }
    return false; // Up/down remain events; no music-service-specific API.
  }
};
