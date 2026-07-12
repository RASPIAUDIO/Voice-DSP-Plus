// Copyright 2026 RASPIAUDIO

#ifndef PI_AI_MIC_HW_H_
#define PI_AI_MIC_HW_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PI_AI_MIC_AMP_SHUTDOWN = 0,
    PI_AI_MIC_AMP_CLASS_AB_BOOST_OFF,
    PI_AI_MIC_AMP_CLASS_D_BOOST_ACF,
    PI_AI_MIC_AMP_CLASS_D_FULL,
} pi_ai_mic_amp_mode_t;

int pi_ai_mic_i2c_forced_reg_write(uint8_t addr, uint8_t reg, uint8_t val);
int pi_ai_mic_i2c_forced_reg_read(uint8_t addr, uint8_t reg, uint8_t *val);
extern volatile int pi_ai_mic_boot_detect_valid;
extern volatile int pi_ai_mic_boot_detect_jack;
extern volatile int pi_ai_mic_boot_detect_mic_square;
extern volatile int pi_ai_mic_output_route_speaker_active;
int pi_ai_mic_expander_init(void);
int pi_ai_mic_set_dac_reset_n(bool level);
int pi_ai_mic_set_int_n(bool level);
int pi_ai_mic_set_amp_mode(pi_ai_mic_amp_mode_t mode);
int pi_ai_mic_read_detect_state(bool *valid, bool *jack_inserted, bool *mic_square);
int pi_ai_mic_read_detect_input(bool *valid, bool *jack_inserted, bool *mic_square);
bool pi_ai_mic_jack_inserted(bool *valid);
bool pi_ai_mic_mic_geometry_square(bool *valid);
int pi_ai_mic_update_mic_geometry_from_strap(void);
void pi_ai_mic_set_mic_geometry_cached(bool valid, bool square);
void pi_ai_mic_wait_for_mic_geometry_ready(void);
bool pi_ai_mic_mic_geometry_cached_square(void);
int pi_ai_mic_get_mic_array_type(void);
bool pi_ai_mic_doa_raw_ring_enabled(void);
bool pi_ai_mic_raw_doa_pcb_order(void);
int pi_ai_mic_raw_doa_delay_sign(void);
bool pi_ai_mic_raw_doa_abs_correlation(void);
bool pi_ai_mic_doa_ring_offset_180(void);
bool pi_ai_mic_doa_neopixel_mirror(void);
bool pi_ai_mic_doa_neopixel_reverse(void);
bool pi_ai_mic_doa_opposite_filter_enabled(void);
int pi_ai_mic_doa_opposite_stable_frames(void);
int pi_ai_mic_probe_i2c_devices(void);
int pi_ai_mic_configure_codec_output(bool jack_inserted);
int pi_ai_mic_apply_output_route(bool jack_inserted, bool speaker_enabled_when_no_jack);
int pi_ai_mic_debug_dump_i2c_state(void);

#endif /* PI_AI_MIC_HW_H_ */
