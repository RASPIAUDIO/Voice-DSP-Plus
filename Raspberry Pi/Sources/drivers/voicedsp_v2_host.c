#include "voicedsp_v2_host.h"
#include "voicedsp_v2_guard.h"
#include <string.h>

enum { HOST_TRANSPORT=-1, HOST_ARGUMENT=-2, HOST_TIMEOUT=-3,
       HOST_HARDWARE=-4, HOST_CANCEL=-5, HOST_IDENTITY=-6 };
static uint32_t now(vdsp_v2_host *h) { return h->ops.now_ms(h->ops.context); }
static void wait_ms(void *context,unsigned ms)
{ vdsp_v2_host *h=context; h->ops.wait_ms(h->ops.context,ms); }
static bool cancelled(vdsp_v2_host *h)
{ return h->ops.cancelled && h->ops.cancelled(h->ops.context); }
static int transfer(vdsp_v2_host *h,uint8_t addr,const uint8_t *tx,size_t nt,uint8_t *rx,size_t nr)
{ return h->ops.transfer(h->ops.context,addr,tx,nt,rx,nr) ? HOST_TRANSPORT : 0; }
static int reg_write(void *context,uint8_t addr,uint8_t reg,uint8_t value)
{ uint8_t data[2]={reg,value}; return transfer(context,addr,data,2,NULL,0); }
static int reg_read(void *context,uint8_t addr,uint8_t reg,uint8_t *value)
{ return transfer(context,addr,&reg,1,value,1); }
static uint32_t unpack32(const uint8_t *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static bool advanced(uint32_t before,uint32_t after)
{ uint32_t delta=after-before; return delta && delta<0x80000000u; }
static bool crc_valid(const uint8_t *p)
{ return vdsp_v2_boot_crc(p,14)==(uint16_t)(p[14]|((unsigned)p[15]<<8)); }
static void crc_finish(uint8_t *p)
{ uint16_t crc=vdsp_v2_boot_crc(p,14); p[14]=(uint8_t)crc; p[15]=(uint8_t)(crc>>8); }

/* SDK read header length INCLUDES the status byte (unlike callback payload_len).
 * Only read-only status 64 gets a bounded retry, never a bootstrap commit.
 */
static int sdk_read(vdsp_v2_host *h,uint8_t command,uint8_t *data,size_t length)
{
    uint8_t header[3]={0x30,(uint8_t)(command|0x80),(uint8_t)(length+1)};
    uint8_t reply[17];
    h->runtime_probe.command=command;
    h->runtime_probe.status=255;
    h->runtime_probe.attempts=0;
    if (!length || length>16) return HOST_ARGUMENT;
    uint32_t started=now(h);
    for(unsigned i=0;i<10;++i) {
        h->runtime_probe.attempts=i+1;
        if(cancelled(h)) return HOST_CANCEL;
        if(transfer(h,0x2c,header,3,reply,length+1)) return HOST_TRANSPORT;
        h->runtime_probe.status=reply[0];
        if(!reply[0]) { memcpy(data,reply+1,length); return 0; }
        if(reply[0]!=64) return HOST_IDENTITY;
        if((uint32_t)(now(h)-started)>=100) return HOST_TIMEOUT;
        wait_ms(h,2);
    }
    return HOST_TIMEOUT;
}
static int runtime_failed(vdsp_v2_host *h,int error,uint32_t started)
{
    h->runtime_probe.error=error;
    h->runtime_probe.elapsed_ms=now(h)-started;
    h->runtime_failure=h->runtime_probe;
    ++h->runtime_failures;
    return error;
}
static int runtime(vdsp_v2_host *h)
{
    uint8_t version[3],echo[16],first[8],second[8];
    h->session_valid=h->clocks=false;
    uint32_t started=now(h);
    h->runtime_probe=(vdsp_v2_runtime_trace){.stage=1};
    int result=sdk_read(h,0,version,3);
    if(result) return runtime_failed(h,result,started);
    if(memcmp(version,"\0\6\0",3)) return runtime_failed(h,HOST_IDENTITY,started);
    h->runtime_probe.stage=2;
    result=sdk_read(h,9,echo,16);
    if(result) return runtime_failed(h,result,started);
    if(memcmp(echo,h->record,16)) return runtime_failed(h,HOST_IDENTITY,started);
    h->runtime_probe.stage=3;
    result=sdk_read(h,10,first,8);
    if(result) return runtime_failed(h,result,started);
    h->runtime_probe.frames_before=unpack32(first);
    h->runtime_probe.ticks_before=unpack32(first+4);
    wait_ms(h,2);
    h->runtime_probe.stage=4;
    result=sdk_read(h,10,second,8);
    if(result) return runtime_failed(h,result,started);
    h->runtime_probe.frames_after=unpack32(second);
    h->runtime_probe.ticks_after=unpack32(second+4);
    h->runtime_probe.stage=5;
    if((uint32_t)(now(h)-started)>500) return runtime_failed(h,HOST_TIMEOUT,started);
    h->runtime_probe.stage=6;
    if(!advanced(unpack32(first),unpack32(second)) ||
       !advanced(unpack32(first+4),unpack32(second+4))) return runtime_failed(h,HOST_TIMEOUT,started);
    h->runtime_probe.stage=0;
    h->runtime_probe.elapsed_ms=now(h)-started;
    h->session_valid=h->clocks=true;
    return 0;
}
static int refresh(vdsp_v2_host *h)
{
    int result=runtime(h);
    h->io.valid=false;
    if(result==HOST_IDENTITY) h->stage=VDSP_HOST_FAULT;
    if(result) return result;
    uint32_t inputs_ms=now(h);
    if(vdsp_v2_control_sample(&h->control,&h->io)) return HOST_TRANSPORT;
    h->inputs_ms=inputs_ms;
    if(h->io.microphone!=h->record[5]) { h->io.valid=false; h->stage=VDSP_HOST_FAULT; return HOST_IDENTITY; }
    (void)vdsp_v2_power_poll(&h->power,now(h));
    h->refreshed_ms=now(h);
    return 0;
}
static int inhibit(vdsp_v2_host *h,int error)
{
    h->output_prepared_valid=false;
    h->policy.inhibited=true;
    if(h->audio.initialized) (void)vdsp_v2_audio_stop(&h->audio);
    else if(h->claimed_bus && h->control.initialized)
        (void)vdsp_v2_control_amp(&h->control,false,false,false);
    h->last_error=error;
    return error;
}
static bool authorize(void *context,vdsp_v2_audio_route route)
{
    vdsp_v2_host *h=context;
    if(h->stage!=VDSP_HOST_READY || cancelled(h) || refresh(h)) return false;
    h->decision=vdsp_v2_policy_update(&h->policy,&h->io,&h->power.current,h->clocks,now(h));
    return !cancelled(h) && h->output_prepared_valid &&
           h->output_prepared_route==route && h->decision.route==route &&
           (route==VDSP_AUDIO_LINE || h->decision.power_approved);
}
int vdsp_v2_host_init(vdsp_v2_host *h,const vdsp_v2_host_ops *ops,unsigned rate,
                      const vdsp_v2_policy_limits *limits,bool paired)
{
    if(!h || !ops || !ops->transfer || !ops->now_ms || !ops->wait_ms ||
       !ops->start_silent_clocks || !paired || (rate!=16000 && rate!=48000)) return HOST_ARGUMENT;
    vdsp_v2_policy policy;
    if(vdsp_v2_policy_init(&policy,limits)) return HOST_ARGUMENT;
    *h=(vdsp_v2_host){.ops=*ops,.rate=rate,.policy=policy,.initialized=true};
    h->control=(vdsp_v2_control){h,reg_write,reg_read,false,false};
    h->audio=(vdsp_v2_audio){&h->control,wait_ms,h,false,true,VDSP_AUDIO_UNKNOWN,-60,0};
    h->power=(vdsp_v2_power){.context=h,.read=reg_read};
    return 0;
}
static int start_error(vdsp_v2_host *h,int error)
{ h->stage=VDSP_HOST_FAULT; return inhibit(h,error); }
int vdsp_v2_host_start(vdsp_v2_host *h,uint32_t cookie)
{
    if(!h || !h->initialized || h->stage!=VDSP_HOST_NEW) return HOST_ARGUMENT;
    uint8_t identity[16];
    int result=HOST_TIMEOUT;
    for(unsigned i=0;i<40;++i) {
        if(cancelled(h)) return start_error(h,HOST_CANCEL);
        result=transfer(h,0x2c,NULL,0,identity,16);
        if(!result) break;
        wait_ms(h,25);
    }
    if(result) return start_error(h,HOST_TIMEOUT);
    if(memcmp(identity,"V2BT\1\0\6\0",8) || identity[8]!=h->rate/1000 ||
       identity[9] || identity[10] || identity[11] || identity[12] || identity[13] || !crc_valid(identity))
        return start_error(h,HOST_IDENTITY);
    h->claimed_bus=true;
    if(vdsp_v2_control_init(&h->control)) return start_error(h,HOST_TRANSPORT);
    if(reg_write(h,0x18,0,0) || reg_write(h,0x18,1,1)) return start_error(h,HOST_TRANSPORT);
    wait_ms(h,1);
    uint8_t reset=1,flags=0xff;
    for(unsigned i=0;i<10;++i) {
        if(reg_read(h,0x18,1,&reset)) return start_error(h,HOST_TRANSPORT);
        if(!(reset&1)) break;
        wait_ms(h,5);
    }
    if((reset&1) || reg_read(h,0x18,37,&flags) || (flags&0xbb)) return start_error(h,HOST_HARDWARE);
    unsigned stable=0; vdsp_v2_mic_array previous=VDSP_MIC_UNKNOWN;
    for(unsigned i=0;i<12;++i) {
        if(cancelled(h)) return start_error(h,HOST_CANCEL);
        if(vdsp_v2_control_sample(&h->control,&h->io) || (h->io.port0&VDSP_V2_AMP_ENABLE)) stable=0;
        else { stable=h->io.microphone==previous ? stable+1 : 1; previous=h->io.microphone; }
        if(stable>=4) break;
        wait_ms(h,10);
    }
    if(stable<4) return start_error(h,HOST_HARDWARE);
    memcpy(h->record,"V2CF",4); h->record[4]=1; h->record[5]=(uint8_t)h->io.microphone;
    h->record[6]=h->io.port0; h->record[7]=h->io.port1; h->record[8]=(uint8_t)(h->rate/1000); h->record[9]=3;
    for(unsigned i=0;i<4;++i) h->record[10+i]=(uint8_t)(cookie>>(8*i));
    crc_finish(h->record);
    if(transfer(h,0x2c,h->record,16,NULL,0)) return start_error(h,HOST_TRANSPORT);
    if(h->ops.start_silent_clocks(h->ops.context,h->rate)) return start_error(h,HOST_HARDWARE);
    for(unsigned i=0;i<40;++i) {
        result=runtime(h);
        if(!result || result==HOST_IDENTITY || result==HOST_CANCEL) break;
        wait_ms(h,25);
    }
    if(result) return start_error(h,result);
    vdsp_v2_audio_config config={h->rate,32,VDSP_DIN_PRIMARY,true};
    h->output_prepared_valid=false;
    result=vdsp_v2_audio_init(&h->audio,&config);
    if(result) return start_error(h,result);
    h->stage=VDSP_HOST_READY;
    h->last_error=0;
    return 0;
}
int vdsp_v2_host_stop(vdsp_v2_host *h)
{
    if(!h || !h->initialized || !h->claimed_bus) return HOST_ARGUMENT;
    h->output_prepare=NULL;
    h->output_prepare_context=NULL;
    h->output_prepared_valid=false;
    (void)vdsp_v2_policy_request(&h->policy,VDSP_AUDIO_OFF);
    int result=h->audio.initialized ? vdsp_v2_audio_stop(&h->audio) : HOST_HARDWARE;
    if(result) return inhibit(h,result);
    h->last_error=0;
    return 0;
}
int vdsp_v2_host_resume(vdsp_v2_host *h)
{
    if(!h || !h->initialized || h->stage!=VDSP_HOST_NEW) return HOST_ARGUMENT;
    uint8_t version[3],record[16];
    int result=sdk_read(h,0,version,3);
    if(result) return start_error(h,result);
    if(memcmp(version,"\0\6\0",3)) return start_error(h,HOST_IDENTITY);
    result=sdk_read(h,9,record,16);
    if(result) return start_error(h,result);
    /* Reuse the exact bootstrap validator, without sending any boot commit. */
    vdsp_v2_host_boot checked;
    if(vdsp_v2_host_boot_init(&checked,h->rate/1000)) return start_error(h,HOST_ARGUMENT);
    vdsp_v2_host_boot_write_begin(&checked);
    for(unsigned i=0;i<16;++i)
        if(!vdsp_v2_host_boot_receive(&checked,record[i])) return start_error(h,HOST_IDENTITY);
    vdsp_v2_host_boot_stop(&checked);
    if(!checked.committed) return start_error(h,HOST_IDENTITY);
    memcpy(h->record,record,16);
    h->claimed_bus=true;
    if(vdsp_v2_control_init(&h->control)) return start_error(h,HOST_TRANSPORT);
    /* A previous session may have left the codec powered. Mute before clocks. */
    if(reg_write(h,0x18,0,0) || reg_write(h,0x18,64,0x0c))
        return start_error(h,HOST_TRANSPORT);
    if(vdsp_v2_control_sample(&h->control,&h->io)) return start_error(h,HOST_TRANSPORT);
    if(h->io.microphone!=h->record[5]) return start_error(h,HOST_IDENTITY);
    if(h->ops.start_silent_clocks(h->ops.context,h->rate)) return start_error(h,HOST_HARDWARE);
    result=runtime(h);
    if(result) return start_error(h,result);
    vdsp_v2_audio_config config={h->rate,32,VDSP_DIN_PRIMARY,true};
    h->output_prepared_valid=false;
    result=vdsp_v2_audio_init(&h->audio,&config);
    if(result) return start_error(h,result);
    h->stage=VDSP_HOST_READY;
    h->last_error=0;
    return 0;
}
/* VDSP_V2_PERSISTENT_PREPARE_V1: effective-route preparation is part of
 * activation, including policy-driven jack transitions. Never prepare while
 * a route is powered, and never replay a successful preparation every poll. */
static int apply(vdsp_v2_host *h,bool force)
{
    h->decision=vdsp_v2_policy_update(&h->policy,&h->io,&h->power.current,h->clocks,now(h));
    if(!h->audio.initialized) return inhibit(h,HOST_HARDWARE);
    const vdsp_v2_audio_route route=h->decision.route;
    int result=0;
    if(route==VDSP_AUDIO_OFF) {
        h->output_prepared_valid=false;
        if(force || h->audio.route!=VDSP_AUDIO_OFF)
            result=vdsp_v2_audio_stop(&h->audio);
    } else if(force || h->audio.route!=route || !h->output_prepared_valid ||
              h->output_prepared_route!=route) {
        h->output_prepared_valid=false;
        result=vdsp_v2_audio_stop(&h->audio);
        if(!result) {
            h->audio.external_gain_db=0;
            h->audio.internal_gain_db=0;
            result=h->output_prepare ? h->output_prepare(h->output_prepare_context,
                &h->audio,h->rate,h->output_attenuation) :
                vdsp_v2_audio_volume(&h->audio,-(int)h->output_attenuation);
        }
        if(!result && (!h->audio.initialized || h->audio.route!=VDSP_AUDIO_OFF ||
                       !h->audio.muted || h->control.amp_enabled ||
                       h->decision.route!=route)) result=HOST_HARDWARE;
        if(!result && cancelled(h)) result=HOST_CANCEL;
        if(!result) {
            h->output_prepared_route=route;
            h->output_prepared_valid=true;
            /* Guard refreshes policy again. A jack edge during preparation
             * must fail closed, never enable a different, unprepared route. */
            result=vdsp_v2_audio_guarded_select(&h->audio,route,authorize,h);
        }
    }
    if(!result) result=vdsp_v2_audio_check(&h->audio);
    if(result) return inhibit(h,result);
    h->last_error=0;
    return 0;
}
int vdsp_v2_host_request_prepared(vdsp_v2_host *h,vdsp_v2_audio_route route,unsigned attenuation,
                                 vdsp_v2_output_prepare_fn prepare,void *context)
{
    if(!h || !h->initialized || route<VDSP_AUDIO_OFF || route>VDSP_AUDIO_DAC_SPEAKERS || attenuation>127) return HOST_ARGUMENT;
    if(route==VDSP_AUDIO_OFF) return vdsp_v2_host_stop(h);
    if(h->stage!=VDSP_HOST_READY) return HOST_ARGUMENT;
    int result=runtime(h);
    if(result==HOST_IDENTITY) h->stage=VDSP_HOST_FAULT;
    if(result) return inhibit(h,result);
    if(!h->control.initialized && vdsp_v2_control_init(&h->control)) return inhibit(h,HOST_TRANSPORT);
    result=refresh(h);
    if(result) return inhibit(h,result);
    (void)vdsp_v2_policy_request(&h->policy,route);
    if(!h->audio.initialized) {
        vdsp_v2_audio_config config={h->rate,32,VDSP_DIN_PRIMARY,true};
        h->output_prepared_valid=false;
    result=vdsp_v2_audio_init(&h->audio,&config);
    }
    if(result) return inhibit(h,result);
    h->output_prepare=prepare;
    h->output_prepare_context=context;
    h->output_attenuation=attenuation;
    h->output_prepared_valid=false;
    return apply(h,true);
}
int vdsp_v2_host_request(vdsp_v2_host *h,vdsp_v2_audio_route route,unsigned attenuation)
{
    return vdsp_v2_host_request_prepared(h,route,attenuation,NULL,NULL);
}
int vdsp_v2_host_poll(vdsp_v2_host *h)
{
    if(!h || !h->initialized || h->stage!=VDSP_HOST_READY) return HOST_ARGUMENT;
    int result=refresh(h);
    if(result) return inhibit(h,result);
    if(cancelled(h)) return inhibit(h,HOST_CANCEL);
    return apply(h,false);
}
