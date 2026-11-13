#ifndef VDSP_USB_NV_H
#define VDSP_USB_NV_H
#include <stdint.h>
#include <string.h>
#include "vdsp_usb_filter.h"
#define USB_NV_MAGIC 0x31534456u
#define USB_NV_COMMIT 0x434f4d54u
typedef struct { uint32_t magic, sequence; uint8_t settings[16]; uint32_t crc, commit; } usb_nv_record;
static inline uint32_t usb_nv_crc(const void *data, unsigned size)
{
    const uint8_t *p=data; uint32_t c=~0u;
    while(size--) { c^=*p++; for(unsigned b=0;b<8;b++) c=(c>>1)^((0u-(c&1u))&0xedb88320u); }
    return ~c;
}
static inline int usb_nv_settings_valid(const uint8_t p[16])
{
    vdsp_v2_crossover_config c;
    return p[0]==2 && p[1]>=1 && p[1]<=4 && p[2]<=120 && !(p[3]&~2u) && !vdsp_usb_filter_decode(p+4,&c);
}
static inline int usb_nv_valid(const usb_nv_record *r)
{ return r->magic==USB_NV_MAGIC && r->commit==USB_NV_COMMIT && r->crc==usb_nv_crc(r,24) && usb_nv_settings_valid(r->settings); }
static inline int usb_nv_newer(uint32_t a,uint32_t b)
{ return (int32_t)(a-b)>0; }
static inline void usb_nv_pack(usb_nv_record *r,uint32_t seq,const uint8_t settings[16])
{
    memset(r,0xff,sizeof(*r)); r->magic=USB_NV_MAGIC;r->sequence=seq;
    memcpy(r->settings,settings,16);r->crc=usb_nv_crc(r,24);
}
#endif
