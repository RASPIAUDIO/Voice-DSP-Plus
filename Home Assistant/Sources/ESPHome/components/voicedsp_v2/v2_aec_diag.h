#pragma once
#include "voicedsp_v2_host.h"
#include <cstdint>
#include <cstring>

namespace esphome::voicedsp_v2 {
struct AecSnapshot {
  uint32_t values[24]{}, valid{}, stamp{};
  uint8_t mux[4]{};
  bool mux_valid{}, raw{};
  int error{};
  unsigned profile{};
  bool profile_valid{};
  int profile_error{};
  int32_t asr_output{},system_delay{};
  unsigned metadata_valid{};
};
class AecDiag {
 public:
  AecSnapshot snapshot() const { return s_; }
  void step(vdsp_v2_host &h, uint32_t request, uint32_t profile_request = 0, unsigned fallback_profile = 0) {
    if (h.stage != VDSP_HOST_READY || !h.session_valid || !h.clocks || h.last_error) {
      s_.profile_valid=false; s_.mux_valid=false; return;
    }
    uint32_t now=h.ops.now_ms(h.ops.context);
    if (request != request_) { request_=request; requested_ms_=now; }
    if (now-last_ms_ < 100) return;
    last_ms_=now;
    if(profile_request != profile_request_) {
      profile_request_=profile_request; profile_ms_=now;
    }
    unsigned profile=profile_request && now-profile_ms_<180000 ? (profile_request&255) : fallback_profile;
    if(profile>6) { s_.profile_error=-6; s_.profile_valid=false; return; }
    if(!profile_step(h,profile)) return;
    unsigned mode=now-requested_ms_<90000 ? request&255 : 0;
    if(mode>5)mode=0;
    bool raw=mode==1;
    if (!have_original_) {
      int rc=read(h,0x23,15,original_,2);
      if(!rc) rc=read(h,0x23,19,original_+2,2);
      s_.error=rc;
      if(rc) return;
      have_original_=true;
    }
    uint8_t desired[4]={3,0,3,1};
    if(!mode) std::memcpy(desired,original_,4);
    // IDs from the compiled device_enums.h, NOT initializer declaration order.
    if(mode==2) { desired[0]=desired[2]=12;desired[1]=desired[3]=0; }
    if(mode==3) { desired[0]=desired[2]=7; }
    if(mode==4) { desired[0]=desired[2]=6;desired[1]=desired[3]=3; }
    if(mode==5) { desired[0]=3;desired[1]=0;desired[2]=12;desired[3]=0; }
    if(!s_.mux_valid || std::memcmp(s_.mux,desired,4)) {
      s_.mux_valid=false;
      for(unsigned side=0;side<2;++side) {
        uint8_t cmd=side?19:15;
        int rc=read(h,0x23,cmd,s_.mux+2*side,2);
        if(!rc && std::memcmp(s_.mux+2*side,desired+2*side,2)) {
          const uint8_t packet[]={0x23,cmd,2,desired[2*side],desired[2*side+1]};
          rc=h.ops.transfer(h.ops.context,0x2c,packet,5,nullptr,0);
          h.ops.wait_ms(h.ops.context,2);
          uint8_t status=255;
          if(!rc) rc=h.ops.transfer(h.ops.context,0x2c,nullptr,0,&status,1);
          if(!rc) rc=status;
          // Absolute, idempotent mux selection; always verify, never accept ACK alone.
          if(!rc) rc=read(h,0x23,cmd,s_.mux+2*side,2);
          if(!rc && std::memcmp(s_.mux+2*side,desired+2*side,2)) rc=-4;
        }
        s_.error=rc;
        if(rc) return;
      }
      s_.raw=raw; s_.mux_valid=true;
    }
    // Read-only snapshot of actual runtime parameters. No SHF_BYPASS writes.
    static const uint8_t fields[24][3]={
      {0x21,70,1},{0x21,5,4},{0x21,3,4},{0x21,0,4},
      {0x11,21,4},{0x11,22,4},{0x11,32,4},{0x11,23,4},
      {0x11,27,4},{0x11,28,4},{0x11,31,4},{0x11,24,4},
      {0x11,25,4},{0x11,26,4},{0x11,10,4},{0x11,13,4},
      {0x11,19,4},{0x11,20,4},{0x23,0,4},{0x23,1,4},
      {0x23,27,1},{0x11,11,4},{0x11,12,4},{0x21,71,4}};
    unsigned i=index_++%26; uint32_t value=0;
    if(i>=24) {
      int rc=read(h,i==24?0x21:0x23,i==24?35:26,reinterpret_cast<uint8_t*>(&value),4);
      unsigned bit=1u<<(i-24);
      if(!rc) { if(i==24)s_.asr_output=static_cast<int32_t>(value);else s_.system_delay=static_cast<int32_t>(value);s_.metadata_valid|=bit; }
      else s_.metadata_valid&=~bit;
      s_.error=rc;s_.stamp=now;return;
    }
    int rc=read(h,fields[i][0],fields[i][1],reinterpret_cast<uint8_t*>(&value),fields[i][2]);
    if(!rc) { s_.values[i]=value; s_.valid|=1u<<i; } else s_.valid&=~(1u<<i);
    s_.error=rc; s_.stamp=now;
  }
 private:
  // Bounded diagnostic profiles; all changed values are saved and restored.
  bool profile_step(vdsp_v2_host &h,unsigned profile) {
    static const uint8_t commands[8]={21,31,10,13,23,27,22,5};
    static const uint8_t resources[8]={0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x21};
    if(!profile_saved_) {
      for(unsigned i=0;i<8;++i) {
        int rc=read(h,resources[i],commands[i],reinterpret_cast<uint8_t*>(&profile_original_[i]),4);
        if(rc) { s_.profile_error=rc; return false; }
      }
      profile_saved_=true;
    }
    const uint32_t now=h.ops.now_ms(h.ops.context);
    if(s_.profile_valid && s_.profile==profile && now-profile_verified_ms_<5000) return true;
    s_.profile_valid=false;
    uint32_t wanted[8];std::memcpy(wanted,profile_original_,sizeof(wanted));
    if(profile==1) { float ns=.10f;std::memcpy(wanted,&ns,4); }
    if(profile==1 || profile==2) wanted[1]=10;
    if(profile==1 || profile==3) {
      wanted[2]=0;float gain=10;std::memcpy(wanted+3,&gain,4);
    }
    if(profile==4 || profile==5) {
      float ns=profile==4?.10f:1.f,gain=5.f;
      std::memcpy(wanted,&ns,4);wanted[1]=10;wanted[2]=0;
      std::memcpy(wanted+3,&gain,4);wanted[4]=0;
      if(profile==4) {
        float far=-24,nn=.51f;std::memcpy(wanted+7,&far,4);
        wanted[5]=1;std::memcpy(wanted+6,&nn,4);
      }
      else { wanted[5]=0;float nn=1;std::memcpy(wanted+6,&nn,4); }
    }
    if(profile==6) {
      // Explicit current V2 baseline, not a snapshot that may already be tuned
      // after an ESP-only restart. Used outside the external-speaker route.
      float ns=.15f,nn=.51f,far=0.f;
      std::memcpy(wanted,&ns,4);wanted[1]=0;wanted[2]=1;
      wanted[4]=1;wanted[5]=1;std::memcpy(wanted+6,&nn,4);
      std::memcpy(wanted+7,&far,4);
    }
    for(unsigned i=0;i<8;++i) {
      if(i==3 && wanted[2]) continue; // AGC gain is adaptive when enabled.
      uint32_t actual=0;int rc=read(h,resources[i],commands[i],reinterpret_cast<uint8_t*>(&actual),4);
      if(!rc && actual!=wanted[i]) {
        rc=write_setting(h,resources[i],commands[i],wanted[i]);
        if(!rc) rc=read(h,resources[i],commands[i],reinterpret_cast<uint8_t*>(&actual),4);
        if(!rc && actual!=wanted[i]) rc=-4;
      }
      if(rc) { s_.profile_error=rc;return false; }
    }
    s_.profile=profile;s_.profile_valid=true;s_.profile_error=0;profile_verified_ms_=now;return true;
  }
  static int write_setting(vdsp_v2_host &h,uint8_t resource,uint8_t cmd,uint32_t value) {
    uint8_t packet[7]={resource,cmd,4};std::memcpy(packet+3,&value,4);
    for(unsigned n=0;n<20;++n) {
      if(h.ops.cancelled && h.ops.cancelled(h.ops.context)) return -2;
      int rc=h.ops.transfer(h.ops.context,0x2c,packet,7,nullptr,0);
      if(rc)return rc;
      h.ops.wait_ms(h.ops.context,5);
      uint8_t status=255;rc=h.ops.transfer(h.ops.context,0x2c,nullptr,0,&status,1);
      if(rc)return rc;
      if(!status){h.ops.wait_ms(h.ops.context,5);return 0;}
      if(status!=64 && status!=68)return status;
    }
    return 64;
  }
  static int read(vdsp_v2_host &h,uint8_t resource,uint8_t cmd,uint8_t *data,unsigned count) {
    const uint8_t packet[]={resource,static_cast<uint8_t>(cmd|128),static_cast<uint8_t>(count+1)};
    uint8_t reply[5]{};
    for(unsigned n=0;n<20;++n) {
      if(h.ops.cancelled && h.ops.cancelled(h.ops.context)) return -2;
      int rc=h.ops.transfer(h.ops.context,0x2c,packet,3,reply,count+1);
      if(rc) return rc;
      if(!reply[0]) { std::memcpy(data,reply+1,count); return 0; }
      if(reply[0]!=64) return reply[0];
      h.ops.wait_ms(h.ops.context,10);
    }
    return 64;
  }
  AecSnapshot s_{};
  uint8_t original_[4]{};
  bool have_original_{};
  unsigned index_{};
  uint32_t request_{}, requested_ms_{}, last_ms_{};
  uint32_t profile_request_{},profile_ms_{},profile_original_[8]{};
  uint32_t profile_verified_ms_{};
  bool profile_saved_{};
};
}
