// Copyright 2026 RASPIAUDIO

#include "pi_ai_mic_hw.h"

#include <stdint.h>

#include "app_conf.h"
#include "dac3101.h"
#include "platform/driver_instances.h"
#include "rtos_i2c_master.h"
#include "rtos_printf.h"
#include "xcore/hwtimer.h"
#include "xcore/port.h"

#define PCAL6408A_I2C_ADDR          0x20
#define PCAL6408A_INPUT_PORT        0x00
#define PCAL6408A_OUTPUT_PORT       0x01
#define PCAL6408A_POLARITY_INV      0x02
#define PCAL6408A_CONF              0x03
#define PCAL6408A_INPUT_LATCH       0x42
#define PCAL6408A_PULLUPDOWN_EN     0x43
#define PCAL6408A_PULLUPDOWN_SEL    0x44
#define PCAL6408A_INTERRUPT_MASK    0x45
#define PCAL6408A_OUTPUT_PORT_CONFIG 0x4F

#define BIT(n) ((uint8_t)(1u << (n)))

#define PI_AI_MIC_FORCED_I2C_HALF_PERIOD_TICKS  2500u
#define PI_AI_MIC_FORCED_I2C_ACK_RELEASE_TICKS  20u
#define PI_AI_MIC_MIC_GEOMETRY_WAIT_TICKS       100000u
#define PI_AI_MIC_MIC_GEOMETRY_WAIT_LOOPS       3000u

/*
 * DAC_RST_N is shared with PCAL P2 on the prototype. In practice the codec only
 * ACKs once P2 is driven high, so keep P2 as an output high and never pulse it
 * low from the RTOS path.
 */
#define PCAL_INPUT_MASK (BIT(PCAL_INT_N_PIN) | \
                         BIT(PCAL_JACK_DET_PIN) | \
                         BIT(PCAL_MIC_DETECT_PIN))

static uint8_t pcal_output_shadow =
    BIT(PCAL_XVF_RST_N_PIN) |
    BIT(PCAL_DAC_RST_N_PIN);

static bool pcal_ready = false;
static volatile bool mic_geometry_ready = false;
static bool mic_geometry_valid = false;
static bool mic_geometry_square = false;
volatile int pi_ai_mic_output_route_speaker_active = PI_AI_MIC_START_WITH_SPEAKER_ROUTE;

static void forced_i2c_delay(hwtimer_t timer)
{
    hwtimer_delay(timer, PI_AI_MIC_FORCED_I2C_HALF_PERIOD_TICKS);
}

static void forced_i2c_drive(port_t port, uint32_t value)
{
    port_write_control_word(port, XS1_SETC_DRIVE_DRIVE);
    port_out(port, value ? 1u : 0u);
    port_sync(port);
}

static void forced_i2c_release_for_ack(port_t port)
{
    port_out(port, 1u);
    port_write_control_word(port, XS1_SETC_DRIVE_PULL_UP);
    port_sync(port);
}

static uint32_t forced_i2c_read(port_t port)
{
    port_sync(port);
    return port_peek(port) & 1u;
}

static void forced_i2c_start(port_t scl, port_t sda, hwtimer_t timer)
{
    forced_i2c_drive(sda, 1u);
    forced_i2c_drive(scl, 1u);
    forced_i2c_delay(timer);
    forced_i2c_drive(sda, 0u);
    forced_i2c_delay(timer);
    forced_i2c_drive(scl, 0u);
    forced_i2c_delay(timer);
}

static void forced_i2c_stop(port_t scl, port_t sda, hwtimer_t timer)
{
    forced_i2c_drive(sda, 0u);
    forced_i2c_delay(timer);
    forced_i2c_drive(scl, 1u);
    forced_i2c_delay(timer);
    forced_i2c_drive(sda, 1u);
    forced_i2c_delay(timer);
}

static void forced_i2c_recover(port_t scl, port_t sda, hwtimer_t timer)
{
    forced_i2c_drive(sda, 1u);

    for (unsigned i = 0; i < 9; i++) {
        forced_i2c_drive(scl, 0u);
        forced_i2c_delay(timer);
        forced_i2c_drive(scl, 1u);
        forced_i2c_delay(timer);
    }

    forced_i2c_stop(scl, sda, timer);
}

static int forced_i2c_write_byte(port_t scl, port_t sda, hwtimer_t timer, uint8_t byte)
{
    for (int bit = 7; bit >= 0; bit--) {
        forced_i2c_drive(sda, (byte >> bit) & 1u);
        forced_i2c_delay(timer);
        forced_i2c_drive(scl, 1u);
        forced_i2c_delay(timer);
        forced_i2c_drive(scl, 0u);
        forced_i2c_delay(timer);
    }

    forced_i2c_drive(sda, 1u);
    forced_i2c_release_for_ack(sda);
    hwtimer_delay(timer, PI_AI_MIC_FORCED_I2C_ACK_RELEASE_TICKS);
    forced_i2c_drive(scl, 1u);
    forced_i2c_delay(timer);
    int ack = forced_i2c_read(sda) == 0u;
    forced_i2c_drive(scl, 0u);
    forced_i2c_delay(timer);
    forced_i2c_drive(sda, 1u);

    return ack;
}

