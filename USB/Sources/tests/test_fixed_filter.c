#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "voicedsp_v2_equalizer.h"
#define vdsp_v2_equalizer_generate usb_fixed_equalizer_generate
#define vdsp_v2_equalizer_prepare_pair usb_fixed_equalizer_prepare_pair
#define vdsp_v2_crossover_prepare usb_fixed_crossover_prepare
#define vdsp_v2_crossover_generate usb_fixed_crossover_generate
#include "overlay/vdsp_usb_fixed_eq.inc"
#undef vdsp_v2_equalizer_generate
#undef vdsp_v2_equalizer_prepare_pair
#undef vdsp_v2_crossover_prepare
#undef vdsp_v2_crossover_generate

static void same(const vdsp_v2_equalizer_pair *a,const vdsp_v2_equalizer_pair *b)
{
    for (unsigned ch=0;ch<2;ch++) {
        assert(memcmp(a->channels[ch].coefficients,b->channels[ch].coefficients,30)==0);
        assert(a->channels[ch].gain_half_db==b->channels[ch].gain_half_db);
        assert(a->volume_half_db[ch]==b->volume_half_db[ch]);
    }
}
int main(void)
{
    const vdsp_v2_crossover_config config={9500,0,-6,0};
    for (int level=-127;level<=0;level++) {
        vdsp_v2_equalizer_pair reference, fixed;
        assert(!vdsp_v2_equalizer_prepare_pair(0,0,0,0,level,&reference));
        assert(!usb_fixed_equalizer_prepare_pair(0,0,0,0,level,&fixed));
        same(&reference,&fixed);
        assert(!vdsp_v2_crossover_prepare(&config,level,&reference));
        assert(!usb_fixed_crossover_prepare(&config,level,&fixed));
        same(&reference,&fixed);
    }
    puts("PASS: 256 preset/level pairs, all 60 coefficient bytes and DAC gains identical to accepted C code");
    return 0;
}
