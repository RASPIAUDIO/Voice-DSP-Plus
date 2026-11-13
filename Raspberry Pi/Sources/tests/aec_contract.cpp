#include "v2_aec_diag.h"
#include "voicedsp_v2_equalizer.h"
#include <cassert>
#include <cstdio>
#include <cstring>
static uint32_t registers_[256][128], now_=1000;
static uint32_t now(void*) { return now_; }
static void wait(void*,unsigned ms) { now_+=ms; }
static int transfer(void*,uint8_t address,const uint8_t *tx,size_t nt,uint8_t *rx,size_t nr)
{
    assert(address==0x2c);
    if(!nt && nr==1) { rx[0]=0; return 0; }
    assert(nt>=3 && tx[2]<=5);
    if(tx[1]&0x80) {
        assert(nt==3 && nr==tx[2]);rx[0]=0;
        std::memcpy(rx+1,&registers_[tx[0]][tx[1]&127],nr-1);
    } else {
        assert(!nr && nt==3+tx[2]);
        std::memcpy(&registers_[tx[0]][tx[1]],tx+3,tx[2]);
    }
    return 0;
}
static float f(unsigned resource,unsigned command)
{ float v;std::memcpy(&v,&registers_[resource][command],4);return v; }
int main()
{
    vdsp_v2_host h{};
    h.stage=VDSP_HOST_READY;h.session_valid=h.clocks=true;
    h.ops.transfer=transfer;h.ops.now_ms=now;h.ops.wait_ms=wait;
    registers_[0x23][15]=8;registers_[0x23][19]=8|(1u<<8);
    esphome::voicedsp_v2::AecDiag aec;
    aec.step(h,0,0,4);
    assert(aec.snapshot().profile_valid && aec.snapshot().profile==4);
    assert(f(0x11,21)==.10f && f(0x11,22)==.51f && f(0x21,5)==-24.f);
    assert(registers_[0x11][31]==10 && !registers_[0x11][10]);
    assert(f(0x11,13)==5.f && !registers_[0x11][23] && registers_[0x11][27]==1);
    now_+=6000;aec.step(h,0,0,6);
    assert(aec.snapshot().profile_valid && aec.snapshot().profile==6);
    assert(f(0x11,21)==.15f && f(0x21,5)==0.f && registers_[0x11][10]==1);
    now_+=6000;aec.step(h,0,0,4);
    assert(aec.snapshot().profile_valid && f(0x21,5)==-24.f && f(0x11,13)==5.f);
    const vdsp_v2_crossover_config config{9500,0,-6,0};
    vdsp_v2_equalizer_pair eq{};
    assert(!vdsp_v2_crossover_prepare(&config,0,&eq));
    assert(eq.volume_half_db[0]==0 && eq.volume_half_db[1]==0);
    now_+=1000;aec.step(h,0x101,0,4);
    assert(aec.snapshot().raw && aec.snapshot().mux_valid);
    assert(registers_[0x23][15]==3 && registers_[0x23][19]==(3|(1u<<8)));
    assert(registers_[0x21][70]==0); // Never SHF_BYPASS.
    now_+=91000;aec.step(h,0x101,0,4);
    assert(!aec.snapshot().raw && registers_[0x23][15]==8);
    assert(registers_[0x23][19]==(8|(1u<<8)));
    assert(f(0x21,5)==-24.f && f(0x11,21)==.10f);
    puts("PASS: real accepted AEC controller profile4 -> profile6 -> profile4; fixed crossover, no hidden overall pad");
}