static uint8_t forced_i2c_read_byte(port_t scl, port_t sda, hwtimer_t timer, bool ack)
{
    uint8_t byte = 0;

    for (int bit = 7; bit >= 0; bit--) {
        /* Match the validated standalone PCAL monitor: precharge SDA before
         * each sampled bit so a released/high bit is not read as stale low. */
        forced_i2c_drive(sda, 1u);
        forced_i2c_release_for_ack(sda);
        hwtimer_delay(timer, PI_AI_MIC_FORCED_I2C_ACK_RELEASE_TICKS);
        forced_i2c_drive(scl, 1u);
        forced_i2c_delay(timer);
        byte |= (uint8_t)(forced_i2c_read(sda) << bit);
        forced_i2c_drive(scl, 0u);
        forced_i2c_delay(timer);
    }

    forced_i2c_drive(sda, ack ? 0u : 1u);
    forced_i2c_delay(timer);
    forced_i2c_drive(scl, 1u);
    forced_i2c_delay(timer);
    forced_i2c_drive(scl, 0u);
    forced_i2c_delay(timer);
    forced_i2c_drive(sda, 1u);

    return byte;
}

int pi_ai_mic_i2c_forced_reg_write(uint8_t addr, uint8_t reg, uint8_t val)
{
    port_t scl = PORT_I2C_SCL;
    port_t sda = PORT_I2C_SDA;
    hwtimer_t timer = hwtimer_alloc();

    port_enable(scl);
    port_enable(sda);
    forced_i2c_drive(scl, 1u);
    forced_i2c_drive(sda, 1u);
    forced_i2c_recover(scl, sda, timer);

    rtos_printf("PI_AI_MIC forced I2C W start SCL=X0D37 SDA=X0D38 addr=0x%02x reg=0x%02x val=0x%02x\n",
                addr, reg, val);

    forced_i2c_start(scl, sda, timer);
    int ack_addr = forced_i2c_write_byte(scl, sda, timer, (uint8_t)(addr << 1));
    int ack_reg = forced_i2c_write_byte(scl, sda, timer, reg);
    int ack_val = forced_i2c_write_byte(scl, sda, timer, val);
    forced_i2c_stop(scl, sda, timer);

    forced_i2c_release_for_ack(scl);
    forced_i2c_release_for_ack(sda);
    port_disable(scl);
    port_disable(sda);
    hwtimer_free(timer);

    int ret = (ack_addr && ack_reg && ack_val) ? 0 : -1;
    rtos_printf("PI_AI_MIC forced I2C W done addr=0x%02x reg=0x%02x val=0x%02x ret=%d ack=%u/%u/%u\n",
                addr, reg, val, ret, ack_addr, ack_reg, ack_val);
    return ret;
}

int pi_ai_mic_i2c_forced_reg_read(uint8_t addr, uint8_t reg, uint8_t *val)
{
    if (val == NULL) {
        return -1;
    }

    port_t scl = PORT_I2C_SCL;
    port_t sda = PORT_I2C_SDA;
    hwtimer_t timer = hwtimer_alloc();

    port_enable(scl);
    port_enable(sda);
    forced_i2c_drive(scl, 1u);
    forced_i2c_drive(sda, 1u);
    forced_i2c_recover(scl, sda, timer);

    forced_i2c_start(scl, sda, timer);
    int ack_addr_w = forced_i2c_write_byte(scl, sda, timer, (uint8_t)(addr << 1));
    int ack_reg = forced_i2c_write_byte(scl, sda, timer, reg);
    forced_i2c_start(scl, sda, timer);
    int ack_addr_r = forced_i2c_write_byte(scl, sda, timer, (uint8_t)((addr << 1) | 0x01u));
    uint8_t read_val = forced_i2c_read_byte(scl, sda, timer, false);
    forced_i2c_stop(scl, sda, timer);

    forced_i2c_release_for_ack(scl);
    forced_i2c_release_for_ack(sda);
    port_disable(scl);
    port_disable(sda);
    hwtimer_free(timer);

    int ret = (ack_addr_w && ack_reg && ack_addr_r) ? 0 : -1;
    if (ret == 0) {
        *val = read_val;
    }
    return ret;
}

static int pi_ai_mic_i2c_forced_reg_read_stop(uint8_t addr, uint8_t reg, uint8_t *val)
{
    if (val == NULL) {
        return -1;
    }

    port_t scl = PORT_I2C_SCL;
    port_t sda = PORT_I2C_SDA;
    hwtimer_t timer = hwtimer_alloc();

    port_enable(scl);
    port_enable(sda);
    forced_i2c_drive(scl, 1u);
    forced_i2c_drive(sda, 1u);
    forced_i2c_recover(scl, sda, timer);

    forced_i2c_start(scl, sda, timer);
    int ack_addr_w = forced_i2c_write_byte(scl, sda, timer, (uint8_t)(addr << 1));
    int ack_reg = forced_i2c_write_byte(scl, sda, timer, reg);
    forced_i2c_stop(scl, sda, timer);

    forced_i2c_start(scl, sda, timer);
    int ack_addr_r = forced_i2c_write_byte(scl, sda, timer, (uint8_t)((addr << 1) | 0x01u));
    uint8_t read_val = forced_i2c_read_byte(scl, sda, timer, false);
    forced_i2c_stop(scl, sda, timer);

    forced_i2c_release_for_ack(scl);
    forced_i2c_release_for_ack(sda);
    port_disable(scl);
    port_disable(sda);
    hwtimer_free(timer);

    int ret = (ack_addr_w && ack_reg && ack_addr_r) ? 0 : -1;
    if (ret == 0) {
        *val = read_val;
    }
    return ret;
}

