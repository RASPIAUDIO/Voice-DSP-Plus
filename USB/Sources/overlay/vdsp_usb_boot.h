#ifndef VDSP_USB_BOOT_H
#define VDSP_USB_BOOT_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
typedef struct {
    uint8_t locked, restored, attenuation, route;
    uint32_t sequence, first_host_ms, last_set_ms, gets, sets, suppressed;
    int16_t last_set_q8;
    uint8_t last_selector, reserved;
} vdsp_usb_boot_state;
static inline void vdsp_usb_boot_init(vdsp_usb_boot_state *s, const uint8_t command[4], uint32_t seq)
{ memset(s,0,sizeof(*s));s->locked=1;s->attenuation=command[2];s->route=command[1];s->sequence=seq; }
static inline void vdsp_usb_boot_get(vdsp_usb_boot_state *s,uint32_t now)
{ if(!s->gets && !s->sets)s->first_host_ms=now;++s->gets; }
static inline bool vdsp_usb_boot_set(vdsp_usb_boot_state *s,uint32_t now,unsigned selector,int16_t value)
{
    if(!s->gets && !s->sets)s->first_host_ms=now;
    s->last_set_ms=now;s->last_set_q8=value;s->last_selector=(uint8_t)selector;++s->sets;
    if(s->locked){++s->suppressed;return false;}return true;
}
/* Enumeration commands are acknowledged but cannot replace the saved gain.
 * After restoration + a short quiet enumeration window, Windows owns normal
 * controls again; the normal UAC interrupt then advertises the real level. */
static inline void vdsp_usb_boot_poll(vdsp_usb_boot_state *s,uint32_t now)
{
    if(s->locked && s->restored && (s->gets || s->sets) &&
       (uint32_t)(now-s->first_host_ms)>=2000u &&
       ((uint32_t)(now-s->last_set_ms)>=500u ||
        (uint32_t)(now-s->first_host_ms)>=5000u))s->locked=0;
}
#ifndef USB_BOOT_TEST
int vdsp_usb_boot_read(uint8_t *status);
int vdsp_usb_boot_volume(unsigned selector,int16_t value);
int vdsp_usb_boot_notify(void);
int vdsp_usb_volume_trace(uint8_t *out);
int vdsp_usb_command_accept_source(const uint8_t command[4],unsigned source);
#endif
#endif
