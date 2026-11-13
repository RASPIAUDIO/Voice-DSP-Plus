#include "app_conf.h"
#include "voicedsp_v2_commands.h"
#include "vdsp_usb_filter.h"
#include "vdsp_usb_volume.h"
#include "vdsp_usb_boot.h"
#include "platform/driver_instances.h"
#include "FreeRTOS.h"
#include "rtos_macros.h"
#include "task.h"
#include "semphr.h"
#include <stdbool.h>
#include <string.h>
#define REPLY_SIZE (5+VDSP_V2_STATUS_SIZE+VDSP_USB_FILTER_STATUS_SIZE)

#if ON_TILE(0)
#include "vdsp_usb_fixed_filter.h" /* Initial preset only, replaced on command19. */
static uint8_t latest[4]={2,1,60,1}; /* Muted/OFF until settings restore. */
static uint8_t published[VDSP_V2_STATUS_SIZE];
static uint32_t epoch,filter_epoch;
static bool ready;
static uint8_t volume_trace[32];
static void trace_volume(unsigned source,const uint8_t command[4])
{
    if(command[2]==latest[2] && command[1]==latest[1])return;
    uint32_t count=0,now=xTaskGetTickCount()*portTICK_PERIOD_MS;
    volume_trace[0]=(uint8_t)source;volume_trace[1]=command[2];volume_trace[2]=command[1];
    memcpy(&count,volume_trace+4,4);++count;memcpy(volume_trace+4,&count,4);
    memcpy(volume_trace+8,&now,4);
    if(!command[2] && !volume_trace[3]){volume_trace[3]=(uint8_t)source;memcpy(volume_trace+12,&now,4);}
    if(source>=1 && source<=4){unsigned offset=12+source*4;memcpy(&count,volume_trace+offset,4);++count;memcpy(volume_trace+offset,&count,4);}
}
static vdsp_v2_crossover_config filter_config={9500,0,-6,0};
static uint8_t filter_coefficients[2][30];

