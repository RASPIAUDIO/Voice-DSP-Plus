#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "overlay/vdsp_usb_nv.h"
typedef int rtos_qspi_flash_t;
static int flash_context;
static uint8_t memory[8192];
static int budget=-1;
static unsigned operations;
static uint32_t nv_seq;
static rtos_qspi_flash_t *get_qspi_flash_ctx(void){return &flash_context;}
static int *get_dfu_image_ctx(void){return &flash_context;}
static unsigned rtos_dfu_image_get_data_partition_addr(int *p){(void)p;return 0x200000;}
static unsigned rtos_qspi_flash_size_get(int *p){(void)p;return 0x400000;}
static unsigned rtos_qspi_flash_sector_size_get(int *p){(void)p;return 4096;}
static unsigned rtos_qspi_flash_page_size_get(int *p){(void)p;return 256;}
static void rtos_qspi_flash_lock(int *p){(void)p;}
static void rtos_qspi_flash_unlock(int *p){(void)p;}
static void bounds(unsigned a,unsigned n){assert(a>=0x3fe000 && a+n<=0x400000);}
static bool writable(void){if(budget==0)return false;if(budget>0)--budget;return true;}
static void rtos_qspi_flash_read(int *p,uint8_t *d,unsigned a,unsigned n)
{(void)p;bounds(a,n);memcpy(d,memory+a-0x3fe000,n);}
static void rtos_qspi_flash_write(int *p,const uint8_t *d,unsigned a,unsigned n)
{(void)p;bounds(a,n);assert(a/256==(a+n-1)/256);++operations;for(unsigned i=0;i<n;i++)if(writable())memory[a-0x3fe000+i]&=d[i];}
static void rtos_qspi_flash_erase(int *p,unsigned a,unsigned n)
{(void)p;bounds(a,n);assert(!(a%4096)&&n==4096);++operations;for(unsigned i=0;i<n;i++)if(writable())memory[a-0x3fe000+i]=255;}
#define USB_NV_TEST
#include "overlay/vdsp_usb_persist.inc"
static void reboot(void){nv_slot=-1;nv_seq=0;memset(nv_saved,0,16);}
int main(void)
{
    memset(memory,255,sizeof(memory));uint8_t a[16]={2,4,60,0},b[16];
    float params[3]={9500,0,-6};memcpy(a+4,params,12);memcpy(b,a,16);b[1]=3;b[2]=24;
    assert(nv_load()==0);assert(!nv_save(a));reboot();assert(nv_load()==1&&!memcmp(a,nv_saved,16));
    uint8_t baseline[8192];memcpy(baseline,memory,sizeof(memory));
    for(int cut=0;cut<=260;cut++) {
        memcpy(memory,baseline,sizeof(memory));reboot();assert(nv_load()==1);
        budget=cut;(void)nv_save(b);budget=-1;reboot();assert(nv_load()==1);
        assert(!memcmp(nv_saved,a,16)||!memcmp(nv_saved,b,16));
        assert(!nv_save(b));reboot();assert(nv_load()==1&&!memcmp(nv_saved,b,16));
    }
    memcpy(memory,baseline,sizeof(memory));reboot();assert(nv_load()==1);
    for(unsigned i=0;i<31;i++){a[2]=(uint8_t)(i+1);assert(!nv_save(a));}
    memcpy(baseline,memory,sizeof(memory));
    for(int cut=0;cut<=4356;cut+=17) {
        memcpy(memory,baseline,sizeof(memory));reboot();assert(nv_load()==1);
        budget=cut;(void)nv_save(b);budget=-1;reboot();assert(nv_load()==1);
        assert(!memcmp(nv_saved,a,16)||!memcmp(nv_saved,b,16));
    }
    memset(memory,0x55,sizeof(memory));reboot();operations=0;
    assert(nv_load()==-1 && operations==0);
    puts("PASS: actual NVM journal, reboot, 261 torn page/commit points, erase rollover cuts, foreign-data refusal");
    return 0;
}
