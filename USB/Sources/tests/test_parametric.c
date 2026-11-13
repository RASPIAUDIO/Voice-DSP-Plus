#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "vdsp_usb_filter.h"

static vdsp_v2_crossover_config cached_config;
static vdsp_v2_equalizer_pair cached_pair;
uint32_t vdsp_v2_filter_get(vdsp_v2_crossover_config *c,vdsp_v2_equalizer_pair *p)
{if(c)*c=cached_config;if(p)*p=cached_pair;return 7;}

/* Rename the tile 0 application helpers; link the original tile 1 math too. */
#define vdsp_v2_equalizer_generate cache_equalizer_generate
#define vdsp_v2_equalizer_prepare_pair cache_equalizer_prepare_pair
#define vdsp_v2_crossover_prepare cache_crossover_prepare
#define vdsp_v2_crossover_generate cache_crossover_generate
#include "vdsp_usb_parametric_eq.inc"
#undef vdsp_v2_equalizer_generate
#undef vdsp_v2_equalizer_prepare_pair
#undef vdsp_v2_crossover_prepare
#undef vdsp_v2_crossover_generate

int main(void)
{
    unsigned checked=0;
    for(int hz=6000;hz<=10000;hz+=500)
    for(int left=0;left>=-24;left-=6)
    for(int right=0;right>=-24;right-=6){
        float params[3]={(float)hz,(float)left,(float)right};
        assert(!vdsp_usb_filter_decode((const uint8_t*)params,&cached_config));
        assert(!vdsp_v2_crossover_generate(&cached_config,&cached_pair));
        for(int level=-127;level<=0;level++){
            vdsp_v2_equalizer_pair actual,expected;
            assert(!cache_crossover_prepare(&cached_config,level,&actual));
            assert(!vdsp_v2_crossover_prepare(&cached_config,level,&expected));
            assert(!memcmp(&actual,&expected,sizeof(actual)));
            ++checked;
        }
    }
    vdsp_v2_equalizer_pair actual,before;
    memset(&actual,0xa5,sizeof(actual));before=actual;
    vdsp_v2_crossover_config stale=cached_config;stale.cutoff_hz=8000;
    assert(cache_crossover_prepare(&stale,0,&actual)==-2);
    assert(!memcmp(&before,&actual,sizeof(actual)));
    float invalid[3]={NAN,0,0};
    assert(vdsp_usb_filter_decode((const uint8_t*)invalid,&stale)==-2);
    for(int level=-127;level<=0;level++){
        vdsp_v2_equalizer_pair expected;
        assert(!cache_equalizer_prepare_pair(0,0,0,0,level,&actual));
        assert(!vdsp_v2_equalizer_prepare_pair(0,0,0,0,level,&expected));
        assert(!memcmp(&actual,&expected,sizeof(actual)));
    }
    printf("PASS: %u parametric cache/math comparisons, flat unity, stale rejection\n",checked);
    return 0;
}