static int pcal_write_internal(uint8_t reg, uint8_t val, bool log)
{
    if (log) {
        rtos_printf("PI_AI_MIC PCAL6408A W start addr=0x%02x reg=0x%02x val=0x%02x\n",
                    PCAL6408A_I2C_ADDR, reg, val);
    }
    int res = pi_ai_mic_i2c_forced_reg_write(PCAL6408A_I2C_ADDR, reg, val);
    if (log) {
        rtos_printf("PI_AI_MIC PCAL6408A W done addr=0x%02x reg=0x%02x val=0x%02x ret=%d\n",
                    PCAL6408A_I2C_ADDR, reg, val, res);
    }
    return res;
}

static int pcal_write(uint8_t reg, uint8_t val)
{
    return pcal_write_internal(reg, val, true);
}

static int pcal_read_internal(uint8_t reg, uint8_t *val, bool log)
{
    if (log) {
        rtos_printf("PI_AI_MIC PCAL6408A R start addr=0x%02x reg=0x%02x\n",
                    PCAL6408A_I2C_ADDR, reg);
    }
    int res = pi_ai_mic_i2c_forced_reg_read(PCAL6408A_I2C_ADDR, reg, val);
    if (log) {
        rtos_printf("PI_AI_MIC PCAL6408A R done addr=0x%02x reg=0x%02x val=0x%02x ret=%d\n",
                    PCAL6408A_I2C_ADDR, reg, res == 0 ? *val : 0, res);
    }
    return res;
}

static int pcal_read(uint8_t reg, uint8_t *val)
{
    return pcal_read_internal(reg, val, true);
}

static int pcal_configure_detect_inputs(bool log)
{
    int ret = 0;

    ret |= pcal_write_internal(PCAL6408A_POLARITY_INV, 0x00, log);
    ret |= pcal_write_internal(PCAL6408A_INPUT_LATCH, 0x00, log);
    ret |= pcal_write_internal(PCAL6408A_PULLUPDOWN_SEL, 0x00, log);
    ret |= pcal_write_internal(PCAL6408A_PULLUPDOWN_EN, 0x00, log);
    ret |= pcal_write_internal(PCAL6408A_INTERRUPT_MASK, 0xFF, log);
    ret |= pcal_write_internal(PCAL6408A_OUTPUT_PORT_CONFIG, 0x00, log);
    ret |= pcal_write_internal(PCAL6408A_CONF, PCAL_INPUT_MASK, log);

    return ret;
}

int pi_ai_mic_read_detect_state(bool *valid, bool *jack_inserted, bool *mic_square)
{
    uint8_t input = 0;
    const char *method = "stop";

    /*
     * Runtime polling must be non-invasive: do not rewrite PCAL config every
     * second because P4/P7 also own the amplifier. The pre-RTOS init leaves P5
     * and P6 as inputs; only reconfigure if the live read actually fails.
     */
    int ret = pi_ai_mic_i2c_forced_reg_read_stop(PCAL6408A_I2C_ADDR,
                                                 PCAL6408A_INPUT_PORT,
                                                 &input);
    if (ret != 0) {
        method = "rs";
        ret = pi_ai_mic_i2c_forced_reg_read(PCAL6408A_I2C_ADDR,
                                            PCAL6408A_INPUT_PORT,
                                            &input);
    }
    if (ret != 0) {
        int cfg_ret = pcal_configure_detect_inputs(false);
        method = "cfg+stop";
        ret = (cfg_ret == 0) ?
            pi_ai_mic_i2c_forced_reg_read_stop(PCAL6408A_I2C_ADDR,
                                               PCAL6408A_INPUT_PORT,
                                               &input) :
            cfg_ret;
    }

    if (valid != NULL) {
        *valid = (ret == 0);
    }

    if (ret != 0) {
        return ret;
    }

    uint8_t jack_level = (uint8_t)((input >> PCAL_JACK_DET_PIN) & 0x01u);
    uint8_t mic_level = (uint8_t)((input >> PCAL_MIC_DETECT_PIN) & 0x01u);

    if (jack_inserted != NULL) {
        *jack_inserted = (jack_level == PI_AI_MIC_JACK_INSERTED_LEVEL);
    }
    if (mic_square != NULL) {
        *mic_square = (mic_level == PI_AI_MIC_MIC_DETECT_SQUARE_LEVEL);
    }

    rtos_printf("PI_AI_MIC detect input live method=%s raw=0x%02x jack=%u mic_square=%u\n",
                method,
                input,
                (jack_level == PI_AI_MIC_JACK_INSERTED_LEVEL) ? 1u : 0u,
                (mic_level == PI_AI_MIC_MIC_DETECT_SQUARE_LEVEL) ? 1u : 0u);

    return 0;
}

