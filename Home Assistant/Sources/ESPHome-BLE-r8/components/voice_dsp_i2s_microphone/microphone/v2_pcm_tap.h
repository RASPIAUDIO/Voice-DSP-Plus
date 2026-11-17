#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <lwip/sockets.h>
#include <fcntl.h>
#include "esphome/core/hal.h"

/* Bounded, opt-in LAN diagnostic. No recording at boot; expires in 90 s.
 * All socket operations are nonblocking and owned by the microphone task.
 * A partial send closes the stream so corrupted/gapped audio cannot look valid.
 */
class V2PcmTap {
 public:
  void arm(bool stereo=false) { stereo_.store(stereo); until_.store(esphome::millis()+90000); }
  void stop() { until_.store(0); }
  void push(const uint8_t *pcm,size_t bytes,size_t channels) {
    uint32_t now=esphome::millis(), until=until_.load();
    if(!until || static_cast<int32_t>(until-now)<=0) { close_all(); return; }
    if(server_<0) {
      server_=socket(AF_INET,SOCK_STREAM,0);
      if(server_<0) return;
      int yes=1; setsockopt(server_,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
      fcntl(server_,F_SETFL,O_NONBLOCK);
      sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons(6969); a.sin_addr.s_addr=INADDR_ANY;
      if(bind(server_,reinterpret_cast<sockaddr*>(&a),sizeof(a)) || listen(server_,1)) { close_all(); return; }
    }
    if(client_<0) {
      client_=accept(server_,nullptr,nullptr);
      if(client_<0) return;
      fcntl(client_,F_SETFL,O_NONBLOCK); sequence_=frames_=0;
      output_channels_=stereo_.load()?2:1;
    }
    if(channels!=2 || bytes%8) { close_all(); return; }
    size_t count=bytes/8;
    for(size_t offset=0;offset<count;) {
      unsigned n=std::min<size_t>(count-offset,256);
      uint8_t packet[16+1024];
      uint32_t header[]={output_channels_==2?0x56445044u:0x56445043u,sequence_++,frames_,n};
      std::memcpy(packet,header,16);
      for(unsigned i=0;i<n;++i) {
        for(unsigned c=0;c<output_channels_;++c) {
          int32_t value; std::memcpy(&value,pcm+8*(offset+i)+4*c,4);
          int16_t sample=static_cast<int16_t>(value>>16);
          std::memcpy(packet+16+2*(output_channels_*i+c),&sample,2);
        }
      }
      frames_+=n; offset+=n;
      const unsigned packet_bytes=16+2*n*output_channels_;
      if(send(client_,packet,packet_bytes,MSG_DONTWAIT)!=static_cast<int>(packet_bytes)) {
        close(client_); client_=-1; return;
      }
    }
  }
 private:
  void close_all() {
    if(client_>=0) { close(client_); client_=-1; }
    if(server_>=0) { close(server_); server_=-1; }
  }
  std::atomic<uint32_t> until_{0};
  std::atomic<bool> stereo_{false};
  unsigned output_channels_{1};
  int server_{-1},client_{-1};
  uint32_t sequence_{},frames_{};
};
