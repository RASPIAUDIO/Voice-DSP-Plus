#include "settings.h"
#include "v2_ui_runtime.h"
#include "voicedsp_v2_equalizer.h"
#include "voicedsp_v2_gesture.h"
extern "C" {
#include "transport.h"
}
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

using esphome::voicedsp_v2::UiRuntime;
static volatile sig_atomic_t quitting;
static void stop_signal(int) { quitting=1; }

struct Runtime {
  vdsp_v2_host host{};
  UiRuntime ui;
  Settings settings;
  uint32_t requests[3]{};
  unsigned profile{4};
  bool voice_profile_override{};
  bool legacy_profile_override{};
  bool crossover_verified{};
  uint32_t capture_request{};
  vdsp_v2_equalizer_pair applied{};

  void step_ui() { ui.step(host,requests,capture_request,0,profile); }
  static int prepare(void *ctx,vdsp_v2_audio *audio,unsigned rate,unsigned attenuation) {
    auto &r=*static_cast<Runtime*>(ctx);
    auto a=r.ui.snapshot().aec;
    if(!a.profile_valid || a.profile!=r.profile) return -3;
    if(r.settings.full && r.host.decision.route==VDSP_AUDIO_HT326) {
      int rc=vdsp_v2_crossover_apply_off(audio,rate,&r.settings.crossover,-int(attenuation),&r.applied);
      if(rc)return rc;
      // A direct-connected tweeter depends on this high-pass: verify both
      // powered-down coefficient banks before the host can enable the amplifier.
      auto *c=audio->control;
      for(unsigned page: {8u,12u}) {
        if(c->write(c->context,VDSP_V2_DAC_ADDR,0,page))return -4;
        for(unsigned ch=0;ch<2;++ch) for(unsigned filter=0;filter<3;++filter)
          for(unsigned byte=0;byte<10;++byte) {
            uint8_t actual{};
            if(c->read(c->context,VDSP_V2_DAC_ADDR,2+64*ch+10*filter+byte,&actual) ||
               actual!=r.applied.channels[ch].coefficients[filter][byte]) {
              (void)c->write(c->context,VDSP_V2_DAC_ADDR,0,0);return -4;
            }
          }
      }
      if(c->write(c->context,VDSP_V2_DAC_ADDR,0,0))return -4;
      r.crossover_verified=true;
      return 0;
    }
    return vdsp_v2_equalizer_apply_pair_off(audio,rate,0,0,0,0,-int(attenuation),&r.applied);
  }
  int apply() {
    crossover_verified=false;
    requests[0]=UiRuntime::next_request(requests[0],settings.mic_muted);
    // XMOS UI state MUTED=9; idle=0 returns ownership to the DOA display.
    requests[1]=UiRuntime::next_request(requests[1],settings.muted||settings.mic_muted?9:0);
    auto route=settings.muted?VDSP_AUDIO_OFF:settings.route;
    int stopped=vdsp_v2_host_stop(&host);
    if(stopped || route==VDSP_AUDIO_OFF) return stopped;
    profile=aec_profile_for_route(settings.route,legacy_profile_override);
    uint32_t start=host.ops.now_ms(host.ops.context);
    // Poll while OFF to keep the session fresh during multi-step UI/AEC readback.
    for(;;) {
      int rc=vdsp_v2_host_poll(&host);if(rc)return rc;
      step_ui();
      auto a=ui.snapshot().aec;
      if(a.profile_valid && a.profile==profile) break;
      if(quitting || uint32_t(host.ops.now_ms(host.ops.context)-start)>4000) return -3;
      host.ops.wait_ms(host.ops.context,20);
    }
    return vdsp_v2_host_request_prepared(&host,route,settings.attenuation,prepare,this);
  }
  std::string status(const vdsp_v2_gesture &gesture) {
    auto s=ui.snapshot();
    std::ostringstream out;
    out<<"{\"ready\":"<<(host.stage==VDSP_HOST_READY?"true":"false")
       <<",\"requested_route\":"<<settings.route<<",\"actual_route\":"<<host.audio.route
       <<",\"volume_db\":"<<(-int(settings.attenuation)/2.0)
       <<",\"full_speaker\":"<<(settings.full?"true":"false")
       <<",\"crossover_verified\":"<<(crossover_verified?"true":"false")
       <<",\"crossover_hz\":"<<settings.crossover.cutoff_hz
       <<",\"woofer_trim_db\":"<<settings.crossover.left_trim_db
       <<",\"tweeter_trim_db\":"<<settings.crossover.right_trim_db
       <<",\"speaker_muted\":"<<(settings.muted?"true":"false")
       <<",\"mic_muted\":"<<(s.mute_valid&&s.muted?"true":"false")
       <<",\"mic_mute_valid\":"<<(s.mute_valid?"true":"false")
       <<",\"geometry\":"<<host.io.microphone<<",\"jack\":"<<((host.io.port0&32)?"true":"false")
       <<",\"power_good\":"<<(host.io.buck_pg?"true":"false")
       <<",\"amp_sd_n\":"<<(host.io.amp_sd_n?"true":"false")
       <<",\"aec_profile\":"<<s.aec.profile<<",\"aec_valid\":"<<(s.aec.profile_valid?"true":"false")
       <<",\"aec_voice_override\":"<<(voice_profile_override?"true":"false")
       <<",\"aec_legacy_override\":"<<(legacy_profile_override?"true":"false")
       <<",\"aec_metadata_valid\":"<<s.aec.metadata_valid
       <<",\"aec_asr_output\":"<<s.aec.asr_output<<",\"aec_system_delay\":"<<s.aec.system_delay
       <<",\"gesture_ready\":"<<(gesture.snapshot.ready?"true":"false")
       <<",\"gesture_errors\":"<<gesture.snapshot.errors
       <<",\"gestures_enabled\":"<<(settings.gestures?"true":"false")
       <<",\"aec_raw\":"<<(s.aec.raw?"true":"false")
       <<",\"aec_mux_valid\":"<<(s.aec.mux_valid?"true":"false")
       <<",\"aec_mux\":["<<unsigned(s.aec.mux[0])<<","<<unsigned(s.aec.mux[1])
       <<","<<unsigned(s.aec.mux[2])<<","<<unsigned(s.aec.mux[3])<<"]"
       <<",\"aec_parameter_mask\":"<<s.aec.valid<<",\"aec_parameter_words\":[";
    for(unsigned i=0;i<24;++i) { if(i)out<<",";out<<s.aec.values[i]; }
    out<<"],\"aec_error\":"<<s.aec.error<<",\"error\":"<<host.last_error<<"}";
    return out.str();
  }
};