int pi_ai_mic_read_detect_input(bool *valid, bool *jack_inserted, bool *mic_square)
{
    uint8_t input = 0;
    int ret = pi_ai_mic_i2c_forced_reg_read(PCAL6408A_I2C_ADDR,
                                            PCAL6408A_INPUT_PORT,
                                            &input);

    if (valid != NULL) {
        *valid = (ret == 0);
    }

    if (ret != 0) {
        return ret;
    }

    uint8_t jack_level = (uint8_t)((input >> PCAL_JACK_DET_PIN) & 0x01u);
    uint8_t mic_level = (uint8_t)((input >> PCAL_MIC_DETECT_PIN) & 0x01u);

    if (jack_inserted != NULL) {
        *jack_inserted = (jack_level == PI_AI_MIC_JACK_INSERTED_LEVEL);
    }
    if (mic_square != NULL) {
        *mic_square = (mic_level == PI_AI_MIC_MIC_DETECT_SQUARE_LEVEL);
    }

    rtos_printf("PI_AI_MIC detect input poll raw=0x%02x jack=%u mic_square=%u\n",
                input,
                (jack_level == PI_AI_MIC_JACK_INSERTED_LEVEL) ? 1u : 0u,
                (mic_level == PI_AI_MIC_MIC_DETECT_SQUARE_LEVEL) ? 1u : 0u);

    return 0;
}

static int dac_read(uint8_t reg, uint8_t *val)
{
    rtos_printf("PI_AI_MIC DAC3101 R start addr=0x%02x reg=0x%02x\n",
                DAC3101_I2C_DEVICE_ADDR, reg);
    int res = pi_ai_mic_i2c_forced_reg_read(DAC3101_I2C_DEVICE_ADDR, reg, val);
    rtos_printf("PI_AI_MIC DAC3101 R done addr=0x%02x reg=0x%02x val=0x%02x ret=%d\n",
                DAC3101_I2C_DEVICE_ADDR, reg, res == 0 ? *val : 0, res);
    return res;
}

static int pcal_write_outputs(void)
{
    return pcal_write(PCAL6408A_OUTPUT_PORT, pcal_output_shadow);
}

int pi_ai_mic_expander_init(void)
{
    int ret = 0;

    /*
     * Write output defaults before switching pins to outputs:
     * P0 XVF_RST_N high; P2 DAC_RST_N high; P3 BOOT_SEL low;
     * P4/P7 AMP_CTRL low for shutdown. P1 INT_N, P5 JACK_DET and
     * P6 MIC_DETECT/mic-geometry strap stay inputs. Pulls are disabled to
     * match the standalone PCAL LED detector validated on this prototype.
     */
    pcal_output_shadow = BIT(PCAL_XVF_RST_N_PIN) |
                         BIT(PCAL_DAC_RST_N_PIN);
    pcal_ready = false;

    ret |= pcal_write_outputs();
    ret |= pcal_configure_detect_inputs(true);

    pcal_ready = (ret == 0);
    rtos_printf("PI_AI_MIC PCAL6408A init %s\n", pcal_ready ? "ok" : "failed");

    return ret;
}

int pi_ai_mic_set_dac_reset_n(bool level)
{
    if (!level) {
        rtos_printf("PI_AI_MIC DAC_RST_N low via PCAL ignored; P2 is held high to avoid reset contention\n");
        return 0;
    }

    pcal_output_shadow |= BIT(PCAL_DAC_RST_N_PIN);
    rtos_printf("PI_AI_MIC DAC_RST_N PCAL P%u held high\n", PCAL_DAC_RST_N_PIN);
    return pcal_write_outputs();
}

int pi_ai_mic_set_int_n(bool level)
{
    (void) level;
    rtos_printf("PI_AI_MIC INT_N PCAL request ignored; owner is XMOS direct GPO pin %u\n",
                GPO_INT_N_PIN);
    return 0;
}

