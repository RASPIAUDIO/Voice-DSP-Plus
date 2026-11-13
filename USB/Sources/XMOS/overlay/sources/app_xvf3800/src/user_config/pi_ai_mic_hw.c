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

#if ON_TILE(0)
static vdsp_v2_sample boot_sample;
typedef struct { hwtimer_t timer; } xmos_bus_context;
static port_t bus_port(unsigned pin) { return pin == VDSP_SCL ? PORT_I2C_SCL : PORT_I2C_SDA; }
VDSP_FPTR("vdsp_bus_output")
static void bus_low(void *ctx, unsigned pin)
{
    (void)ctx;
    port_out(bus_port(pin), 0);
    port_sync(bus_port(pin));
}
VDSP_FPTR("vdsp_bus_output")
static void bus_release(void *ctx, unsigned pin)
{
    (void)ctx;
    /* Port remains in DRIVE_PULL_UP (drive-low) mode: 1 is high impedance. */
    port_out(bus_port(pin), 1);
    port_sync(bus_port(pin));
}
VDSP_FPTR("vdsp_bus_input")
static bool bus_level(void *ctx, unsigned pin)
{
    (void)ctx;
    return (port_peek(bus_port(pin)) & 1u) != 0;
}
VDSP_FPTR("vdsp_bus_delay")
static void bus_delay(void *ctx)
{
    /* 100 MHz reference timer: 5 us half-period, at most 100 kHz. */
    hwtimer_delay(((xmos_bus_context *)ctx)->timer, 500u);
}
VDSP_FPTR("vdsp_reg_write")
static int register_write(void *ctx, uint8_t addr, uint8_t reg, uint8_t value)
{
    return vdsp_v2_i2c_write((vdsp_v2_i2c *)ctx, addr, reg, value);
}
VDSP_FPTR("vdsp_reg_read")
static int register_read(void *ctx, uint8_t addr, uint8_t reg, uint8_t *value)
{
    return vdsp_v2_i2c_read((vdsp_v2_i2c *)ctx, addr, reg, value);
}
#endif

void vdsp_v2_set_boot_geometry(uint8_t geometry)
{
    boot_geometry = (geometry >= VDSP_MIC_INTERNAL_SQUARE &&
                     geometry <= VDSP_MIC_EXTERNAL_LINEAR) ?
                     (vdsp_v2_mic_array)geometry : VDSP_MIC_UNKNOWN;
}
uint8_t vdsp_v2_get_boot_geometry(void) { return (uint8_t)boot_geometry; }

void vdsp_v2_pre_boot(void)
{
#if ON_TILE(0)
    xmos_bus_context context = {hwtimer_alloc()};
    vdsp_v2_i2c bus = {&context, bus_low, bus_release, bus_level, bus_delay, 1000};
    vdsp_v2_control control = {&bus, register_write, register_read, false, false};
    unsigned consecutive = 0;
    vdsp_v2_mic_array previous = VDSP_MIC_UNKNOWN;
    vdsp_v2_set_boot_geometry(VDSP_MIC_UNKNOWN);
    port_enable(PORT_I2C_SCL);
    port_enable(PORT_I2C_SDA);
    port_write_control_word(PORT_I2C_SCL, XS1_SETC_DRIVE_PULL_UP);
    port_write_control_word(PORT_I2C_SDA, XS1_SETC_DRIVE_PULL_UP);
    bus_release(&context, VDSP_SDA);
    bus_release(&context, VDSP_SCL);
    hwtimer_delay(context.timer, 1000000u);

    vdsp_v2_boot_pcal_result = vdsp_v2_control_init(&control);
    if (vdsp_v2_boot_pcal_result == 0) {
        for (unsigned i = 0; i < 8; ++i) {
            if (vdsp_v2_control_sample(&control, &boot_sample)) {
                consecutive = 0;
                previous = VDSP_MIC_UNKNOWN;
            } else {
                consecutive = boot_sample.microphone == previous ? consecutive + 1 : 1;
                previous = boot_sample.microphone;
                vdsp_v2_boot_port0 = boot_sample.port0;
                vdsp_v2_boot_port1 = boot_sample.port1;
            }
            if (consecutive >= 4) {
                vdsp_v2_set_boot_geometry((uint8_t)previous);
                break;
            }
            hwtimer_delay(context.timer, 1000000u);
        }
        /* Reset the codec to powered-down defaults; no speaker/HP register setup.
         * Shared DAC_RST_N remains an INPUT at PCAL, never a competing output.
         */
        /* DAC_RST_N has a pulldown: release it before accessing the codec.
         * PCAL P0.2 remains input; only the XMOS drives the shared net. */
        port_enable(GPO_TILE_0_PORT_8C);
        port_out(GPO_TILE_0_PORT_8C, 0xffu & ~(1u << 3));
        port_sync(GPO_TILE_0_PORT_8C);
        hwtimer_delay(context.timer, 1000000u);
        port_out(GPO_TILE_0_PORT_8C, 0xffu);
        port_sync(GPO_TILE_0_PORT_8C);
        hwtimer_delay(context.timer, 1000000u);
        vdsp_v2_boot_dac_reset_result = vdsp_v2_i2c_write(&bus, 0x18, 0x00, 0x00);
        if (vdsp_v2_boot_dac_reset_result == 0)
            vdsp_v2_boot_dac_reset_result = vdsp_v2_i2c_write(&bus, 0x18, 0x01, 0x01);
        /* Capture-only: reassert reset, then release the port to the runtime
         * GPO owner. No analog output is needed by this diagnostic. */
        port_out(GPO_TILE_0_PORT_8C, 0xffu & ~(1u << 3));
        port_sync(GPO_TILE_0_PORT_8C);
        port_disable(GPO_TILE_0_PORT_8C);
    }
    pi_ai_mic_boot_detect_valid = boot_geometry != VDSP_MIC_UNKNOWN;
    pi_ai_mic_boot_detect_jack = !boot_sample.valid ||
        (((boot_sample.port0 >> 5) & 1u) == PI_AI_MIC_JACK_INSERTED_LEVEL);
    pi_ai_mic_boot_detect_mic_square = boot_geometry == VDSP_MIC_INTERNAL_SQUARE ||
                                      boot_geometry == VDSP_MIC_EXTERNAL_SQUARE;
    bus_release(&context, VDSP_SDA);
    bus_release(&context, VDSP_SCL);
    port_disable(PORT_I2C_SDA);
    port_disable(PORT_I2C_SCL);
    hwtimer_free(context.timer);
#endif
}

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

#include "vdsp_usb_runtime.inc"