int main(int argc,char **argv) {
  std::string device, socket_path;
  bool paired=false,clocks=false,resume=false;
  for(int i=1;i<argc;++i) {
    std::string arg=argv[i];
    if(arg=="--device" && i+1<argc) device=argv[++i];
    else if(arg=="--socket" && i+1<argc) socket_path=argv[++i];
    else if(arg=="--paired-image") paired=true;
    else if(arg=="--external-clocks") clocks=true;
    else if(arg=="--resume") resume=true;
    else {
      puts("voicedsp-pi48 --device /dev/i2c-N --socket /run/user/UID/voicedsp.sock --paired-image --external-clocks [--resume]\n48 kHz only. Default OFF/-30dB. Use kit runner to supply clocks. No flash/QSPI writes.");
      return arg=="--help"?0:2;
    }
  }
  if(device.empty() || socket_path.empty() || !paired || !clocks) return 2;
  sockaddr_un address{};address.sun_family=AF_UNIX;
  if(socket_path.size()>=sizeof(address.sun_path)) return 2;
  std::strcpy(address.sun_path,socket_path.c_str());
  // Never unlink an existing listener/path: another controller may own it.
  umask(0077);
  int listener=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_NONBLOCK|SOCK_CLOEXEC,0);
  if(listener<0 || bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address)) || listen(listener,4)) {
    perror("control socket (remove stale socket only after checking service)");if(listener>=0)close(listener);return 1;
  }
  struct sigaction action{};action.sa_handler=stop_signal;sigemptyset(&action.sa_mask);
  sigaction(SIGINT,&action,nullptr);sigaction(SIGTERM,&action,nullptr);
  vdsp_linux bus{};bus.fd=-1;bus.external_clocks=true;bus.cancel=&quitting;
  Runtime runtime;
  auto ops=vdsp_linux_ops(&bus);
  const vdsp_v2_policy_limits limits{500,100,65000,15000,9000,5000};
  int result=vdsp_v2_host_init(&runtime.host,&ops,48000,&limits,true);
  uint32_t cookie{};
  if(!result && getrandom(&cookie,sizeof(cookie),0)!=sizeof(cookie)) result=-1;
  if(!result) result=vdsp_linux_open(&bus,device.c_str(),250);
  if(!result) result=resume?vdsp_v2_host_resume(&runtime.host):vdsp_v2_host_start(&runtime.host,cookie);
  vdsp_v2_gesture gesture{};
  vdsp_v2_gesture_init(&gesture,ops.context,ops.transfer,ops.now_ms);
  GestureActions gestures;
  uint32_t seen=0,last_status=0;unsigned buttons=0;
  if(!result) result=runtime.apply();
  while(!result && !quitting) {
    result=vdsp_v2_host_poll(&runtime.host);
    if(result) break;
    runtime.step_ui();
    if(runtime.host.audio.route!=VDSP_AUDIO_OFF && !runtime.ui.snapshot().aec.profile_valid) {
      result=-3;break;
    }
    uint32_t now=ops.now_ms(ops.context);
    if(runtime.settings.gestures) vdsp_v2_gesture_step(&gesture,true,runtime.host.io.gesture_irq);
    if(gesture.snapshot.sequence!=seen) {
      seen=gesture.snapshot.sequence;
      printf("gesture=%s raw=%u\n",vdsp_v2_gesture_name(gesture.snapshot.event_raw),gesture.snapshot.event_raw);
      if(gestures.apply(gesture.snapshot.event_raw,now,runtime.settings)) result=runtime.apply();
    }
    if(buttons!=runtime.host.io.buttons_pressed) {
      buttons=runtime.host.io.buttons_pressed;printf("buttons_pressed=%u\n",buttons);
    }
    int client=accept4(listener,nullptr,nullptr,SOCK_NONBLOCK|SOCK_CLOEXEC);
    if(client>=0) {
      // One bounded packet, no slow client may stall the sole I2C owner.
      pollfd ready{client,POLLIN,0};
      char input[256];ssize_t n=poll(&ready,1,50)>0?recv(client,input,sizeof(input),0):-1;
      std::string answer;
      if(n>0 && n<ssize_t(sizeof(input)) && input[n-1]=='\n') {
        std::string line(input,size_t(n-1));
        if(line=="status") answer=runtime.status(gesture);
        else if(line=="capture raw" || line=="capture processed" || line=="capture reference" ||
                line=="capture residual" || line=="capture alignment") {
          // Sole I2C owner switches the capture mux; no timing bypass or DSP tuning.
          // Existing AecDiag automatically restores the original mux after 90 s.
          unsigned mode=line=="capture raw"?1:line=="capture reference"?2:
                        line=="capture residual"?3:line=="capture alignment"?5:0;
          runtime.capture_request=UiRuntime::next_request(runtime.capture_request,mode);
          answer=runtime.status(gesture);
        }
        else if(line=="aec-profile default" || line=="aec-profile voice" || line=="aec-profile legacy") {
          // Change the diagnostic policy only while audio is OFF. The voice
          // profile is the exact previously accepted external profile, not new tuning.
          if(runtime.host.audio.route!=VDSP_AUDIO_OFF || runtime.settings.route!=VDSP_AUDIO_OFF)
            answer="{\"error\":\"select output off before changing AEC profile\"}";
          else {
            runtime.voice_profile_override=line=="aec-profile voice";
            runtime.legacy_profile_override=line=="aec-profile legacy";
            answer=runtime.status(gesture);
          }
        }
        else if(command(line,runtime.settings)) {
          result=runtime.apply();answer=result?"{\"error\":\"apply failed; see journal\"}":runtime.status(gesture);
        } else answer="{\"error\":\"invalid command\"}";
      } else answer="{\"error\":\"send one complete command (max 254 bytes)\"}";
      answer+='\n';send(client,answer.data(),answer.size(),MSG_NOSIGNAL);close(client);
    }
    if(uint32_t(now-last_status)>2000) { puts(runtime.status(gesture).c_str());last_status=now; }
    fflush(stdout);ops.wait_ms(ops.context,20);
  }
  if(runtime.host.claimed_bus) {
    // Shutdown must not be cancelled by the signal that requested it.
    bus.cancel=nullptr;
    int off=vdsp_v2_host_stop(&runtime.host);
    if(off) fprintf(stderr,"Output shutdown UNCONFIRMED error=%d\n",off);
    if(!result) result=off;
  }
  vdsp_linux_close(&bus);close(listener);unlink(socket_path.c_str());
  if(result) fprintf(stderr,"Controller stopped error=%d; no automatic recovery\n",result);
  return result?1:0;
}