int pi_ai_mic_set_amp_mode(pi_ai_mic_amp_mode_t mode)
{
    bool ctrl1 = false;
    bool ctrl2 = false;
    int ret = 0;

#if PI_AI_MIC_HARD_SPEAKER_MUTE
    if (mode != PI_AI_MIC_AMP_SHUTDOWN) {
        rtos_printf("PI_AI_MIC hard speaker mute: forcing amp shutdown instead of mode %u\n", mode);
    }
    mode = PI_AI_MIC_AMP_SHUTDOWN;
#endif

    switch (mode) {
    case PI_AI_MIC_AMP_CLASS_AB_BOOST_OFF:
        ctrl1 = true;
        ctrl2 = false;
        break;
    case PI_AI_MIC_AMP_CLASS_D_BOOST_ACF:
        ctrl1 = false;
        ctrl2 = true;
        break;
    case PI_AI_MIC_AMP_CLASS_D_FULL:
        ctrl1 = true;
        ctrl2 = true;
        break;
    case PI_AI_MIC_AMP_SHUTDOWN:
    default:
        ctrl1 = false;
        ctrl2 = false;
        break;
    }

    if (ctrl1) {
        pcal_output_shadow |= BIT(PCAL_AMP_CTRL1_PIN);
    } else {
        pcal_output_shadow &= (uint8_t)~BIT(PCAL_AMP_CTRL1_PIN);
    }

    if (ctrl2) {
        pcal_output_shadow |= BIT(PCAL_AMP_CTRL2_PIN);
    } else {
        pcal_output_shadow &= (uint8_t)~BIT(PCAL_AMP_CTRL2_PIN);
    }

    ret |= pcal_write(PCAL6408A_OUTPUT_PORT_CONFIG, 0x00);
    ret |= pcal_write(PCAL6408A_CONF, PCAL_INPUT_MASK);
    ret |= pcal_write_outputs();

    uint8_t output = 0;
    uint8_t conf = 0;
    int output_ret = pcal_read_internal(PCAL6408A_OUTPUT_PORT, &output, false);
    int conf_ret = pcal_read_internal(PCAL6408A_CONF, &conf, false);

    rtos_printf("PI_AI_MIC amp mode %u: AMP_CTRL1=P4=%u AMP_CTRL2=P7=%u output=0x%02x conf=0x%02x ret=%d/%d/%d\n",
                mode,
                ctrl1,
                ctrl2,
                output_ret == 0 ? output : 0,
                conf_ret == 0 ? conf : 0,
                ret,
                output_ret,
                conf_ret);
    return ret | output_ret | conf_ret;
}

bool pi_ai_mic_jack_inserted(bool *valid)
{
    uint8_t inputs = 0;
    bool ok = pcal_ready && (pcal_read_internal(PCAL6408A_INPUT_PORT, &inputs, false) == 0);
    static bool last_log_valid = false;
    static bool last_log_inserted = false;
    static bool logged_invalid = false;

    if (valid != NULL) {
        *valid = ok;
    }

    if (!ok) {
        if (!logged_invalid) {
            rtos_printf("PI_AI_MIC JACK_DET read failed; defaulting to headphone safe mode\n");
            logged_invalid = true;
        }
        last_log_valid = false;
        return true;
    }

    uint8_t level = (inputs >> PCAL_JACK_DET_PIN) & 0x01;
    bool inserted = (level == PI_AI_MIC_JACK_INSERTED_LEVEL);
    if (!last_log_valid || inserted != last_log_inserted) {
        rtos_printf("PI_AI_MIC JACK_DET P5=%u -> %s\n", level, inserted ? "jack" : "speaker");
    }

    logged_invalid = false;
    last_log_valid = true;
    last_log_inserted = inserted;

    return inserted;
}

bool pi_ai_mic_mic_geometry_square(bool *valid)
{
    uint8_t inputs = 0;
    bool ok = pcal_ready && (pcal_read_internal(PCAL6408A_INPUT_PORT, &inputs, false) == 0);

    if (valid != NULL) {
        *valid = ok;
    }

    if (!ok) {
        rtos_printf("PI_AI_MIC MIC_DETECT P6 read failed; defaulting to line geometry\n");
        return false;
    }

    uint8_t level = (inputs >> PCAL_MIC_DETECT_PIN) & 0x01;
    bool square = (level == PI_AI_MIC_MIC_DETECT_SQUARE_LEVEL);
    rtos_printf("PI_AI_MIC MIC_DETECT P6=%u -> %s geometry, 4 mics\n",
                level, square ? "square" : "line");

    return square;
}

#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
static bool pi_ai_mic_sample_mic_geometry(bool *valid)
{
    unsigned square_count = 0;
    unsigned line_count = 0;
    hwtimer_t timer = hwtimer_alloc();

    for (unsigned sample = 0; sample < PI_AI_MIC_MIC_GEOMETRY_SAMPLE_COUNT; sample++) {
        uint8_t inputs = 0;
        if (pcal_ready && (pcal_read_internal(PCAL6408A_INPUT_PORT, &inputs, false) == 0)) {
            uint8_t level = (inputs >> PCAL_MIC_DETECT_PIN) & 0x01;
            if (level == PI_AI_MIC_MIC_DETECT_SQUARE_LEVEL) {
                square_count++;
            } else {
                line_count++;
            }
        }

        if ((sample + 1u) < PI_AI_MIC_MIC_GEOMETRY_SAMPLE_COUNT) {
            hwtimer_delay(timer, PI_AI_MIC_MIC_GEOMETRY_SAMPLE_INTERVAL_TICKS);
        }
    }

    hwtimer_free(timer);

    bool stable_square = square_count >= PI_AI_MIC_MIC_GEOMETRY_SAMPLE_MIN_MATCH;
    bool stable_line = line_count >= PI_AI_MIC_MIC_GEOMETRY_SAMPLE_MIN_MATCH;
    bool stable = stable_square != stable_line;

    if (valid != NULL) {
        *valid = stable;
    }

    rtos_printf("PI_AI_MIC MIC_DETECT boot samples: square=%u line=%u -> %s%s\n",
                square_count,
                line_count,
                stable_square ? "square" : (stable_line ? "line" : "unstable"),
                stable ? "" : " (fallback)");

    return stable_square;
}
#endif

