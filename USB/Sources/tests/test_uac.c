#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "overlay/vdsp_usb_volume.h"
#define USB_BOOT_TEST
#include "overlay/vdsp_usb_boot.h"
#define TU_U16_LOW(v) ((v)&255)
#define TU_U16_HIGH(v) ((v)>>8)
#define AUDIO_CS_REQ_CUR 1
#define AUDIO_CS_REQ_RANGE 2
#define AUDIO_FU_CTRL_MUTE 1
#define AUDIO_FU_CTRL_VOLUME 2
#define ITF_NUM_AUDIO_CONTROL 0
#define UAC2_ENTITY_SPK_FEATURE_UNIT 0x12
typedef struct {uint8_t bRequest;uint16_t wValue,wIndex,wLength;} tusb_control_request_t;
typedef struct {uint8_t bInfo,bAttribute;uint16_t wValue,wIndex;} audio_interrupt_data_t;
static uint8_t command[4]={2,3,36,2}, response[16];
static unsigned response_size,notifications;
static bool mounted=true,busy=false;
static audio_interrupt_data_t last;
static vdsp_usb_boot_state boot={.restored=1};
static uint32_t now;
int vdsp_usb_volume_set(unsigned selector,int16_t value)
{return vdsp_usb_volume_apply(command,selector,value);}
static int vdsp_v2_command_exchange(const uint8_t *p,unsigned n,uint8_t *s)
{(void)p;(void)n;memset(s,0,56);s[40]=command[3];s[47]=command[2];return 0;}
static int vdsp_usb_boot_volume(unsigned selector,int16_t value)
{
    uint8_t check[4]={2,4,60,0};if(vdsp_usb_volume_apply(check,selector,value))return -2;
    return vdsp_usb_boot_set(&boot,now,selector,value)?vdsp_usb_volume_set(selector,value):0;
}
static int vdsp_usb_boot_read(uint8_t *status)
{
    vdsp_v2_command_exchange(NULL,0,status);vdsp_usb_boot_get(&boot,now);
    if(!boot.restored){status[47]=boot.attenuation;status[40]&=2u;}return 0;
}
static int vdsp_usb_boot_notify(void)
{vdsp_usb_boot_poll(&boot,now);return !boot.locked;}
static bool tud_audio_buffer_and_schedule_control_xfer(uint8_t port,const tusb_control_request_t *r,void *data,unsigned n)
{(void)port;(void)r;assert(n<=sizeof(response));memcpy(response,data,n);response_size=n;return true;}
static bool tud_mounted(void){return mounted;}
static bool tud_audio_int_write(const audio_interrupt_data_t *p)
{if(busy)return false;last=*p;notifications++;return true;}
#define USB_UAC_TEST
#include "overlay/vdsp_usb_uac.inc"
int main(void)
{
    tusb_control_request_t r={1,0x200,0x1200,2};
    for(int a=0;a<=120;a++) {
        int16_t q=vdsp_usb_volume_q8(a);
        assert(vdsp_usb_uac_set(&r,(uint8_t*)&q));
        assert(command[1]==3&&command[2]==a&&command[3]==2);
        assert(vdsp_usb_uac_get(0,&r)&&response_size==2&&!memcmp(response,&q,2));
    }
    int16_t bads[]={1,-1,-15361,32767,-32768};
    for(unsigned i=0;i<sizeof(bads)/sizeof(*bads);i++)assert(!vdsp_usb_uac_set(&r,(uint8_t*)&bads[i]));
    r.wValue=0x201;assert(!vdsp_usb_uac_get(0,&r));assert(!vdsp_usb_uac_set(&r,(uint8_t*)bads));
    r.wValue=0x200;r.bRequest=2;
    assert(vdsp_usb_uac_get(0,&r)&&response_size==8);
    int16_t expected[]={1,-15360,0,128};assert(!memcmp(response,expected,8));
    r.bRequest=1;r.wValue=0x100;r.wLength=1;uint8_t mute=1;
    assert(vdsp_usb_uac_set(&r,&mute)&&command[3]==3);
    assert(vdsp_usb_uac_get(0,&r)&&response_size==1&&response[0]==1);
    mute=2;assert(!vdsp_usb_uac_set(&r,&mute));
    uint8_t status[56];vdsp_v2_command_exchange(NULL,0,status);
    busy=true;vdsp_usb_uac_notify(status);assert(!notifications);
    busy=false;vdsp_usb_uac_notify(status);assert(notifications==1&&last.wValue==0x200);
    assert(last.wIndex==0x1200&&last.bInfo==0&&last.bAttribute==1);
    vdsp_usb_uac_notify(status);assert(notifications==2&&last.wValue==0x100);
    vdsp_usb_uac_notify(status);assert(notifications==2);
    status[47]=54;vdsp_usb_uac_notify(status);assert(notifications==3&&last.wValue==0x200);
    mounted=false;vdsp_usb_uac_notify(status);mounted=true;vdsp_usb_uac_notify(status);assert(notifications==4);
    uint8_t saved[4]={2,3,20,0};vdsp_usb_boot_init(&boot,saved,31);
    command[1]=3;command[2]=120;command[3]=0;now=100;
    r.wValue=0x200;r.wLength=2;
    assert(vdsp_usb_uac_get(0,&r));int16_t restored=-10*256;
    assert(!memcmp(response,&restored,2)); /* Advertise saved, not ramp level. */
    int16_t zero=0;assert(vdsp_usb_uac_set(&r,(uint8_t*)&zero));
    assert(command[2]==120 && boot.suppressed==1);
    vdsp_usb_uac_notify(status);assert(notifications==4);
    command[2]=20;boot.restored=1;now=2099;
    vdsp_v2_command_exchange(NULL,0,status);vdsp_usb_uac_notify(status);
    assert(boot.locked && notifications==4);
    now=2100;vdsp_usb_uac_notify(status);assert(!boot.locked && notifications==5);
    assert(vdsp_usb_uac_get(0,&r) && !memcmp(response,&restored,2));
    int16_t later=-24*256;assert(vdsp_usb_uac_set(&r,(uint8_t*)&later));assert(command[2]==48);
    assert(vdsp_usb_uac_set(&r,(uint8_t*)&zero) && command[2]==0); /* No permanent ceiling. */
    vdsp_usb_boot_init(&boot,saved,32);now=0xfffffc00u;
    vdsp_usb_boot_get(&boot,now);boot.restored=1;
    assert(!vdsp_usb_boot_set(&boot,now+1800u,2,0));
    vdsp_usb_boot_poll(&boot,now+2100u);assert(boot.locked);
    vdsp_usb_boot_poll(&boot,now+2300u);assert(!boot.locked);
    vdsp_usb_boot_init(&boot,saved,33);vdsp_usb_boot_get(&boot,100);boot.restored=1;
    assert(!vdsp_usb_boot_set(&boot,5099,2,0));
    vdsp_usb_boot_poll(&boot,5099);assert(boot.locked);
    vdsp_usb_boot_poll(&boot,5100);assert(!boot.locked); /* Cannot lock controls indefinitely. */
    puts("PASS: saved boot level advertised, enumeration SET=0 suppressed, post-boot Windows controls intact, quiet window/wrap");
    puts("PASS: actual UAC callbacks, 121 levels, range, master-only, mute, endpoint-specific notifications/retry");
    return 0;
}
