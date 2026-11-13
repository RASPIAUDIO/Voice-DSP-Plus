#ifndef VDSP_USB_FILTER_H
#define VDSP_USB_FILTER_H
#include "voicedsp_v2_equalizer.h"
#include <stdint.h>
#include <string.h>

#define VDSP_USB_FILTER_PARAMS_SIZE 12u
#define VDSP_USB_FILTER_STATUS_SIZE 16u
#define VDSP_USB_FILTER_PACKET_SIZE 72u

/* Three IEEE754 LE floats: cutoff Hz, left trim dB, right trim dB.
 * XMOS and supported host tools are little endian. No coefficients accepted
 * from the external USB host: tile 1 calculates them with the accepted math. */
static inline int vdsp_usb_filter_decode(const uint8_t *p,vdsp_v2_crossover_config *c)
{
    if(!p || !c) return -2;
    memcpy(&c->cutoff_hz,p,4);memcpy(&c->left_trim_db,p+4,4);memcpy(&c->right_trim_db,p+8,4);
    c->mute_mask=0;
    if(!(c->cutoff_hz>=6000 && c->cutoff_hz<=10000 &&
         c->left_trim_db>=-24 && c->left_trim_db<=0 &&
         c->right_trim_db>=-24 && c->right_trim_db<=0)) return -2;
    return 0;
}
/* Serial application-servicer API, tile 1. Read returns params + filter epoch. */
int vdsp_v2_filter_exchange(const uint8_t *parameters,size_t length,uint8_t *status);
/* Sole runtime consumer on tile 0; coherent snapshot of configuration/cache. */
uint32_t vdsp_v2_filter_get(vdsp_v2_crossover_config *,vdsp_v2_equalizer_pair *);
#endif
