// Copyright 2026 RASPIAUDIO

#ifndef PI_AI_MIC_NEOPIXEL_H_
#define PI_AI_MIC_NEOPIXEL_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PI_AI_MIC_UI_DOA_IDLE = 0,
    PI_AI_MIC_UI_INIT = 1,
    PI_AI_MIC_UI_BLE_IMPROV = 2,
    PI_AI_MIC_UI_NO_HA = 3,
    PI_AI_MIC_UI_WAITING = 4,
    PI_AI_MIC_UI_LISTENING = 5,
    PI_AI_MIC_UI_THINKING = 6,
    PI_AI_MIC_UI_REPLYING = 7,
    PI_AI_MIC_UI_ERROR = 8,
    PI_AI_MIC_UI_MUTED = 9,
    PI_AI_MIC_UI_JACK_IN = 10,
    PI_AI_MIC_UI_JACK_OUT = 11,
    PI_AI_MIC_UI_FLASHING = 12,
    PI_AI_MIC_UI_FLASH_OK = 13,
    PI_AI_MIC_UI_FLASH_ERROR = 14,
    PI_AI_MIC_UI_VOLUME = 15,
    PI_AI_MIC_UI_STATE_COUNT
} pi_ai_mic_ui_state_t;

void pi_ai_mic_neopixel_publish_doa(float azimuth_rad, float speech_energy);
void pi_ai_mic_ui_set_state(uint8_t state);
uint8_t pi_ai_mic_ui_get_state(void);
void pi_ai_mic_ui_set_progress(uint8_t progress);
uint8_t pi_ai_mic_ui_get_progress(void);
void pi_ai_mic_ui_set_style(unsigned index, uint8_t value);
uint8_t pi_ai_mic_ui_get_style(unsigned index);
void pi_ai_mic_set_mic_mute(bool muted);
bool pi_ai_mic_get_mic_mute(void);
int pi_ai_mic_neopixel_start_task(void);

#endif /* PI_AI_MIC_NEOPIXEL_H_ */
