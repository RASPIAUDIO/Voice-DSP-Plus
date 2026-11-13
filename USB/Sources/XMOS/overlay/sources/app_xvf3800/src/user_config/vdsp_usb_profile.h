#ifndef VDSP_USB_PROFILE_H
#define VDSP_USB_PROFILE_H
#include <stdint.h>
extern volatile uint32_t vdsp_usb_profile_requested;
extern volatile uint32_t vdsp_usb_profile_applied;
/* Executed exclusively inside the corresponding SHF control window. */
static inline float vdsp_usb_far_gain(unsigned profile) { return profile == 4 ? -24.0f : 0.0f; }
#endif
