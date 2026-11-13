/* Only the two USB presets are exposed. The register writer below remains
 * identical to the accepted driver; general-purpose libm is not linked. */
#include "vdsp_usb_fixed_filter.h"
#include <string.h>
static const uint8_t usb_identity[10] = {0x7f,0xff,0,0,0,0,0,0,0,0};

int vdsp_v2_equalizer_generate(float bass, float treble,
                             vdsp_v2_equalizer_coefficients *out)
{
    if (!out || bass != 0 || treble != 0) return -2;
    memset(out,0,sizeof(*out));
    for (unsigned f=0;f<3;f++) memcpy(out->coefficients[f],usb_identity,10);
    return 0;
}
int vdsp_v2_equalizer_prepare_pair(float bl,float tl,float br,float tr,int level,
                                 vdsp_v2_equalizer_pair *out)
{
    if (!out || level < -127 || level > 0) return -2;
    if (vdsp_v2_equalizer_generate(bl,tl,&out->channels[0]) ||
        vdsp_v2_equalizer_generate(br,tr,&out->channels[1])) return -2;
    out->volume_half_db[0]=out->volume_half_db[1]=level;
    return 0;
}
int vdsp_v2_crossover_prepare(const vdsp_v2_crossover_config *c,int level,
                            vdsp_v2_equalizer_pair *out)
{
    if (!c || !out || c->cutoff_hz != 9500 || c->left_trim_db != 0 ||
        c->right_trim_db != -6 || c->mute_mask || level < -127 || level > 0) return -2;
    memset(out,0,sizeof(*out));
    for (unsigned ch=0;ch<2;ch++) {
        memcpy(out->channels[ch].coefficients,usb_full_speaker_coefficients[ch],30);
        out->volume_half_db[ch]=level;
    }
    return 0;
}
int vdsp_v2_crossover_generate(const vdsp_v2_crossover_config *c,vdsp_v2_equalizer_pair *out)
{ return vdsp_v2_crossover_prepare(c,0,out); }
