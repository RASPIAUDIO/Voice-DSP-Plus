/* Tile 0 only: apply a cache calculated on tile 1 using the original equations.
 * No fixed-preset acceptance restriction. No general-purpose libm on tile 0. */
#include "vdsp_usb_filter.h"
#include <string.h>
static const uint8_t usb_identity[10] = {0x7f,0xff,0,0,0,0,0,0,0,0};

int vdsp_v2_equalizer_generate(float bass,float treble,vdsp_v2_equalizer_coefficients *out)
{
    if(!out || bass!=0 || treble!=0) return -2;
    memset(out,0,sizeof(*out));
    for(unsigned f=0;f<3;f++)memcpy(out->coefficients[f],usb_identity,10);
    return 0;
}
int vdsp_v2_equalizer_prepare_pair(float bl,float tl,float br,float tr,int level,vdsp_v2_equalizer_pair *out)
{
    if(!out || level < -127 || level>0) return -2;
    if(vdsp_v2_equalizer_generate(bl,tl,&out->channels[0]) || vdsp_v2_equalizer_generate(br,tr,&out->channels[1])) return -2;
    out->volume_half_db[0]=out->volume_half_db[1]=level;return 0;
}
int vdsp_v2_crossover_prepare(const vdsp_v2_crossover_config *c,int level,vdsp_v2_equalizer_pair *out)
{
    if(!c || !out || c->mute_mask || level < -127 || level>0)return -2;
    vdsp_v2_crossover_config accepted;
    vdsp_v2_equalizer_pair pair;
    vdsp_v2_filter_get(&accepted,&pair);
    if(c->cutoff_hz!=accepted.cutoff_hz || c->left_trim_db!=accepted.left_trim_db || c->right_trim_db!=accepted.right_trim_db)return -2;
    pair.volume_half_db[0]=pair.volume_half_db[1]=level;*out=pair;return 0;
}
int vdsp_v2_crossover_generate(const vdsp_v2_crossover_config *c,vdsp_v2_equalizer_pair *out)
{return vdsp_v2_crossover_prepare(c,0,out);}