int pi_ai_mic_update_mic_geometry_from_strap(void)
{
#if PI_AI_MIC_FORCE_MIC_GEOMETRY_SQUARE
    mic_geometry_square = true;
    mic_geometry_valid = true;
    mic_geometry_ready = true;
    rtos_printf("PI_AI_MIC mic geometry selected: square (build forced)\n");
    return 0;
#elif PI_AI_MIC_FORCE_MIC_GEOMETRY_LINE
    mic_geometry_square = false;
    mic_geometry_valid = true;
    mic_geometry_ready = true;
    rtos_printf("PI_AI_MIC mic geometry selected: line (build forced)\n");
    return 0;
#elif PI_AI_MIC_MIC_GEOMETRY_STRAP_ENABLE
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    /*
     * AUTO profiles receive the tile-0 pre-RTOS P6 result before the AEC
     * PAR job starts. Keep that boot decision frozen for the whole run.
     */
    if (mic_geometry_ready) {
        rtos_printf("PI_AI_MIC mic geometry retained: %s%s (boot intertile)\n",
                    mic_geometry_square ? "square" : "line",
                    mic_geometry_valid ? "" : " (fallback)");
        return mic_geometry_valid ? 0 : -1;
    }
#endif
    bool valid = false;
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    bool square = pi_ai_mic_sample_mic_geometry(&valid);
#else
    bool square = pi_ai_mic_mic_geometry_square(&valid);
#endif

    mic_geometry_square = valid ? square : (PI_AI_MIC_MIC_GEOMETRY_FALLBACK_SQUARE != 0);
    mic_geometry_valid = valid;
    mic_geometry_ready = true;

    rtos_printf("PI_AI_MIC mic geometry selected: %s%s\n",
                mic_geometry_square ? "square" : "line",
                mic_geometry_valid ? "" : " (fallback)");
    return valid ? 0 : -1;
#else
    mic_geometry_square = false;
    mic_geometry_valid = true;
    mic_geometry_ready = true;
    rtos_printf("PI_AI_MIC mic geometry selected: line (strap disabled)\n");
    return 0;
#endif
}

void pi_ai_mic_set_mic_geometry_cached(bool valid, bool square)
{
    mic_geometry_valid = valid;
    mic_geometry_square = valid ? square : (PI_AI_MIC_MIC_GEOMETRY_FALLBACK_SQUARE != 0);
    mic_geometry_ready = true;
}

void pi_ai_mic_wait_for_mic_geometry_ready(void)
{
#if PI_AI_MIC_MIC_GEOMETRY_STRAP_ENABLE
    if (mic_geometry_ready) {
        return;
    }

    hwtimer_t timer = hwtimer_alloc();
    for (unsigned i = 0; !mic_geometry_ready && i < PI_AI_MIC_MIC_GEOMETRY_WAIT_LOOPS; i++) {
        hwtimer_delay(timer, PI_AI_MIC_MIC_GEOMETRY_WAIT_TICKS);
    }
    hwtimer_free(timer);

    if (!mic_geometry_ready) {
        mic_geometry_square = false;
        mic_geometry_valid = false;
        mic_geometry_ready = true;
    }
#endif
}

bool pi_ai_mic_mic_geometry_cached_square(void)
{
    if (!mic_geometry_ready) {
        return false;
    }
    return mic_geometry_square;
}

int pi_ai_mic_get_mic_array_type(void)
{
    return pi_ai_mic_mic_geometry_cached_square() ? BECLEAR_CIRCULAR_ARRAY : BECLEAR_LINEAR_ARRAY;
}

bool pi_ai_mic_doa_raw_ring_enabled(void)
{
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    return pi_ai_mic_mic_geometry_cached_square();
#else
    return PI_AI_MIC_DOA_RAW_RING_ENABLE != 0;
#endif
}

bool pi_ai_mic_raw_doa_pcb_order(void)
{
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    return pi_ai_mic_mic_geometry_cached_square();
#else
    return PI_AI_MIC_RAW_DOA_PCB_ORDER != 0;
#endif
}

int pi_ai_mic_raw_doa_delay_sign(void)
{
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    return pi_ai_mic_mic_geometry_cached_square() ? 1 : 0;
#else
    return PI_AI_MIC_RAW_DOA_DELAY_SIGN;
#endif
}

bool pi_ai_mic_raw_doa_abs_correlation(void)
{
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    return !pi_ai_mic_mic_geometry_cached_square();
#else
    return PI_AI_MIC_RAW_DOA_ABS_CORRELATION != 0;
#endif
}

bool pi_ai_mic_doa_ring_offset_180(void)
{
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    return !pi_ai_mic_mic_geometry_cached_square();
#else
    return PI_AI_MIC_DOA_RING_OFFSET_180 != 0;
#endif
}

bool pi_ai_mic_doa_neopixel_mirror(void)
{
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    return !pi_ai_mic_mic_geometry_cached_square();
#else
    return PI_AI_MIC_DOA_NEOPIXEL_MIRROR_90_270 != 0;
#endif
}

bool pi_ai_mic_doa_neopixel_reverse(void)
{
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    return true;
#else
    return PI_AI_MIC_DOA_NEOPIXEL_REVERSE_ORDER != 0;
#endif
}