int vdsp_usb_command_accept_source(const uint8_t command[4],unsigned source)
{
    if(!command || command[0]!=2 || command[1]<1 || command[1]>4 || command[2]>120 ||
       (command[3]&~3u))return -2;
    taskENTER_CRITICAL();
    if(!ready){taskEXIT_CRITICAL();return -1;}
    trace_volume(source,command);memcpy(latest,command,4);++epoch;taskEXIT_CRITICAL();return 0;
}
int vdsp_v2_command_accept(const uint8_t command[4])
{return vdsp_usb_command_accept_source(command,1);}
static int filter_accept(const uint8_t *packet)
{
    vdsp_v2_crossover_config c;
    if(vdsp_usb_filter_decode(packet,&c))return -2;
    taskENTER_CRITICAL();
    if(!ready){taskEXIT_CRITICAL();return -1;}
    filter_config=c;memcpy(filter_coefficients,packet+12,60);
    filter_epoch=++epoch;taskEXIT_CRITICAL();return 0;
}
uint32_t vdsp_v2_filter_get(vdsp_v2_crossover_config *config,vdsp_v2_equalizer_pair *pair)
{
    taskENTER_CRITICAL();uint32_t revision=filter_epoch;
    if(config)*config=filter_config;
    if(pair){
        memset(pair,0,sizeof(*pair));
        for(unsigned ch=0;ch<2;ch++)memcpy(pair->channels[ch].coefficients,filter_coefficients[ch],30);
    }
    taskEXIT_CRITICAL();return revision;
}
static void filter_status(uint8_t *data)
{
    taskENTER_CRITICAL();
    memcpy(data,&filter_config.cutoff_hz,4);memcpy(data+4,&filter_config.left_trim_db,4);
    memcpy(data+8,&filter_config.right_trim_db,4);memcpy(data+12,&filter_epoch,4);
    taskEXIT_CRITICAL();
}
void vdsp_v2_command_ready(void)
{
    taskENTER_CRITICAL();
    memcpy(filter_coefficients,usb_full_speaker_coefficients,60);
    ready=true;taskEXIT_CRITICAL();
}
uint32_t vdsp_v2_command_get(uint8_t command[4])
{taskENTER_CRITICAL();memcpy(command,latest,4);uint32_t e=epoch;taskEXIT_CRITICAL();return e;}
void vdsp_v2_status_publish(const uint8_t status[VDSP_V2_STATUS_SIZE])
{taskENTER_CRITICAL();memcpy(published,status,sizeof(published));taskEXIT_CRITICAL();}
void vdsp_v2_status_get(uint8_t status[VDSP_V2_STATUS_SIZE])
{
    taskENTER_CRITICAL();memcpy(status,published,sizeof(published));
    uint32_t ticks=xTaskGetTickCount();memcpy(status+48,&ticks,4);memcpy(status+16,&epoch,4);
    status[40]=latest[3];status[46]=latest[1];status[47]=latest[2];taskEXIT_CRITICAL();
}
static int volume_accept(const uint8_t *p)
{
    int16_t value;memcpy(&value,p+1,2);
    taskENTER_CRITICAL();
    uint8_t next[4];memcpy(next,latest,4);
    int rc=ready?vdsp_usb_volume_apply(next,p[0],value):-1;
    if(!rc){trace_volume(2,next);memcpy(latest,next,4);++epoch;}
    taskEXIT_CRITICAL();return rc;
}
int vdsp_v2_command_exchange(const uint8_t *command,size_t length,uint8_t *status)
{
    if((command && length!=4) || (!command && length))return -2;
    int result=command?vdsp_v2_command_accept(command):0;
    if(status)vdsp_v2_status_get(status);return result;
}
static void command_responder(void *unused)
{
    (void)unused;rtos_intertile_t *ctx=get_intertile_ctx();
    for(;;){
        void *request=NULL;
        size_t n=rtos_intertile_rx(ctx,VDSP_V2_COMMAND_PORT,&request,RTOS_OSAL_WAIT_FOREVER);
        if(request && n>=5){
            const uint8_t *p=request;
            uint8_t reply[REPLY_SIZE]={0};memcpy(reply,p,4);
            int rc=(p[4]==0 || p[4]==3 || p[4]==6) && n==5?0:
                p[4]==1 && n==9?vdsp_v2_command_accept(p+5):
                p[4]==2 && n==5+VDSP_USB_FILTER_PACKET_SIZE?filter_accept(p+5):
                p[4]==4 && n==8?volume_accept(p+5):
                p[4]==5 && n==9?vdsp_usb_command_accept_source(p+5,5):-2;
            reply[4]=rc?1:0;vdsp_v2_status_get(reply+5);
            filter_status(reply+5+VDSP_V2_STATUS_SIZE);
            if(p[4]==6){taskENTER_CRITICAL();memcpy(reply+5,volume_trace,32);taskEXIT_CRITICAL();}
            rtos_intertile_tx(ctx,VDSP_V2_COMMAND_PORT,reply,sizeof(reply));
        }
        if(request)rtos_osal_free(request);
    }
}
void vdsp_v2_command_start(void)
{configASSERT(xTaskCreate(command_responder,"V2 commands",RTOS_THREAD_STACK_SIZE(command_responder),NULL,appconfTEST_TASK_PRIORITY,NULL)==pdPASS);}
#else
static SemaphoreHandle_t command_mutex;
static volatile uint8_t nv_state, nv_restored;
static uint32_t nv_seq;
static vdsp_usb_boot_state boot_volume;
static int rpc_unlocked(unsigned operation,const uint8_t *payload,size_t size,uint8_t *status,uint8_t *filter)
{
    static uint32_t sequence;
    uint8_t request[5+VDSP_USB_FILTER_PACKET_SIZE]={0};
    if(size>VDSP_USB_FILTER_PACKET_SIZE)return -2;
    uint32_t id=++sequence;memcpy(request,&id,4);request[4]=operation;
    if(size)memcpy(request+5,payload,size);
    rtos_intertile_t *ctx=get_intertile_ctx();
    rtos_intertile_tx(ctx,VDSP_V2_COMMAND_PORT,request,5+size);
    TickType_t start=xTaskGetTickCount();
    for(;;){
        TickType_t elapsed=xTaskGetTickCount()-start;if(elapsed>=pdMS_TO_TICKS(250))return -1;
        void *reply=NULL;
        size_t n=rtos_intertile_rx(ctx,VDSP_V2_COMMAND_PORT,&reply,pdMS_TO_TICKS(250)-elapsed);
        bool match=n==REPLY_SIZE && reply && !memcmp(reply,&id,4);
        int result=-1;
        if(match){
            uint8_t *p=reply;result=p[4]?-2:0;
            if(status)memcpy(status,p+5,VDSP_V2_STATUS_SIZE);
            if(filter)memcpy(filter,p+5+VDSP_V2_STATUS_SIZE,VDSP_USB_FILTER_STATUS_SIZE);
        }
        if(reply)rtos_osal_free(reply);
        if(match || !n)return result;
    }
}
static int rpc(unsigned operation,const uint8_t *payload,size_t size,uint8_t *status,uint8_t *filter)
{
    if(!command_mutex)return -1;
    xSemaphoreTake(command_mutex,portMAX_DELAY);
    int rc=rpc_unlocked(operation,payload,size,status,filter);
    xSemaphoreGive(command_mutex);return rc;
}
int vdsp_v2_command_exchange(const uint8_t *command,size_t length,uint8_t *status)
{
    if((command && length!=4) || (!command && length))return -2;
    int rc=rpc(command?1:0,command,length,status,NULL);
    if(!rc && status){status[52]=nv_state;status[53]=nv_restored;status[54]=nv_seq;status[55]=nv_seq>>8;}
    return rc;
}
int vdsp_usb_volume_set(unsigned selector,int16_t value)
{
    uint8_t packet[3]={(uint8_t)selector};memcpy(packet+1,&value,2);
    return rpc(4,packet,sizeof(packet),NULL,NULL);
}
int vdsp_usb_boot_volume(unsigned selector,int16_t value)
{
    uint8_t check[4]={2,4,60,0};
    if(vdsp_usb_volume_apply(check,selector,value))return -2;
    taskENTER_CRITICAL();
    bool apply=vdsp_usb_boot_set(&boot_volume,xTaskGetTickCount()*portTICK_PERIOD_MS,selector,value);
    taskEXIT_CRITICAL();
    return apply?vdsp_usb_volume_set(selector,value):0;
}
int vdsp_usb_boot_read(uint8_t *status)
{
    int rc=vdsp_v2_command_exchange(NULL,0,status);if(rc)return rc;
    taskENTER_CRITICAL();vdsp_usb_boot_get(&boot_volume,xTaskGetTickCount()*portTICK_PERIOD_MS);
    if(!boot_volume.restored){status[47]=boot_volume.attenuation;status[40]&=2u;}
    taskEXIT_CRITICAL();return 0;
}
int vdsp_usb_boot_notify(void)
{
    taskENTER_CRITICAL();vdsp_usb_boot_poll(&boot_volume,xTaskGetTickCount()*portTICK_PERIOD_MS);
    int locked=boot_volume.locked;taskEXIT_CRITICAL();return !locked;
}
int vdsp_usb_volume_trace(uint8_t *out)
{
    uint8_t status[56];int rc=rpc(6,NULL,0,status,NULL);if(rc)return rc;
    taskENTER_CRITICAL();memcpy(out,&boot_volume,32);taskEXIT_CRITICAL();
    memcpy(out+32,status,24);return 0;
}
int vdsp_v2_filter_exchange(const uint8_t *params,size_t length,uint8_t *status)
{
    if(!params)return length?-2:rpc(3,NULL,0,NULL,status);
    if(length!=VDSP_USB_FILTER_PARAMS_SIZE)return -2;
    vdsp_v2_crossover_config config;vdsp_v2_equalizer_pair pair;
    if(vdsp_usb_filter_decode(params,&config) || vdsp_v2_crossover_generate(&config,&pair))return -2;
    uint8_t packet[VDSP_USB_FILTER_PACKET_SIZE];memcpy(packet,params,12);
    for(unsigned ch=0;ch<2;ch++)memcpy(packet+12+ch*30,pair.channels[ch].coefficients,30);
    return rpc(2,packet,sizeof(packet),NULL,status);
}
#include "vdsp_usb_persist.inc"
#endif
