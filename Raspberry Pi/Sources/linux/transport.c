#define _POSIX_C_SOURCE 200809L
#include "transport.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

int vdsp_linux_open(vdsp_linux *s,const char *path,unsigned timeout_ms)
{
    if(!s || !path || timeout_ms<10 || timeout_ms>1000) return -1;
    s->fd=open(path,O_RDWR|O_CLOEXEC);
    if(s->fd<0) return -1;
    unsigned long funcs=0;
    /* flock coordinates cooperating processes only. Timeout/retry settings are
     * adapter-wide: this program requires exclusive ownership of the whole bus.
     */
    if(flock(s->fd,LOCK_EX|LOCK_NB)<0 || ioctl(s->fd,I2C_FUNCS,&funcs)<0 ||
       !(funcs&I2C_FUNC_I2C) || ioctl(s->fd,I2C_RETRIES,0)<0 ||
       ioctl(s->fd,I2C_TIMEOUT,(timeout_ms+9)/10)<0) {
        close(s->fd); s->fd=-1; return -1;
    }
    return 0;
}
void vdsp_linux_close(vdsp_linux *s)
{ if(s && s->fd>=0) { close(s->fd); s->fd=-1; } }
static int linux_transfer(void *context,uint8_t addr,const uint8_t *tx,size_t nt,uint8_t *rx,size_t nr)
{
    vdsp_linux *s=context;
    struct i2c_msg msgs[2]; unsigned count=0;
    if(s->fd<0 || addr>0x7f || nt>256 || nr>256 || (!nt && !nr) || (nt && !tx) || (nr && !rx)) return -1;
    if(nt) msgs[count++]=(struct i2c_msg){.addr=addr,.flags=0,.len=(__u16)nt,.buf=(uint8_t *)tx};
    if(nr) msgs[count++]=(struct i2c_msg){.addr=addr,.flags=I2C_M_RD,.len=(__u16)nr,.buf=rx};
    struct i2c_rdwr_ioctl_data request={.msgs=msgs,.nmsgs=count};
    /* No EINTR retry: a write may already have reached the device. */
    return ioctl(s->fd,I2C_RDWR,&request)==(int)count ? 0 : -1;
}
static uint32_t linux_now(void *context)
{
    (void)context; struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)) return 0; /* Stationary time fails liveness. */
    return (uint32_t)((uint64_t)t.tv_sec*1000+(unsigned long)t.tv_nsec/1000000);
}
static void linux_wait(void *context,unsigned ms)
{
    (void)context;
    struct timespec t={.tv_sec=ms/1000,.tv_nsec=(long)(ms%1000)*1000000};
    /* Finish short driver settling intervals even if shutdown was requested;
     * guards and outer loops observe cancellation before enabling/unmuting.
     */
    while(nanosleep(&t,&t) && errno==EINTR) { }
}
static int linux_clocks(void *context,unsigned rate)
{
    vdsp_linux *s=context;
    /* ALSA is a separate clock owner. This declaration does not count as clock
     * evidence: the service still checks the XMOS frame/tick counters.
     */
    return s->external_clocks && (rate==16000 || rate==48000) ? 0 : -1;
}
static bool linux_cancel(void *context)
{ vdsp_linux *s=context; return s->cancel && *s->cancel; }
vdsp_v2_host_ops vdsp_linux_ops(vdsp_linux *s)
{ return (vdsp_v2_host_ops){s,linux_transfer,linux_now,linux_wait,linux_clocks,linux_cancel}; }
