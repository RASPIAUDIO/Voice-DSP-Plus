/* Voice DSP+ V2 capture bring-up. Deliberately no playback or runtime I2C.
 * Function names retained only for the inherited application ABI.
 */
#include "pi_ai_mic_hw.h"
#include "voicedsp_v2_i2c.h"
#include "app_conf.h"
#include "device_enums.h"
#include "platform/driver_instances.h"
#include "rtos_printf.h"
#include "xcore/hwtimer.h"
#include "xcore/port.h"
#include <stddef.h>

#if !VOICEDSP_V2_CAPTURE
#error This overlay is V2-only; configure with VOICEDSP_V2_CAPTURE=ON
#endif

volatile int pi_ai_mic_output_route_speaker_active = 0;
static vdsp_v2_mic_array boot_geometry = VDSP_MIC_UNKNOWN;
/* Symbols retained in the debugger, including failure return codes. */
volatile int vdsp_v2_boot_pcal_result = -999;
volatile int vdsp_v2_boot_dac_reset_result = -999;
volatile uint8_t vdsp_v2_boot_port0;
volatile uint8_t vdsp_v2_boot_port1;



void vdsp_v2_set_boot_geometry(uint8_t geometry)
{
    boot_geometry = (geometry >= VDSP_MIC_INTERNAL_SQUARE &&
                     geometry <= VDSP_MIC_EXTERNAL_LINEAR) ?
                     (vdsp_v2_mic_array)geometry : VDSP_MIC_UNKNOWN;
}
uint8_t vdsp_v2_get_boot_geometry(void) { return (uint8_t)boot_geometry; }

#include "voicedsp_v2_host_boot.inc"

/* There is intentionally no run-time master: the Pi control endpoint owns the
 * ports after startup. USB capture uses only the boot snapshot as well. These
 * wrappers reject obsolete V1 writes instead of silently emulating PCAL6408.
 */
int pi_ai_mic_i2c_forced_reg_write(uint8_t a, uint8_t r, uint8_t v)
{ (void)a; (void)r; (void)v; return -1; }
int pi_ai_mic_i2c_forced_reg_read(uint8_t a, uint8_t r, uint8_t *v)
{ (void)a; (void)r; (void)v; return -1; }
int pi_ai_mic_expander_init(void) { return -1; }
int pi_ai_mic_set_dac_reset_n(bool level) { (void)level; return -1; }
int pi_ai_mic_set_int_n(bool level) { (void)level; return -1; }
int pi_ai_mic_set_amp_mode(pi_ai_mic_amp_mode_t mode) { (void)mode; return -1; }
int pi_ai_mic_configure_codec_output(bool jack) { (void)jack; return -1; }
int pi_ai_mic_apply_output_route(bool jack, bool speaker)
{ (void)jack; (void)speaker; return -1; }
int pi_ai_mic_probe_i2c_devices(void) { return -1; }
int pi_ai_mic_debug_dump_i2c_state(void) { return -1; }
int pi_ai_mic_read_detect_state(bool *valid, bool *jack, bool *square)
{ if (valid) *valid = false; if (jack) *jack = true; if (square) *square = false; return -1; }
int pi_ai_mic_read_detect_input(bool *valid, bool *jack, bool *square)
{ return pi_ai_mic_read_detect_state(valid, jack, square); }
bool pi_ai_mic_jack_inserted(bool *valid)
{ if (valid) *valid = false; return true; }
bool pi_ai_mic_mic_geometry_cached_square(void)
{ return boot_geometry == VDSP_MIC_INTERNAL_SQUARE || boot_geometry == VDSP_MIC_EXTERNAL_SQUARE; }
bool pi_ai_mic_mic_geometry_square(bool *valid)
{ if (valid) *valid = boot_geometry != VDSP_MIC_UNKNOWN; return pi_ai_mic_mic_geometry_cached_square(); }
int pi_ai_mic_update_mic_geometry_from_strap(void)
{ return boot_geometry == VDSP_MIC_UNKNOWN ? -1 : 0; }
void pi_ai_mic_set_mic_geometry_cached(bool valid, bool square)
{ (void)valid; (void)square; /* V1 boolean cannot encode the two different squares. */ }
void pi_ai_mic_wait_for_mic_geometry_ready(void) { /* Transferred before task start. */ }
int pi_ai_mic_get_mic_array_type(void)
{ return pi_ai_mic_mic_geometry_cached_square() ? BECLEAR_CIRCULAR_ARRAY : BECLEAR_LINEAR_ARRAY; }
bool pi_ai_mic_doa_raw_ring_enabled(void) { return false; }
bool pi_ai_mic_raw_doa_pcb_order(void) { return true; }
int pi_ai_mic_raw_doa_delay_sign(void) { return 1; }
bool pi_ai_mic_raw_doa_abs_correlation(void) { return false; }
bool pi_ai_mic_doa_ring_offset_180(void) { return false; }
bool pi_ai_mic_doa_neopixel_mirror(void) { return false; }
bool pi_ai_mic_doa_neopixel_reverse(void) { return false; }
bool pi_ai_mic_doa_opposite_filter_enabled(void) { return false; }
int pi_ai_mic_doa_opposite_stable_frames(void) { return 30; }
