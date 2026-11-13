#ifndef VDSP_LINUX_TRANSPORT_H
#define VDSP_LINUX_TRANSPORT_H
#include "voicedsp_v2_host.h"
#include <signal.h>
typedef struct {
    int fd;
    bool external_clocks;
    volatile sig_atomic_t *cancel;
} vdsp_linux;
int vdsp_linux_open(vdsp_linux *,const char *,unsigned timeout_ms);
void vdsp_linux_close(vdsp_linux *);
vdsp_v2_host_ops vdsp_linux_ops(vdsp_linux *);
#endif
