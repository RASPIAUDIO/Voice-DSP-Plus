#ifndef VDSP_USB_DFU_GUARD_H
#define VDSP_USB_DFU_GUARD_H
#include <stdint.h>
#include <stddef.h>
/* V2 W25Q32, 4 KiB sectors, 256-byte pages. The running factory partition
 * remains untouched; no fallback to address zero when metadata is absent. */
static inline unsigned vdsp_usb_dfu_check(unsigned alt, unsigned block, unsigned length,
                                        uint32_t base, uint32_t end, size_t written)
{
    if (alt != 1) return 3;
    if (length != 256) return 8;
    if (!base || base % 4096 || end > 0x400000u || end <= base || end % 4096) return 8;
    if (block == 0) written = 0;
    if (written != (size_t)block * 256 || written > end-base || end-base-written < length) return 8;
    return 0;
}
#endif