bool pi_ai_mic_doa_opposite_filter_enabled(void)
{
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    return pi_ai_mic_mic_geometry_cached_square();
#else
    return PI_AI_MIC_DOA_RING_OPPOSITE_FILTER_ENABLE != 0;
#endif
}

int pi_ai_mic_doa_opposite_stable_frames(void)
{
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    return pi_ai_mic_mic_geometry_cached_square() ? 30 : 15;
#else
    return PI_AI_MIC_DOA_RING_OPPOSITE_STABLE_FRAMES;
#endif
}

int pi_ai_mic_probe_i2c_devices(void)
{
    int ret = 0;
    uint8_t val = 0;

    rtos_printf("PI_AI_MIC I2C probe begin\n");

    rtos_printf("PI_AI_MIC I2C probe PCAL6408A 0x20 start\n");
    int pcal_ret = pcal_read(PCAL6408A_INPUT_PORT, &val);
    rtos_printf("PI_AI_MIC I2C probe PCAL6408A 0x20 done ret=%d val=0x%02x\n",
                pcal_ret, pcal_ret == 0 ? val : 0);
    ret |= pcal_ret;

    rtos_printf("PI_AI_MIC I2C probe DAC3101 0x18 start\n");
    int dac_ret = dac_read(DAC3101_PAGE_CTRL, &val);
    rtos_printf("PI_AI_MIC I2C probe DAC3101 0x18 done ret=%d val=0x%02x\n",
                dac_ret, dac_ret == 0 ? val : 0);
    ret |= dac_ret;

    rtos_printf("PI_AI_MIC I2C probe %s\n", ret == 0 ? "ok" : "failed");
    return ret;
}

int pi_ai_mic_configure_codec_output(bool jack_inserted)
{
    int ret = 0;

    if (jack_inserted) {
        rtos_printf("PI_AI_MIC DAC route: jack normal mode\n");
        ret |= dac3101_reg_write(DAC3101_PAGE_CTRL, 0x01);
        ret |= dac3101_reg_write(DAC3101_DAC_OP_MIX, 0x44);
        ret |= dac3101_reg_write(DAC3101_HPL_DRVR, PI_AI_MIC_HEADPHONE_DRIVER_REG);
        ret |= dac3101_reg_write(DAC3101_HPR_DRVR, PI_AI_MIC_HEADPHONE_DRIVER_REG);
        ret |= dac3101_reg_write(DAC3101_HP_LINEOUT, PI_AI_MIC_HEADPHONE_LINEOUT_CTRL);
        ret |= dac3101_reg_write(DAC3101_HPL_VOL_A, PI_AI_MIC_HEADPHONE_ANALOG_VOL);
        ret |= dac3101_reg_write(DAC3101_HPR_VOL_A, PI_AI_MIC_HEADPHONE_ANALOG_VOL);
        ret |= dac3101_reg_write(DAC3101_SPKL_VOL_A, PI_AI_MIC_HEADPHONE_MODE_SPEAKER_ANALOG_VOL);
        ret |= dac3101_reg_write(DAC3101_SPKR_VOL_A, PI_AI_MIC_HEADPHONE_MODE_SPEAKER_ANALOG_VOL);
        ret |= dac3101_reg_write(DAC3101_PAGE_CTRL, 0x00);
        ret |= dac3101_reg_write(DAC3101_DAC_DAT_PATH, PI_AI_MIC_DAC_DAT_PATH_STEREO);
    } else {
#if PI_AI_MIC_SPEAKER_USE_XMOS_STEREO_ROUTE
        rtos_printf("PI_AI_MIC DAC route: speaker XMOS stereo mode\n");
        ret |= dac3101_reg_write(DAC3101_PAGE_CTRL, 0x01);
        ret |= dac3101_reg_write(DAC3101_DAC_OP_MIX, 0x44);
        ret |= dac3101_reg_write(DAC3101_HPL_DRVR, 0x06);
        ret |= dac3101_reg_write(DAC3101_HPR_DRVR, 0x06);
        ret |= dac3101_reg_write(DAC3101_HP_LINEOUT, 0x00);
        ret |= dac3101_reg_write(DAC3101_HPL_VOL_A, PI_AI_MIC_HEADPHONE_ANALOG_VOL);
        ret |= dac3101_reg_write(DAC3101_HPR_VOL_A, PI_AI_MIC_HEADPHONE_ANALOG_VOL);
        ret |= dac3101_reg_write(DAC3101_SPKL_VOL_A, PI_AI_MIC_SPEAKER_ANALOG_VOL);
        ret |= dac3101_reg_write(DAC3101_SPKR_VOL_A, PI_AI_MIC_SPEAKER_ANALOG_VOL);
        ret |= dac3101_reg_write(DAC3101_PAGE_CTRL, 0x00);
        ret |= dac3101_reg_write(DAC3101_DAC_DAT_PATH, PI_AI_MIC_DAC_DAT_PATH_STEREO);
#else
        rtos_printf("PI_AI_MIC DAC route: speaker differential mode\n");
        ret |= dac3101_reg_write(DAC3101_PAGE_CTRL, 0x01);
        ret |= dac3101_reg_write(DAC3101_DAC_OP_MIX, 0x41);
        ret |= dac3101_reg_write(DAC3101_HPL_DRVR, 0x06);
        ret |= dac3101_reg_write(DAC3101_HPR_DRVR, 0x06);
        ret |= dac3101_reg_write(DAC3101_HP_LINEOUT, 0x00);
        ret |= dac3101_reg_write(DAC3101_HPL_VOL_A, PI_AI_MIC_HEADPHONE_ANALOG_VOL);
        ret |= dac3101_reg_write(DAC3101_HPR_VOL_A, PI_AI_MIC_HEADPHONE_ANALOG_VOL);
        ret |= dac3101_reg_write(DAC3101_SPKL_VOL_A, PI_AI_MIC_SPEAKER_ANALOG_VOL);
        ret |= dac3101_reg_write(DAC3101_SPKR_VOL_A, PI_AI_MIC_SPEAKER_ANALOG_VOL);
        ret |= dac3101_reg_write(DAC3101_PAGE_CTRL, 0x00);
        ret |= dac3101_reg_write(DAC3101_DAC_DAT_PATH, PI_AI_MIC_DAC_DAT_PATH_SPEAKER_DIFF);
#endif
    }

    ret |= dac3101_reg_write(DAC3101_DACL_VOL_D, PI_AI_MIC_DAC_DIGITAL_VOL);
    ret |= dac3101_reg_write(DAC3101_DACR_VOL_D, PI_AI_MIC_DAC_DIGITAL_VOL);
    ret |= dac3101_reg_write(DAC3101_DAC_VOL, 0x00);
    return ret;
}

