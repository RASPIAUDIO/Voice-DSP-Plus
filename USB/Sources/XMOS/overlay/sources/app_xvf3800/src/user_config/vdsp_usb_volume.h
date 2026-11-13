#ifndef VDSP_USB_VOLUME_H
#define VDSP_USB_VOLUME_H
#include <stdint.h>
/* UAC2 signed Q8 dB <-> existing half-dB hardware attenuation. */
static inline int vdsp_usb_volume_apply(uint8_t command[4], unsigned selector, int value)
{
    if(selector==1) {
        if(value!=0 && value!=1)return -2;
        command[3]=(command[3]&2u)|(unsigned)value;
    } else if(selector==2) {
        if(value < -60*256 || value>0 || value%128)return -2;
        command[2]=(uint8_t)(-value/128);
    } else return -2;
    return 0;
}
static inline int16_t vdsp_usb_volume_q8(unsigned attenuation)
{ return (int16_t)(-(int)attenuation*128); }
#endif
