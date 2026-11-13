/* Included in usb_audio.c on tile 1. Only the speaker feature unit changes.
 * No HID key injection: the UAC notification identifies this exact endpoint. */
#ifndef USB_UAC_TEST
#include "user_config/voicedsp_v2_commands.h"
#include "user_config/vdsp_usb_volume.h"
#include "user_config/vdsp_usb_boot.h"
#endif
extern int vdsp_usb_volume_set(unsigned selector, int16_t value);
void tud_audio_int_done_cb(uint8_t rhport) { (void)rhport; }

static bool vdsp_usb_uac_set(tusb_control_request_t const *r,const uint8_t *data)
{
    if(r->bRequest!=AUDIO_CS_REQ_CUR || TU_U16_LOW(r->wValue)!=0 ||
       TU_U16_LOW(r->wIndex)!=ITF_NUM_AUDIO_CONTROL)return false;
    unsigned selector=TU_U16_HIGH(r->wValue);
    int16_t value;
    if(selector==AUDIO_FU_CTRL_MUTE && r->wLength==1)value=data[0];
    else if(selector==AUDIO_FU_CTRL_VOLUME && r->wLength==2)memcpy(&value,data,2);
    else return false;
    return vdsp_usb_boot_volume(selector,value)==0;
}
static bool vdsp_usb_uac_get(uint8_t rhport,tusb_control_request_t const *r)
{
    if(TU_U16_LOW(r->wValue)!=0 || TU_U16_LOW(r->wIndex)!=ITF_NUM_AUDIO_CONTROL)return false;
    unsigned selector=TU_U16_HIGH(r->wValue);
    if(selector==AUDIO_FU_CTRL_VOLUME && r->bRequest==AUDIO_CS_REQ_RANGE) {
        const int16_t range[4]={1,-60*256,0,128};
        return tud_audio_buffer_and_schedule_control_xfer(rhport,r,(void*)range,sizeof(range));
    }
    if(r->bRequest!=AUDIO_CS_REQ_CUR)return false;
    uint8_t status[56];
    if(vdsp_usb_boot_read(status))return false;
    if(selector==AUDIO_FU_CTRL_MUTE) {
        uint8_t mute=status[40]&1u;
        return tud_audio_buffer_and_schedule_control_xfer(rhport,r,&mute,1);
    }
    if(selector==AUDIO_FU_CTRL_VOLUME) {
        int16_t volume=vdsp_usb_volume_q8(status[47]);
        return tud_audio_buffer_and_schedule_control_xfer(rhport,r,&volume,2);
    }
    return false;
}

void vdsp_usb_uac_notify(const uint8_t status[56])
{
    static int sent_volume=-1,sent_mute=-1;
    if(!tud_mounted()){sent_volume=sent_mute=-1;return;}
    if(!vdsp_usb_boot_notify()){sent_volume=sent_mute=-1;return;}
    int volume=status[47],mute=status[40]&1u;
    unsigned selector=sent_volume!=volume?AUDIO_FU_CTRL_VOLUME:
                      sent_mute!=mute?AUDIO_FU_CTRL_MUTE:0;
    if(!selector)return;
    audio_interrupt_data_t message={0};
    message.bAttribute=AUDIO_CS_REQ_CUR;
    message.wValue=(uint16_t)(selector<<8);
    message.wIndex=(UAC2_ENTITY_SPK_FEATURE_UNIT<<8)|ITF_NUM_AUDIO_CONTROL;
    if(tud_audio_int_write(&message)) {
        if(selector==AUDIO_FU_CTRL_VOLUME)sent_volume=volume;else sent_mute=mute;
    }
}
