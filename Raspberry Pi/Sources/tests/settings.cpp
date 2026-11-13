#include "settings.h"
#include <cassert>
int main() {
  assert(aec_profile_for_route(VDSP_AUDIO_DAC_SPEAKERS)==4);
  assert(aec_profile_for_route(VDSP_AUDIO_HT326)==4);
  assert(aec_profile_for_route(VDSP_AUDIO_DAC_SPEAKERS,true)==6);
  assert(aec_profile_for_route(VDSP_AUDIO_HT326,true)==4);
  Settings s;
  assert(command("output full",s) && s.full && s.route==VDSP_AUDIO_HT326);
  assert(command("volume 0",s) && s.attenuation==0);
  assert(!command("volume 1",s) && s.attenuation==0);
  assert(!command("volume -3x",s) && s.attenuation==0);
  assert(!command("output internal extra",s) && s.full);
  assert(command("output internal",s) && !s.full);
  GestureActions g;
  assert(g.apply(16,100,s) && s.muted);
  assert(!g.apply(1,1099,s) && s.muted);
  assert(g.apply(1,1100,s) && !s.muted && s.attenuation==0);
  assert(g.apply(1,2100,s) && s.attenuation==6);
  assert(command("gestures off",s) && !g.apply(16,4000,s));
  assert(command("mic-mute on",s) && s.mic_muted && !s.muted);
  assert(command("crossover 8000 -2 -8",s));
  assert(s.crossover.cutoff_hz==8000 && s.crossover.left_trim_db==-2 && s.crossover.right_trim_db==-8);
  assert(!command("crossover 2000 0 0",s) && s.crossover.cutoff_hz==8000);
  assert(!command("crossover 8000 3 0",s));
  assert(!command("crossover nan 0 0",s));
  assert(!command("crossover 9000 -2 -8 extra",s));
}
