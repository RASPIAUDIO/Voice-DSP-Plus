#include "voicedsp_v2_host_boot.h"
#include <string.h>
uint8_t vdsp_v2_host_record[VDSP_HOST_BOOT_SIZE];
volatile uint32_t vdsp_v2_host_i2s_frames;
uint16_t vdsp_v2_boot_crc(const uint8_t *data, size_t n)
{
    uint16_t crc = 0xffff;
    for (size_t i = 0; i < n; ++i) {
        crc ^= (uint16_t)((unsigned)data[i] << 8);
        for (unsigned b = 0; b < 8; ++b)
            crc = (uint16_t)((crc & 0x8000) ? ((unsigned)crc << 1) ^ 0x1021 : (unsigned)crc << 1);
    }
    return crc;
}
static bool valid(const vdsp_v2_host_boot *s)
{
    const uint8_t *p = s->packet;
    uint16_t expected = (uint16_t)(p[14] | ((unsigned)p[15] << 8));
    return !memcmp(p,"V2CF",4) && p[4] == 1 && p[8] == s->rate_khz && p[9] == 3 &&
        p[5] != VDSP_MIC_UNKNOWN && p[5] == vdsp_v2_decode_microphones(true,p[6],p[7]) &&
        !(p[6] & VDSP_V2_AMP_ENABLE) && vdsp_v2_boot_crc(p,14) == expected;
}
int vdsp_v2_host_boot_init(vdsp_v2_host_boot *s, unsigned rate)
{
    if (!s || (rate != 16 && rate != 48)) return -2;
    *s = (vdsp_v2_host_boot){.rate_khz = rate};
    return 0;
}
void vdsp_v2_host_boot_write_begin(vdsp_v2_host_boot *s)
{ s->received = 0; s->writing = !s->committed; s->overflow = false; }
bool vdsp_v2_host_boot_receive(vdsp_v2_host_boot *s, uint8_t value)
{
    if (!s->writing || s->overflow || s->received >= VDSP_HOST_BOOT_SIZE) {
        s->overflow = true; return false;
    }
    unsigned i = s->received++;
    s->packet[i] = value;
    if ((i < 4 && value != (uint8_t)"V2CF"[i]) || (i == 4 && value != 1) ||
        (s->received == VDSP_HOST_BOOT_SIZE && !valid(s))) {
        s->overflow = true; return false;
    }
    return true;
}
void vdsp_v2_host_boot_stop(vdsp_v2_host_boot *s)
{
    if (s->writing && !s->overflow && s->received == VDSP_HOST_BOOT_SIZE && valid(s)) {
        memcpy(s->accepted,s->packet,VDSP_HOST_BOOT_SIZE);
        s->committed = true;
    }
    s->writing = false;
}
void vdsp_v2_host_boot_read_begin(vdsp_v2_host_boot *s)
{ s->read_index = 0; s->writing = false; }
uint8_t vdsp_v2_host_boot_read(vdsp_v2_host_boot *s)
{
    uint8_t message[VDSP_HOST_BOOT_SIZE] = {'V','2','B','T',1,0,6,0};
    message[8] = (uint8_t)s->rate_khz;
    message[9] = (uint8_t)s->committed;
    uint16_t crc = vdsp_v2_boot_crc(message,14);
    message[14] = (uint8_t)crc; message[15] = (uint8_t)(crc >> 8);
    return s->read_index < VDSP_HOST_BOOT_SIZE ? message[s->read_index++] : 0xff;
}
