#include "overlay/vdsp_usb_state.h"
#include "overlay/vdsp_usb_profile.h"
#include "overlay/vdsp_usb_dfu_guard.h"
#include "overlay/vdsp_usb_nv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define VDSP_BUTTON_LONG_MS 4000
#include "../src/voicedsp_v2_buttons.c"
static void test_buttons(void)
{
    vdsp_v2_buttons b = {0};
    vdsp_v2_button_events e;
    uint8_t c[4] = {2,4,60,3};
    vdsp_v2_buttons_update(&b,0,0,true,0);
    vdsp_v2_buttons_update(&b,40,40,true,0);
    vdsp_v2_buttons_update(&b,60,60,true,1);
    e=vdsp_v2_buttons_update(&b,100,100,true,1);
    assert(vdsp_usb_buttons_apply(e.pressed,e.short_press,e.long_press,c));
    assert(c[2]==54 && c[3]==3);
    for(uint32_t t=120;t<=5100;t+=20) {
        e=vdsp_v2_buttons_update(&b,t,t,true,1);
        assert(!vdsp_usb_buttons_apply(e.pressed,e.short_press,e.long_press,c));
    }
    vdsp_v2_buttons_update(&b,5120,5120,true,0);
    e=vdsp_v2_buttons_update(&b,5160,5160,true,0);
    assert(!vdsp_usb_buttons_apply(e.pressed,e.short_press,e.long_press,c));
    vdsp_v2_buttons_update(&b,5200,5200,true,2);
    vdsp_v2_buttons_update(&b,5240,5240,true,2);
    vdsp_v2_buttons_update(&b,5300,5300,true,0);
    e=vdsp_v2_buttons_update(&b,5340,5340,true,0);
    assert(vdsp_usb_buttons_apply(e.pressed,e.short_press,e.long_press,c));
    assert(c[2]==60 && c[1]==4);
    vdsp_v2_buttons_update(&b,5400,5400,true,2);
    vdsp_v2_buttons_update(&b,5440,5440,true,2);
    for(uint32_t t=5460;t<9440;t+=20) {
        e=vdsp_v2_buttons_update(&b,t,t,true,2);
        assert(!vdsp_usb_buttons_apply(e.pressed,e.short_press,e.long_press,c));
    }
    e=vdsp_v2_buttons_update(&b,9440,9440,true,2);
    assert(vdsp_usb_buttons_apply(e.pressed,e.short_press,e.long_press,c));
    assert(c[1]==3 && c[2]==60 && c[3]==3);
    vdsp_v2_buttons_update(&b,9460,9460,true,0);
    e=vdsp_v2_buttons_update(&b,9500,9500,true,0);
    assert(!vdsp_usb_buttons_apply(e.pressed,e.short_press,e.long_press,c));
    assert(vdsp_usb_buttons_apply(0,0,2,c) && c[1]==4);
    c[2]=0; assert(!vdsp_usb_buttons_apply(1,0,0,c));
    c[2]=120; assert(!vdsp_usb_buttons_apply(0,2,0,c));
    /* An invalid sample cancels a held button rather than creating a click. */
    vdsp_v2_buttons_update(&b,9520,9520,true,2);
    vdsp_v2_buttons_update(&b,9560,9560,true,2);
    e=vdsp_v2_buttons_update(&b,9580,9580,false,0);
    assert(!e.short_press && !e.long_press);
    puts("PASS: buttons, 4s toggle, no repeat, no release click after hold, bounds, mute/filter preserved");
}
static void test_nv(void)
{
    uint8_t settings[16]={2,3,12,2};float params[3]={9500,0,-6};
    memcpy(settings+4,params,12);usb_nv_record r;
    assert(sizeof(r)==32 && usb_nv_settings_valid(settings));
    usb_nv_pack(&r,7,settings);assert(!usb_nv_valid(&r));
    r.commit=USB_NV_COMMIT;assert(usb_nv_valid(&r));
    for(unsigned i=0;i<32;i++) {
        usb_nv_record corrupt=r;((uint8_t*)&corrupt)[i]^=1;assert(!usb_nv_valid(&corrupt));
    }
    /* Torn first programming or commit never wins over the preceding record. */
    for(unsigned n=0;n<32;n++) {
        usb_nv_record torn;memset(&torn,255,sizeof(torn));memcpy(&torn,&r,n);
        assert(!usb_nv_valid(&torn));
    }
    assert(usb_nv_newer(0,UINT32_MAX));assert(!usb_nv_newer(UINT32_MAX,0));
    settings[2]=121;assert(!usb_nv_settings_valid(settings));
    settings[2]=0;settings[3]=1;assert(!usb_nv_settings_valid(settings));
    settings[3]=2;settings[1]=4;assert(usb_nv_settings_valid(settings));
    puts("PASS: settings CRC, incomplete commit, torn records, bounds and sequence wrap");
}
volatile uint32_t vdsp_usb_profile_requested, vdsp_usb_profile_applied;
static uint32_t pp[128], aec[128];
static bool reject;
static void SHF_SetPar_PP(unsigned cmd, const void *v) { if (!reject) memcpy(pp+cmd,v,4); }
static void SHF_GetPar_PP(unsigned cmd, void *v) { memcpy(v,pp+cmd,4); }
static void SHF_SetPar_AEC(unsigned cmd, const void *v) { if (!reject) memcpy(aec+cmd,v,4); }
static void SHF_GetPar_AEC(unsigned cmd, void *v) { memcpy(v,aec+cmd,4); }
#include "overlay/vdsp_usb_profile.inc"
static float value(const uint32_t *p) { float f; memcpy(&f,p,4); return f; }
int main(void)
{
    test_buttons();
    test_nv();
    vdsp_usb_gestures s = {.attenuation=60};
    /* Reproduce the old loop-start timestamp rejecting a newly read event. */
    assert(!vdsp_usb_gesture_fresh(1000, 1006));
    assert(vdsp_usb_gesture_fresh(1008, 1006));
    assert(vdsp_usb_gesture_fresh(1256, 1006));
    assert(!vdsp_usb_gesture_fresh(1257, 1006));
    assert(vdsp_usb_gesture_fresh(5, UINT32_MAX - 5));
    assert(vdsp_usb_gesture(&s,16,100)==1 && s.muted);
    for(unsigned i=1;i<=32;i*=2) assert(vdsp_usb_gesture(&s,i,1099)==0 && s.muted);
    assert(vdsp_usb_gesture(&s,2,1100)==1 && !s.muted && s.attenuation==60);
    assert(vdsp_usb_gesture(&s,2,2100)==2 && s.attenuation==54);
    assert(vdsp_usb_gesture(&s,1,2101)==2 && s.attenuation==60);
    for(unsigned i=0;i<30;i++) vdsp_usb_gesture(&s,2,3000+i);
    assert(s.attenuation==0);
    for(unsigned i=0;i<30;i++) vdsp_usb_gesture(&s,1,4000+i);
    assert(s.attenuation==120);
    assert(!vdsp_usb_gesture(&s,0x100,5000));
    assert(!vdsp_usb_gesture(&s,0x40,5000));
    assert(!vdsp_usb_gesture(&s,3,5000));
    s.have_transition=true; s.transition_ms=UINT32_MAX-500; s.muted=true;
    assert(!vdsp_usb_gesture(&s,32,498));
    assert(vdsp_usb_gesture(&s,32,499)==1 && !s.muted);
    assert(vdsp_usb_far_gain(4)==-24.0f && vdsp_usb_far_gain(6)==0.0f);
    vdsp_usb_profile_requested=4; vdsp_usb_profile_applied=0;
    vdsp_usb_apply_pp();
    assert(vdsp_usb_profile_applied==4);
    assert(value(pp+21)==.10f && value(pp+22)==.51f);
    assert(pp[31]==10 && pp[10]==0 && value(pp+13)==5.0f);
    assert(pp[23]==0 && pp[27]==1);
    vdsp_usb_profile_applied=0; vdsp_usb_apply_aec();
    assert(vdsp_usb_profile_applied==4 && value(aec+5)==-24.0f);
    vdsp_usb_profile_requested=6; vdsp_usb_profile_applied=0;
    vdsp_usb_apply_pp();
    assert(vdsp_usb_profile_applied==6 && pp[10]==1 && pp[23]==1);
    assert(value(pp+21)==.15f && pp[31]==0);
    vdsp_usb_profile_applied=0; vdsp_usb_apply_aec();
    assert(vdsp_usb_profile_applied==6 && value(aec+5)==0.0f);
    reject=true; vdsp_usb_profile_requested=4; vdsp_usb_profile_applied=0;
    vdsp_usb_apply_pp(); assert(vdsp_usb_profile_applied==0);
    vdsp_usb_apply_aec(); assert(vdsp_usb_profile_applied==0);
    assert(!vdsp_usb_dfu_check(1,0,256,0xf0000,0x200000,999));
    assert(!vdsp_usb_dfu_check(1,1,256,0xf0000,0x200000,256));
    assert(vdsp_usb_dfu_check(0,0,256,0xf0000,0x200000,0));
    assert(vdsp_usb_dfu_check(1,0,128,0xf0000,0x200000,0));
    assert(vdsp_usb_dfu_check(1,0,256,0,0x200000,0));
    assert(vdsp_usb_dfu_check(1,0,256,0xf0001,0x200000,0));
    assert(vdsp_usb_dfu_check(1,0,256,0x200000,0x100000,0));
    assert(vdsp_usb_dfu_check(1,0,256,0xf0000,0x800000,0));
    assert(vdsp_usb_dfu_check(1,2,256,0xf0000,0x200000,256));
    assert(vdsp_usb_dfu_check(1,16,256,0x1ff000,0x200000,4096));
    puts("PASS: mute, six unmute gestures, cooldown/wrap, volume bounds, profile gains");
    puts("PASS: DFU alt1, 256-byte pages, partition bounds, order, capacity, retry reset");
    return 0;
}
