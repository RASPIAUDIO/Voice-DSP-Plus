#ifndef VDSP_USB_LINK_H
#define VDSP_USB_LINK_H
#include <stdint.h>
#define VDSP_USB_LINK_PORT 7
typedef struct { uint32_t sequence, profile, ui; } vdsp_usb_request;
typedef struct { uint32_t sequence, frames, profile; } vdsp_usb_reply;
int vdsp_usb_link(unsigned profile, unsigned ui, vdsp_usb_reply *reply);
void vdsp_v2_runtime_start(void);
#endif
