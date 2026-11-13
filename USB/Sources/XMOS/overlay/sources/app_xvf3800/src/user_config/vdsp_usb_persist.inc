/* Tile 1 only. Last two 4 KiB sectors reserved for local USB settings.
 * Boot/DFU partition stays at [0, 0x200000); no audio/DSP parameters stored. */
#include "vdsp_usb_nv.h"
#ifndef USB_NV_TEST
#include "rtos_qspi_flash.h"
#include "rtos_dfu_image.h"
#endif
static int nv_slot=-1;
static uint8_t nv_saved[16];
static const unsigned nv_base=0x3fe000;
static int nv_blank(const uint8_t *p,unsigned n)
{ while(n--)if(*p++!=255)return 0;return 1; }
static int nv_load(void)
{
    rtos_qspi_flash_t *f=get_qspi_flash_ctx();
    if(rtos_qspi_flash_size_get(f)!=0x400000 || rtos_qspi_flash_sector_size_get(f)!=4096 ||
       rtos_qspi_flash_page_size_get(f)!=256 || rtos_dfu_image_get_data_partition_addr(get_dfu_image_ctx())!=0x200000)return -1;
    uint8_t page[256]; bool foreign=false;
    for(unsigned i=0;i<32;i++) {
        rtos_qspi_flash_read(f,page,nv_base+256*i,256);
        usb_nv_record r;memcpy(&r,page,sizeof(r));
        if(usb_nv_valid(&r) && (nv_slot<0 || usb_nv_newer(r.sequence,nv_seq))) {
            nv_slot=(int)i;nv_seq=r.sequence;memcpy(nv_saved,r.settings,16);
        }
        if(!nv_blank(page,256) && r.magic!=USB_NV_MAGIC)foreign=true;
    }
    /* Never claim pre-existing unrelated data as a new settings partition. */
    if(foreign && nv_slot<0)return -1;
    return nv_slot<0?0:1;
}
static int nv_save(const uint8_t settings[16])
{
    rtos_qspi_flash_t *f=get_qspi_flash_ctx();
    unsigned slot=nv_slot<0?0:((unsigned)nv_slot+1)%32;
    uint8_t page[256],verify[256];usb_nv_record r;
    usb_nv_pack(&r,nv_seq+1,settings);memset(page,255,sizeof(page));memcpy(page,&r,sizeof(r));
    rtos_qspi_flash_lock(f);
    if(slot%16==0)rtos_qspi_flash_erase(f,nv_base+slot*256,4096);
    else {
        rtos_qspi_flash_read(f,verify,nv_base+slot*256,256);
        if(!nv_blank(verify,256)) {
            /* A torn page is skipped; preserve the sector containing the last good record. */
            slot=((unsigned)(nv_slot/16)^1u)*16;
            rtos_qspi_flash_erase(f,nv_base+slot*256,4096);
        }
    }
    rtos_qspi_flash_write(f,page,nv_base+slot*256,256);
    rtos_qspi_flash_read(f,verify,nv_base+slot*256,256);
    if(memcmp(page,verify,256)){rtos_qspi_flash_unlock(f);return -1;}
    uint32_t commit=USB_NV_COMMIT;
    rtos_qspi_flash_write(f,(uint8_t*)&commit,nv_base+slot*256+28,4);
    rtos_qspi_flash_read(f,verify,nv_base+slot*256,sizeof(r));
    rtos_qspi_flash_unlock(f);memcpy(&r,verify,sizeof(r));
    if(!usb_nv_valid(&r))return -1;
    nv_slot=slot;nv_seq=r.sequence;memcpy(nv_saved,settings,16);return 0;
}
#ifndef USB_NV_TEST
extern void vdsp_usb_uac_notify(const uint8_t status[56]);
static void persistence_task(void *unused)
{
    (void)unused;
    int loaded=nv_load();nv_state=loaded<0?1:loaded?3:2;nv_restored=loaded==1;
    uint8_t initial[4]={2,4,60,0};if(loaded==1)memcpy(initial,nv_saved,4);
    taskENTER_CRITICAL();
    boot_volume.attenuation=initial[2];boot_volume.route=initial[1];boot_volume.sequence=nv_seq;
    taskEXIT_CRITICAL();
    uint8_t status[56],filter[16],target[16]={2,4,60,0};
    float defaults[3]={9500,0,-6};memcpy(target+4,defaults,12);
    if(loaded==1)memcpy(target,nv_saved,16);
    for(;;) {
        if(!vdsp_v2_command_exchange(NULL,0,status) && status[1])break;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if(loaded==1 && vdsp_v2_filter_exchange(target+4,12,NULL)) {
        nv_state=6;vTaskDelete(NULL);return;
    }
    /* Ramp restored level through the existing gain path; never modify AEC.
     * A user command during this ramp takes precedence and cancels it. */
    uint8_t cmd[4];memcpy(cmd,target,4);cmd[2]=120;
    if(rpc(5,cmd,4,status,NULL)){nv_state=6;vTaskDelete(NULL);return;}
    uint32_t expected;memcpy(&expected,status+16,4);
    while(cmd[2]>target[2]) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if(vdsp_v2_command_exchange(NULL,0,status) || memcmp(&expected,status+16,4))break;
        cmd[2]=cmd[2]-target[2]>6?cmd[2]-6:target[2];
        if(rpc(5,cmd,4,status,NULL)){nv_state=6;break;}
        memcpy(&expected,status+16,4);
    }
    taskENTER_CRITICAL();boot_volume.restored=1;taskEXIT_CRITICAL();
    uint8_t pending[16]={0};uint32_t changed=0;
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(200));
        xSemaphoreTake(command_mutex,portMAX_DELAY);
        int rc=rpc_unlocked(0,NULL,0,status,filter);
        xSemaphoreGive(command_mutex);
        if(!rc && status[1])vdsp_usb_uac_notify(status);
        if(loaded<0 || nv_state==6)continue;
        if(rc || !status[1] || (status[5]&16) || memcmp(status+16,status+20,4))continue;
        uint8_t current[16]={2,status[2],status[44],status[40]&2u};memcpy(current+4,filter,12);
        if(!usb_nv_settings_valid(current))continue;
        uint32_t now=xTaskGetTickCount()*portTICK_PERIOD_MS;
        if(memcmp(current,pending,16)){
            memcpy(pending,current,16);changed=now;
            nv_state=nv_slot>=0 && !memcmp(current,nv_saved,16)?(nv_restored?3:5):4;
        }
        if((uint32_t)(now-changed)>=3000 && (nv_slot<0 || memcmp(current,nv_saved,16))) {
            nv_state=nv_save(current)?6:5;
        }
    }
}
void vdsp_usb_persistence_start(void)
{
    command_mutex=xSemaphoreCreateMutex();configASSERT(command_mutex);
    /* Guard host writes immediately; never block USB startup on flash RPC. */
    const uint8_t initial[4]={2,4,60,0};vdsp_usb_boot_init(&boot_volume,initial,0);
    _Static_assert(sizeof(boot_volume)==32,"USB boot diagnostic ABI");
    configASSERT(xTaskCreate(persistence_task,"USB settings",RTOS_THREAD_STACK_SIZE(persistence_task),NULL,
                            appconfTEST_TASK_PRIORITY,NULL)==pdPASS);
}
#endif