int pi_ai_mic_apply_output_route(bool jack_inserted, bool speaker_enabled_when_no_jack)
{
    int ret = 0;

#if PI_AI_MIC_HARD_SPEAKER_MUTE
    pi_ai_mic_output_route_speaker_active = 0;
    ret |= pi_ai_mic_configure_codec_output(true);
    ret |= pi_ai_mic_set_amp_mode(PI_AI_MIC_AMP_SHUTDOWN);
    rtos_printf("PI_AI_MIC audio route applied: hard headphone-only, amp shutdown, jack=%u speaker_enable=%u ret=%d\n",
                jack_inserted, speaker_enabled_when_no_jack, ret);
    return ret;
#endif

    ret |= pi_ai_mic_set_amp_mode(PI_AI_MIC_AMP_SHUTDOWN);

    if (jack_inserted || !speaker_enabled_when_no_jack) {
        pi_ai_mic_output_route_speaker_active = 0;
        ret |= pi_ai_mic_configure_codec_output(true);
        ret |= pi_ai_mic_set_amp_mode(PI_AI_MIC_AMP_SHUTDOWN);
        rtos_printf("PI_AI_MIC audio route applied: headphone stereo, amp shutdown, jack=%u speaker_enable=%u ret=%d\n",
                    jack_inserted, speaker_enabled_when_no_jack, ret);
    } else {
        pi_ai_mic_output_route_speaker_active = 1;
        ret |= pi_ai_mic_configure_codec_output(false);
        ret |= pi_ai_mic_set_amp_mode(PI_AI_MIC_AMP_CLASS_D_BOOST_ACF);
        rtos_printf("PI_AI_MIC audio route applied: speaker differential, amp class-D boost ACF-on, ret=%d\n",
                    ret);
    }

    return ret;
}

int pi_ai_mic_debug_dump_i2c_state(void)
{
    int ret = 0;
    uint8_t val = 0;

    rtos_printf("PI_AI_MIC I2C debug dump begin\n");

    ret |= pcal_read(PCAL6408A_INPUT_PORT, &val);
    ret |= pcal_read(PCAL6408A_OUTPUT_PORT, &val);
    ret |= pcal_read(PCAL6408A_CONF, &val);

    ret |= dac3101_reg_write(DAC3101_PAGE_CTRL, 0x00);
    ret |= dac_read(DAC3101_PAGE_CTRL, &val);
    ret |= dac_read(DAC3101_DAC_DAT_PATH, &val);
    ret |= dac_read(DAC3101_DAC_VOL, &val);
    ret |= dac_read(DAC3101_CODEC_IF, &val);

    ret |= dac3101_reg_write(DAC3101_PAGE_CTRL, 0x01);
    ret |= dac_read(DAC3101_PAGE_CTRL, &val);
    ret |= dac_read(DAC3101_DAC_OP_MIX, &val);
    ret |= dac_read(DAC3101_HP_DRVR, &val);
    ret |= dac_read(DAC3101_HPL_VOL_A, &val);
    ret |= dac_read(DAC3101_HPR_VOL_A, &val);
    ret |= dac_read(DAC3101_HPL_DRVR, &val);
    ret |= dac_read(DAC3101_HPR_DRVR, &val);
    ret |= dac_read(DAC3101_HP_LINEOUT, &val);

    ret |= dac3101_reg_write(DAC3101_PAGE_CTRL, 0x00);

    rtos_printf("PI_AI_MIC I2C debug dump %s\n", ret == 0 ? "ok" : "failed");

    return ret;
}
