// Copyright 2022-2024 XMOS LIMITED.
// This Software is subject to the terms of the XCORE VocalFusion Licence.
/// tile 0 main. Starts all top level baremetal and rtos threads.

#define DEBUG_UNIT MAIN_TILE0
#ifndef DEBUG_PRINT_ENABLE_MAIN_TILE0
    #define DEBUG_PRINT_ENABLE_MAIN_TILE0 0
#endif
#include "debug_print.h"

#include "FreeRTOS.h"
#include "rtos_macros.h"
#include "xcore/chanend.h"
#include "xcore/hwtimer.h"
#include "xcore/parallel.h"
#include "xcore/port.h"
#include "task.h"
#ifdef __XS3A__
#include <xscope.h>
#endif


#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// bsp
#include "platform/platform_init.h"
#include "platform/driver_instances.h"
#include "app_conf.h"
#include "dac3101.h"
#include "user_config/pi_ai_mic_hw.h"
#if (PI_AI_MIC_I2C_ELECTRICAL_TEST_ENABLE && PI_AI_MIC_I2C_ELECTRICAL_TILE0_ENABLE) || \
        (PI_AI_MIC_I2C_FORCED_PROBE_TEST_ENABLE && PI_AI_MIC_I2C_FORCED_PROBE_TILE0_ENABLE) || \
        (PI_AI_MIC_I2C_PORT_SCAN_TEST_ENABLE && PI_AI_MIC_I2C_PORT_SCAN_TILE0_ENABLE) || \
        (PI_AI_MIC_I2C_PIN_LOCATOR_TEST_ENABLE && PI_AI_MIC_I2C_PIN_LOCATOR_TILE0_ENABLE)
#include "pi_ai_mic_i2c_scan.h"
#endif
#include "platform.h"
#include "tile_common.h"

#include "data_plane.h"
#include "servicer.h"
#include "xud_device.h"
#include "usb_buffer.h"
#include "pll_servicer.h"

#include "usb_proxy.h"

TaskHandle_t io_config_servicer_parent_task = NULL;

volatile int pi_ai_mic_boot_detect_valid = 0;
volatile int pi_ai_mic_boot_detect_jack = 1;
volatile int pi_ai_mic_boot_detect_mic_square = 0;

#if PI_AI_MIC_PRE_RTOS_SPEAKER_HW_INIT
#ifndef PI_AI_MIC_PRE_RTOS_LOG_ENABLE
#define PI_AI_MIC_PRE_RTOS_LOG_ENABLE       0
#endif

#if PI_AI_MIC_PRE_RTOS_LOG_ENABLE
#define PI_AI_MIC_PRE_LOG(...)              printf(__VA_ARGS__)
#else
#define PI_AI_MIC_PRE_LOG(...)              do { } while (0)
#endif

#define PI_AI_MIC_PRE_I2C_HALF_PERIOD_TICKS 500u
#define PI_AI_MIC_PRE_I2C_ACK_RELEASE_TICKS 20u
#define PI_AI_MIC_PRE_RESET_TICKS           100000u
#define PI_AI_MIC_PRE_STARTUP_DELAY_TICKS   100000000u
#define PI_AI_MIC_PRE_RETRY_DELAY_TICKS     10000000u
#define PI_AI_MIC_PRE_INIT_ATTEMPTS         5u
#define PI_AI_MIC_LATE_INIT_DELAY_MS        3000u
#define PI_AI_MIC_GPO_ALL_SAFE_HIGH         0xFFu

volatile int pi_ai_mic_pre_init_ret = -999;
volatile int pi_ai_mic_pre_pcal_ret = -999;
volatile int pi_ai_mic_pre_pcal_reset_ret = -999;
volatile int pi_ai_mic_pre_dac_ret = -999;
volatile int pi_ai_mic_pre_amp_ret = -999;
volatile unsigned pi_ai_mic_pre_init_attempt = 0;
volatile unsigned pi_ai_mic_pre_init_stage = 0;
volatile int pi_ai_mic_late_init_ret = -999;
volatile int pi_ai_mic_late_pcal_ret = -999;
volatile int pi_ai_mic_late_amp_shutdown_ret = -999;
volatile int pi_ai_mic_late_dac_ret = -999;
volatile int pi_ai_mic_late_amp_ret = -999;
volatile unsigned pi_ai_mic_late_init_count = 0;
volatile unsigned pi_ai_mic_late_init_stage = 0;

#define PCAL6408A_I2C_ADDR                  0x20
#define PCAL6408A_INPUT_PORT                0x00
#define PCAL6408A_OUTPUT_PORT               0x01
#define PCAL6408A_POLARITY_INV              0x02
#define PCAL6408A_CONF                      0x03
#define PCAL6408A_INPUT_LATCH               0x42
#define PCAL6408A_PULLUPDOWN_EN             0x43
#define PCAL6408A_PULLUPDOWN_SEL            0x44
#define PCAL6408A_INTERRUPT_MASK            0x45
#define PCAL6408A_INTERRUPT_STATUS          0x46
#define PCAL6408A_OUTPUT_PORT_CONFIG        0x4F
#define PI_AI_MIC_BIT(n)                    (1u << (n))

static void pi_ai_mic_pre_delay(hwtimer_t timer)
{
    hwtimer_delay(timer, PI_AI_MIC_PRE_I2C_HALF_PERIOD_TICKS);
}

static void pi_ai_mic_pre_drive(port_t port, uint32_t value)
{
    port_write_control_word(port, XS1_SETC_DRIVE_DRIVE);
    port_out(port, value ? 1u : 0u);
    port_sync(port);
}

static void pi_ai_mic_pre_release_for_ack(port_t port)
{
    port_write_control_word(port, XS1_SETC_DRIVE_PULL_UP);
    port_out(port, 1u);
    port_sync(port);
}

static uint32_t pi_ai_mic_pre_read(port_t port)
{
    port_sync(port);
    return port_peek(port) & 1u;
}

static void pi_ai_mic_pre_i2c_stop(port_t scl, port_t sda, hwtimer_t timer)
{
    pi_ai_mic_pre_drive(sda, 0u);
    pi_ai_mic_pre_delay(timer);
    pi_ai_mic_pre_drive(scl, 1u);
    pi_ai_mic_pre_delay(timer);
    pi_ai_mic_pre_drive(sda, 1u);
    pi_ai_mic_pre_delay(timer);
}

static void pi_ai_mic_pre_i2c_recover(port_t scl, port_t sda, hwtimer_t timer)
{
    pi_ai_mic_pre_drive(sda, 1u);
    for (unsigned i = 0; i < 9; i++) {
        pi_ai_mic_pre_drive(scl, 0u);
        pi_ai_mic_pre_delay(timer);
        pi_ai_mic_pre_drive(scl, 1u);
        pi_ai_mic_pre_delay(timer);
    }
    pi_ai_mic_pre_i2c_stop(scl, sda, timer);
}

static void pi_ai_mic_pre_i2c_start(port_t scl, port_t sda, hwtimer_t timer)
{
    pi_ai_mic_pre_drive(sda, 1u);
    pi_ai_mic_pre_drive(scl, 1u);
    pi_ai_mic_pre_delay(timer);
    pi_ai_mic_pre_drive(sda, 0u);
    pi_ai_mic_pre_delay(timer);
    pi_ai_mic_pre_drive(scl, 0u);
    pi_ai_mic_pre_delay(timer);
}

static int pi_ai_mic_pre_i2c_write_byte(port_t scl, port_t sda, hwtimer_t timer, uint8_t byte)
{
    for (int bit = 7; bit >= 0; bit--) {
        pi_ai_mic_pre_drive(sda, (byte >> bit) & 1u);
        pi_ai_mic_pre_delay(timer);
        pi_ai_mic_pre_drive(scl, 1u);
        pi_ai_mic_pre_delay(timer);
        pi_ai_mic_pre_drive(scl, 0u);
        pi_ai_mic_pre_delay(timer);
    }

    pi_ai_mic_pre_drive(sda, 1u);
    pi_ai_mic_pre_release_for_ack(sda);
    hwtimer_delay(timer, PI_AI_MIC_PRE_I2C_ACK_RELEASE_TICKS);
    pi_ai_mic_pre_drive(scl, 1u);
    pi_ai_mic_pre_delay(timer);
    int ack = pi_ai_mic_pre_read(sda) == 0u;
    pi_ai_mic_pre_drive(scl, 0u);
    pi_ai_mic_pre_delay(timer);
    pi_ai_mic_pre_drive(sda, 1u);

    return ack;
}

static int pi_ai_mic_pre_i2c_reg_write(uint8_t addr, uint8_t reg, uint8_t val)
{
    port_t scl = PORT_I2C_SCL;
    port_t sda = PORT_I2C_SDA;
    hwtimer_t timer = hwtimer_alloc();

    port_enable(scl);
    port_enable(sda);
    pi_ai_mic_pre_drive(scl, 1u);
    pi_ai_mic_pre_drive(sda, 1u);
    pi_ai_mic_pre_i2c_recover(scl, sda, timer);

    pi_ai_mic_pre_i2c_start(scl, sda, timer);
    int ack_addr = pi_ai_mic_pre_i2c_write_byte(scl, sda, timer, (uint8_t)(addr << 1));
    int ack_reg = pi_ai_mic_pre_i2c_write_byte(scl, sda, timer, reg);
    int ack_val = pi_ai_mic_pre_i2c_write_byte(scl, sda, timer, val);
    pi_ai_mic_pre_i2c_stop(scl, sda, timer);

    pi_ai_mic_pre_release_for_ack(scl);
    pi_ai_mic_pre_release_for_ack(sda);
    port_disable(scl);
    port_disable(sda);
    hwtimer_free(timer);

    int ret = (ack_addr && ack_reg && ack_val) ? 0 : -1;
    PI_AI_MIC_PRE_LOG("PI_AI_MIC PRE I2C W addr=0x%02x reg=0x%02x val=0x%02x ret=%d ack=%u/%u/%u\n",
                      addr, reg, val, ret, ack_addr, ack_reg, ack_val);
    return ret;
}

static uint8_t pi_ai_mic_pre_i2c_read_byte(port_t scl, port_t sda, hwtimer_t timer, int ack)
{
    uint8_t byte = 0;

    for (int bit = 7; bit >= 0; bit--) {
        pi_ai_mic_pre_drive(sda, 1u);
        pi_ai_mic_pre_release_for_ack(sda);
        hwtimer_delay(timer, PI_AI_MIC_PRE_I2C_ACK_RELEASE_TICKS);
        pi_ai_mic_pre_delay(timer);
        pi_ai_mic_pre_drive(scl, 1u);
        pi_ai_mic_pre_delay(timer);
        byte |= (uint8_t)(pi_ai_mic_pre_read(sda) << bit);
        pi_ai_mic_pre_drive(scl, 0u);
        pi_ai_mic_pre_delay(timer);
    }

    pi_ai_mic_pre_drive(sda, ack ? 0u : 1u);
    pi_ai_mic_pre_delay(timer);
    pi_ai_mic_pre_drive(scl, 1u);
    pi_ai_mic_pre_delay(timer);
    pi_ai_mic_pre_drive(scl, 0u);
    pi_ai_mic_pre_delay(timer);
    pi_ai_mic_pre_drive(sda, 1u);

    return byte;
}

static int pi_ai_mic_pre_i2c_reg_read(uint8_t addr, uint8_t reg, uint8_t *val)
{
    if (val == NULL) {
        return -1;
    }

    port_t scl = PORT_I2C_SCL;
    port_t sda = PORT_I2C_SDA;
    hwtimer_t timer = hwtimer_alloc();

    port_enable(scl);
    port_enable(sda);
    pi_ai_mic_pre_drive(scl, 1u);
    pi_ai_mic_pre_drive(sda, 1u);
    pi_ai_mic_pre_i2c_recover(scl, sda, timer);

    pi_ai_mic_pre_i2c_start(scl, sda, timer);
    int ack_addr_w = pi_ai_mic_pre_i2c_write_byte(scl, sda, timer, (uint8_t)(addr << 1));
    int ack_reg = pi_ai_mic_pre_i2c_write_byte(scl, sda, timer, reg);
    pi_ai_mic_pre_i2c_start(scl, sda, timer);
    int ack_addr_r = pi_ai_mic_pre_i2c_write_byte(scl, sda, timer, (uint8_t)((addr << 1) | 0x01u));
    uint8_t read_val = pi_ai_mic_pre_i2c_read_byte(scl, sda, timer, 0);
    pi_ai_mic_pre_i2c_stop(scl, sda, timer);

    pi_ai_mic_pre_release_for_ack(scl);
    pi_ai_mic_pre_release_for_ack(sda);
    port_disable(scl);
    port_disable(sda);
    hwtimer_free(timer);

    int ret = (ack_addr_w && ack_reg && ack_addr_r) ? 0 : -1;
    if (ret == 0) {
        *val = read_val;
    }
    PI_AI_MIC_PRE_LOG("PI_AI_MIC PRE I2C R addr=0x%02x reg=0x%02x val=0x%02x ret=%d ack=%u/%u/%u\n",
                      addr, reg, read_val, ret, ack_addr_w, ack_reg, ack_addr_r);
    return ret;
}

static int pi_ai_mic_pre_i2c_reg_read_stop(uint8_t addr, uint8_t reg, uint8_t *val)
{
    if (val == NULL) {
        return -1;
    }

    port_t scl = PORT_I2C_SCL;
    port_t sda = PORT_I2C_SDA;
    hwtimer_t timer = hwtimer_alloc();

    port_enable(scl);
    port_enable(sda);
    pi_ai_mic_pre_drive(scl, 1u);
    pi_ai_mic_pre_drive(sda, 1u);
    pi_ai_mic_pre_i2c_recover(scl, sda, timer);

    pi_ai_mic_pre_i2c_start(scl, sda, timer);
    int ack_addr_w = pi_ai_mic_pre_i2c_write_byte(scl, sda, timer, (uint8_t)(addr << 1));
    int ack_reg = pi_ai_mic_pre_i2c_write_byte(scl, sda, timer, reg);
    pi_ai_mic_pre_i2c_stop(scl, sda, timer);

    pi_ai_mic_pre_i2c_start(scl, sda, timer);
    int ack_addr_r = pi_ai_mic_pre_i2c_write_byte(scl, sda, timer, (uint8_t)((addr << 1) | 0x01u));
    uint8_t read_val = pi_ai_mic_pre_i2c_read_byte(scl, sda, timer, 0);
    pi_ai_mic_pre_i2c_stop(scl, sda, timer);

    pi_ai_mic_pre_release_for_ack(scl);
    pi_ai_mic_pre_release_for_ack(sda);
    port_disable(scl);
    port_disable(sda);
    hwtimer_free(timer);

    int ret = (ack_addr_w && ack_reg && ack_addr_r) ? 0 : -1;
    if (ret == 0) {
        *val = read_val;
    }
    PI_AI_MIC_PRE_LOG("PI_AI_MIC PRE I2C R_STOP addr=0x%02x reg=0x%02x val=0x%02x ret=%d ack=%u/%u/%u\n",
                      addr, reg, read_val, ret, ack_addr_w, ack_reg, ack_addr_r);
    return ret;
}

static int pi_ai_mic_pre_latch_detect(void)
{
    uint8_t input_rs = 0;
    uint8_t input_stop = 0;
    uint8_t status8 = 0;
    int ret = 0;

    ret |= pi_ai_mic_pre_i2c_reg_read(PCAL6408A_I2C_ADDR, PCAL6408A_INPUT_PORT, &input_rs);
    ret |= pi_ai_mic_pre_i2c_reg_read_stop(PCAL6408A_I2C_ADDR, PCAL6408A_INPUT_PORT, &input_stop);
    ret |= pi_ai_mic_pre_i2c_reg_read(PCAL6408A_I2C_ADDR, PCAL6408A_INTERRUPT_STATUS, &status8);

    if (ret == 0) {
        uint8_t jack_level_rs = (uint8_t)((input_rs >> PCAL_JACK_DET_PIN) & 0x01u);
        uint8_t jack_level_stop = (uint8_t)((input_stop >> PCAL_JACK_DET_PIN) & 0x01u);
        uint8_t mic_level_rs = (uint8_t)((input_rs >> PCAL_MIC_DETECT_PIN) & 0x01u);
        uint8_t mic_level_stop = (uint8_t)((input_stop >> PCAL_MIC_DETECT_PIN) & 0x01u);
        (void) jack_level_rs;
        (void) mic_level_rs;
        /*
         * The standalone LED detector validated P5/P6 from INPUT_PORT only.
         * INTERRUPT_STATUS can remain sticky and must not be interpreted as
         * the current jack level, otherwise jack is falsely latched high.
         */
        /*
         * Use the STOP-separated read as the boot truth. The repeated-start
         * read can occasionally sample P5 high during early bring-up; OR-ing
         * both reads falsely latches "jack inserted" and leaves the speaker
         * muted until a later route change.
         */
        uint8_t jack_level = jack_level_stop;
        uint8_t mic_level = mic_level_stop;

        pi_ai_mic_boot_detect_jack = (jack_level == PI_AI_MIC_JACK_INSERTED_LEVEL) ? 1 : 0;
        pi_ai_mic_boot_detect_mic_square = (mic_level == PI_AI_MIC_MIC_DETECT_SQUARE_LEVEL) ? 1 : 0;
        pi_ai_mic_boot_detect_valid = 1;
    } else {
        pi_ai_mic_boot_detect_valid = 0;
        pi_ai_mic_boot_detect_jack = 1;
        pi_ai_mic_boot_detect_mic_square = 0;
    }

    PI_AI_MIC_PRE_LOG("PI_AI_MIC PRE detect latch ret=%d input_rs=0x%02x input_stop=0x%02x status8=0x%02x valid=%d jack=%d mic_square=%d\n",
                      ret,
                      input_rs,
                      input_stop,
                      status8,
                      pi_ai_mic_boot_detect_valid,
                      pi_ai_mic_boot_detect_jack,
                      pi_ai_mic_boot_detect_mic_square);
    return ret;
}

static void pi_ai_mic_pre_dac_reset(void)
{
    port_t gpo = GPO_TILE_0_PORT_8C;
    hwtimer_t timer = hwtimer_alloc();
    uint32_t high = PI_AI_MIC_GPO_ALL_SAFE_HIGH;

    port_enable(gpo);
    port_write_control_word(gpo, XS1_SETC_DRIVE_DRIVE);
    port_out(gpo, high);
    hwtimer_delay(timer, PI_AI_MIC_PRE_RESET_TICKS);
    port_out(gpo, high & ~PI_AI_MIC_BIT(GPO_DAC_RST_N_PIN));
    hwtimer_delay(timer, PI_AI_MIC_PRE_RESET_TICKS);
    port_out(gpo, high);
    hwtimer_delay(timer, PI_AI_MIC_PRE_RESET_TICKS);
    port_disable(gpo);
    hwtimer_free(timer);
    PI_AI_MIC_PRE_LOG("PI_AI_MIC PRE DAC reset via GPO bit %u done\n", GPO_DAC_RST_N_PIN);
}

static int pi_ai_mic_pre_pcal_init(void)
{
    int ret = 0;

    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_OUTPUT_PORT,
                                       PI_AI_MIC_BIT(PCAL_XVF_RST_N_PIN) |
                                       PI_AI_MIC_BIT(PCAL_DAC_RST_N_PIN));
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_POLARITY_INV, 0x00);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_INPUT_LATCH, 0x00);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_PULLUPDOWN_SEL, 0x00);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_PULLUPDOWN_EN, 0x00);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_INTERRUPT_MASK, 0xFF);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_OUTPUT_PORT_CONFIG, 0x00);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_CONF, 0xFF);

    return ret;
}

static int pi_ai_mic_pre_pcal_audio_output_init(void)
{
    int ret = 0;
    uint8_t input_mask = PI_AI_MIC_BIT(PCAL_INT_N_PIN) |
                         PI_AI_MIC_BIT(PCAL_JACK_DET_PIN) |
                         PI_AI_MIC_BIT(PCAL_MIC_DETECT_PIN);
    uint8_t output_defaults = PI_AI_MIC_BIT(PCAL_XVF_RST_N_PIN) |
                              PI_AI_MIC_BIT(PCAL_DAC_RST_N_PIN);

    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_OUTPUT_PORT, output_defaults);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_POLARITY_INV, 0x00);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_INPUT_LATCH, 0x00);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_PULLUPDOWN_SEL, 0x00);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_PULLUPDOWN_EN, 0x00);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_INTERRUPT_MASK, 0xFF);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_OUTPUT_PORT_CONFIG, 0x00);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_CONF, input_mask);

    return ret;
}

static int pi_ai_mic_pre_pcal_dac_reset(void)
{
    hwtimer_t timer = hwtimer_alloc();
    uint8_t reset_low = PI_AI_MIC_BIT(PCAL_XVF_RST_N_PIN);
    uint8_t reset_high = PI_AI_MIC_BIT(PCAL_XVF_RST_N_PIN) |
                         PI_AI_MIC_BIT(PCAL_DAC_RST_N_PIN);
    int ret = 0;

    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_OUTPUT_PORT, reset_low);
    hwtimer_delay(timer, PI_AI_MIC_PRE_RESET_TICKS);
    ret |= pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_OUTPUT_PORT, reset_high);
    hwtimer_delay(timer, PI_AI_MIC_PRE_RESET_TICKS);
    hwtimer_free(timer);

    PI_AI_MIC_PRE_LOG("PI_AI_MIC PRE DAC reset via PCAL P%u ret=%d\n", PCAL_DAC_RST_N_PIN, ret);
    return ret;
}

static int pi_ai_mic_pre_amp_low(void)
{
    uint8_t outputs = PI_AI_MIC_BIT(PCAL_XVF_RST_N_PIN) |
                      PI_AI_MIC_BIT(PCAL_DAC_RST_N_PIN);
#if PI_AI_MIC_HARD_SPEAKER_MUTE
    /* Leave AMP_CTRL1/P4 and AMP_CTRL2/P7 low even during pre-RTOS bring-up. */
#elif PI_AI_MIC_PRE_RTOS_SPEAKER_AMP_CTRL2_ONLY
    outputs |= PI_AI_MIC_BIT(PCAL_AMP_CTRL2_PIN);
#elif PI_AI_MIC_PRE_RTOS_SPEAKER_AMP_FULL
    outputs |= PI_AI_MIC_BIT(PCAL_AMP_CTRL1_PIN);
    outputs |= PI_AI_MIC_BIT(PCAL_AMP_CTRL2_PIN);
#else
    outputs |= PI_AI_MIC_BIT(PCAL_AMP_CTRL1_PIN);
#endif
    int ret = pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_OUTPUT_PORT, outputs);
    PI_AI_MIC_PRE_LOG("PI_AI_MIC PRE amp output=0x%02x ret=%d ctrl1=%u ctrl2=%u\n",
                      outputs,
                      ret,
                      (outputs & PI_AI_MIC_BIT(PCAL_AMP_CTRL1_PIN)) ? 1u : 0u,
                      (outputs & PI_AI_MIC_BIT(PCAL_AMP_CTRL2_PIN)) ? 1u : 0u);
    return ret;
}

static int pi_ai_mic_pre_amp_shutdown(void)
{
    uint8_t outputs = PI_AI_MIC_BIT(PCAL_XVF_RST_N_PIN) |
                      PI_AI_MIC_BIT(PCAL_DAC_RST_N_PIN);
    return pi_ai_mic_pre_i2c_reg_write(PCAL6408A_I2C_ADDR, PCAL6408A_OUTPUT_PORT, outputs);
}

static int pi_ai_mic_pre_dac_write(uint8_t reg, uint8_t val)
{
    return pi_ai_mic_pre_i2c_reg_write(DAC3101_I2C_DEVICE_ADDR, reg, val);
}

static int pi_ai_mic_pre_dac_init(void)
{
    int ret = 0;
    const unsigned sample_rate = appconfLRCLK_NOMINAL_HZ;
#if PI_AI_MIC_HARD_SPEAKER_MUTE
    const int boot_jack_inserted = 1;
#elif PI_AI_MIC_FORCE_SPEAKER_ROUTE
    const int boot_jack_inserted = 0;
#else
    const int boot_jack_inserted = (!pi_ai_mic_boot_detect_valid) || (pi_ai_mic_boot_detect_jack != 0);
#endif
    const unsigned PLLP = 1;
    const unsigned PLLR = 4;
    const unsigned PLLJ = (sample_rate == 16000) ? 24 : (sample_rate == 32000) ? 12 : 8;
    const unsigned PLLD = 0;
    const unsigned NDAC = 4;
    const unsigned MDAC = (sample_rate == 48000) ? 4 : 6;
    const unsigned DOSR = (sample_rate == 16000) ? 256 : 128;
    hwtimer_t timer = hwtimer_alloc();

    ret |= pi_ai_mic_pre_dac_write(DAC3101_PAGE_CTRL, 0x00);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_SW_RST, 0x01);
    hwtimer_delay(timer, PI_AI_MIC_PRE_RESET_TICKS);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_PLL_J, (uint8_t)PLLJ);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_PLL_D_LSB, PLLD & 0xFF);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_PLL_D_MSB, (PLLD & 0xFF00) >> 8);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_B_DIV_VAL, 0x81);
    hwtimer_delay(timer, PI_AI_MIC_PRE_RESET_TICKS);

    ret |= pi_ai_mic_pre_dac_write(DAC3101_CLK_GEN_MUX, (0x01u << 2) + 0x03u);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_PLL_P_R, 0x80 + (PLLP << 4) + PLLR);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_NDAC_VAL, 0x80 + NDAC);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_MDAC_VAL, 0x80 + MDAC);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_DOSR_VAL_LSB, DOSR & 0xFF);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_DOSR_VAL_MSB, (DOSR & 0xFF00) >> 8);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_CLKOUT_MUX, 0x04);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_CLKOUT_M_VAL, 0x81);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_GPIO1_IO, 0x10);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_CODEC_IF, 0x20);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_DAC_PROC_BLK, 0x01);

    ret |= pi_ai_mic_pre_dac_write(DAC3101_PAGE_CTRL, 0x01);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_HP_DRVR, 0x14);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_HP_DEPOP, 0x4E);
    if (boot_jack_inserted) {
        ret |= pi_ai_mic_pre_dac_write(DAC3101_DAC_OP_MIX, 0x44);
    } else {
#if PI_AI_MIC_SPEAKER_USE_XMOS_STEREO_ROUTE
        ret |= pi_ai_mic_pre_dac_write(DAC3101_DAC_OP_MIX, 0x44);
#else
        ret |= pi_ai_mic_pre_dac_write(DAC3101_DAC_OP_MIX, 0x41);
#endif
    }
    ret |= pi_ai_mic_pre_dac_write(DAC3101_HPL_DRVR,
                                   boot_jack_inserted ? PI_AI_MIC_HEADPHONE_DRIVER_REG : 0x06);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_HPR_DRVR,
                                   boot_jack_inserted ? PI_AI_MIC_HEADPHONE_DRIVER_REG : 0x06);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_HP_LINEOUT,
                                   boot_jack_inserted ? PI_AI_MIC_HEADPHONE_LINEOUT_CTRL : 0x00);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_SPKL_DRVR, 0x0C);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_SPKR_DRVR, 0x0C);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_HP_DRVR, 0xD4);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_SPK_AMP, 0xC6);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_HPL_VOL_A, PI_AI_MIC_HEADPHONE_ANALOG_VOL);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_HPR_VOL_A, PI_AI_MIC_HEADPHONE_ANALOG_VOL);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_SPKL_VOL_A,
                                   boot_jack_inserted ? PI_AI_MIC_HEADPHONE_MODE_SPEAKER_ANALOG_VOL : PI_AI_MIC_SPEAKER_ANALOG_VOL);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_SPKR_VOL_A,
                                   boot_jack_inserted ? PI_AI_MIC_HEADPHONE_MODE_SPEAKER_ANALOG_VOL : PI_AI_MIC_SPEAKER_ANALOG_VOL);

#if PI_AI_MIC_SPEAKER_GENELEC_BIQUADS
    ret |= pi_ai_mic_pre_dac_write(DAC3101_PAGE_CTRL, 0x08);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_A_N0_HI, 0x7F);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_A_N0_LO, 0xFF);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_A_N1_HI, 0x82);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_A_N1_LO, 0xC4);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_A_N2_HI, 0x7A);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_A_N2_LO, 0x9A);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_A_D1_HI, 0x7B);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_A_D1_LO, 0x14);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_A_D2_HI, 0x89);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_A_D2_LO, 0x6F);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_A_N0_HI, 0x7F);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_A_N0_LO, 0xFF);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_A_N1_HI, 0x82);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_A_N1_LO, 0xC4);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_A_N2_HI, 0x7A);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_A_N2_LO, 0x9A);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_A_D1_HI, 0x7B);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_A_D1_LO, 0x14);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_A_D2_HI, 0x89);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_A_D2_LO, 0x6F);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_B_N0_HI, 0x7F);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_B_N0_LO, 0xFF);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_B_N1_HI, 0x80);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_B_N1_LO, 0x01);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_B_N2_HI, 0x7F);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_B_N2_LO, 0xFF);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_B_D1_HI, 0x7E);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_B_D1_LO, 0x38);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_B_D2_HI, 0x83);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_B_D2_LO, 0x83);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_B_N0_HI, 0x7F);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_B_N0_LO, 0xFF);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_B_N1_HI, 0x80);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_B_N1_LO, 0x01);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_B_N2_HI, 0x7F);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_B_N2_LO, 0xFF);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_B_D1_HI, 0x7E);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_B_D1_LO, 0x38);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_B_D2_HI, 0x83);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_B_D2_LO, 0x83);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_C_N0_HI, 0x7F);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_C_N0_LO, 0xFF);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_C_N1_HI, 0x80);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_C_N1_LO, 0x01);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_C_N2_HI, 0x7F);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_C_N2_LO, 0xFF);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_C_D1_HI, 0x7E);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_C_D1_LO, 0x38);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_C_D2_HI, 0x83);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_L_BQD_C_D2_LO, 0x83);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_C_N0_HI, 0x7F);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_C_N0_LO, 0xFF);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_C_N1_HI, 0x80);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_C_N1_LO, 0x01);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_C_N2_HI, 0x7F);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_C_N2_LO, 0xFF);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_C_D1_HI, 0x7E);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_C_D1_LO, 0x38);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_C_D2_HI, 0x83);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_R_BQD_C_D2_LO, 0x83);
    hwtimer_delay(timer, 10000000u);
#endif

    ret |= pi_ai_mic_pre_dac_write(DAC3101_PAGE_CTRL, 0x00);
    if (boot_jack_inserted) {
        ret |= pi_ai_mic_pre_dac_write(DAC3101_DAC_DAT_PATH, PI_AI_MIC_DAC_DAT_PATH_STEREO);
    } else {
#if PI_AI_MIC_SPEAKER_USE_XMOS_STEREO_ROUTE
        ret |= pi_ai_mic_pre_dac_write(DAC3101_DAC_DAT_PATH, PI_AI_MIC_DAC_DAT_PATH_STEREO);
#else
        ret |= pi_ai_mic_pre_dac_write(DAC3101_DAC_DAT_PATH, PI_AI_MIC_DAC_DAT_PATH_SPEAKER_DIFF);
#endif
    }
    ret |= pi_ai_mic_pre_dac_write(DAC3101_DACL_VOL_D, PI_AI_MIC_DAC_DIGITAL_VOL);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_DACR_VOL_D, PI_AI_MIC_DAC_DIGITAL_VOL);
    ret |= pi_ai_mic_pre_dac_write(DAC3101_DAC_VOL, 0x00);

    hwtimer_free(timer);
    return ret;
}

static uint32_t pi_ai_mic_pre_detect_led_gpo_value(int success)
{
    uint32_t led_value = PI_AI_MIC_GPO_ALL_SAFE_HIGH;

    if (success && pi_ai_mic_boot_detect_valid) {
        if (pi_ai_mic_boot_detect_jack) {
            led_value &= ~PI_AI_MIC_BIT(GPO_LED_RED_PIN);
        }
        if (pi_ai_mic_boot_detect_mic_square) {
            led_value &= ~PI_AI_MIC_BIT(GPO_LED_GREEN_PIN);
        }
    } else if (success) {
        led_value &= ~PI_AI_MIC_BIT(GPO_LED_GREEN_PIN);
    } else {
        led_value &= ~PI_AI_MIC_BIT(GPO_LED_RED_PIN);
    }

    return led_value;
}

static void pi_ai_mic_pre_led_status(int success)
{
    port_t gpo = GPO_TILE_0_PORT_8C;
    uint32_t led_value = pi_ai_mic_pre_detect_led_gpo_value(success);

    port_enable(gpo);
    port_write_control_word(gpo, XS1_SETC_DRIVE_DRIVE);
    port_out(gpo, led_value);
    PI_AI_MIC_PRE_LOG("PI_AI_MIC PRE detect LED persist valid=%d jack=%d mic_square=%d gpo=0x%02x\n",
                      pi_ai_mic_boot_detect_valid,
                      pi_ai_mic_boot_detect_jack,
                      pi_ai_mic_boot_detect_mic_square,
                      (unsigned) led_value);
}

static void pi_ai_mic_pre_rtos_speaker_hw_init(void)
{
    hwtimer_t timer = hwtimer_alloc();
    int ret = -1;

    PI_AI_MIC_PRE_LOG("PI_AI_MIC PRE speaker hw init start\n");
    hwtimer_delay(timer, PI_AI_MIC_PRE_STARTUP_DELAY_TICKS);

    for (unsigned attempt = 0; attempt < PI_AI_MIC_PRE_INIT_ATTEMPTS; attempt++) {
        int pcal_ret;
        int pcal_reset_ret;
        int dac_ret;
        int amp_ret;

        pi_ai_mic_pre_init_attempt = attempt + 1;
        ret = 0;
        pi_ai_mic_pre_init_stage = 1;
        pi_ai_mic_pre_dac_reset();
        pi_ai_mic_pre_init_stage = 2;
        pcal_ret = pi_ai_mic_pre_pcal_init();
        pi_ai_mic_pre_pcal_ret = pcal_ret;
        if (pcal_ret == 0) {
            (void) pi_ai_mic_pre_latch_detect();
            pcal_ret |= pi_ai_mic_pre_pcal_audio_output_init();
            pi_ai_mic_pre_pcal_ret = pcal_ret;
        } else {
            pi_ai_mic_boot_detect_valid = 0;
            pi_ai_mic_boot_detect_jack = 1;
            pi_ai_mic_boot_detect_mic_square = 0;
        }
        pi_ai_mic_pre_init_stage = 3;
        pcal_reset_ret = pi_ai_mic_pre_pcal_dac_reset();
        pi_ai_mic_pre_pcal_reset_ret = pcal_reset_ret;
        pi_ai_mic_pre_init_stage = 4;
        dac_ret = pi_ai_mic_pre_dac_init();
        pi_ai_mic_pre_dac_ret = dac_ret;
        pi_ai_mic_pre_init_stage = 5;
#if PI_AI_MIC_FORCE_SPEAKER_ROUTE
        amp_ret = pi_ai_mic_pre_amp_low();
#else
        amp_ret = pi_ai_mic_boot_detect_jack ? pi_ai_mic_pre_amp_shutdown() : pi_ai_mic_pre_amp_low();
#endif
        pi_ai_mic_pre_amp_ret = amp_ret;
        ret |= pcal_ret;
        ret |= pcal_reset_ret;
        ret |= dac_ret;
        ret |= amp_ret;
        pi_ai_mic_pre_init_ret = ret;
        if (ret == 0) {
            break;
        }
        hwtimer_delay(timer, PI_AI_MIC_PRE_RETRY_DELAY_TICKS);
    }

    hwtimer_free(timer);
    pi_ai_mic_pre_init_stage = (ret == 0) ? 100u : 200u;
    pi_ai_mic_pre_led_status(ret == 0);
    PI_AI_MIC_PRE_LOG("PI_AI_MIC PRE speaker hw init %s\n", ret == 0 ? "ok" : "failed");
}

static void pi_ai_mic_late_speaker_hw_init_task(void *arg)
{
    (void) arg;

    pi_ai_mic_late_init_count++;
    pi_ai_mic_late_init_stage = 1;
    PI_AI_MIC_PRE_LOG("PI_AI_MIC LATE speaker hw init wait %u ms\n", PI_AI_MIC_LATE_INIT_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(PI_AI_MIC_LATE_INIT_DELAY_MS));

    int ret = 0;

    PI_AI_MIC_PRE_LOG("PI_AI_MIC LATE speaker hw init start\n");

    pi_ai_mic_late_init_stage = 2;
    int pcal_ret = pi_ai_mic_pre_pcal_init();
    if (pcal_ret == 0) {
        pcal_ret |= pi_ai_mic_pre_pcal_audio_output_init();
    }
    pi_ai_mic_late_pcal_ret = pcal_ret;
    ret |= pcal_ret;

    pi_ai_mic_late_init_stage = 3;
    int amp_shutdown_ret = pi_ai_mic_pre_amp_shutdown();
    pi_ai_mic_late_amp_shutdown_ret = amp_shutdown_ret;
    ret |= amp_shutdown_ret;

    pi_ai_mic_late_init_stage = 4;
    int dac_ret = pi_ai_mic_pre_dac_init();
    pi_ai_mic_late_dac_ret = dac_ret;
    ret |= dac_ret;

    pi_ai_mic_late_init_stage = 5;
    int amp_ret = pi_ai_mic_pre_amp_low();
    pi_ai_mic_late_amp_ret = amp_ret;
    ret |= amp_ret;

    pi_ai_mic_late_init_ret = ret;
    pi_ai_mic_late_init_stage = (ret == 0) ? 100u : 200u;
    pi_ai_mic_pre_led_status(ret == 0);
    PI_AI_MIC_PRE_LOG("PI_AI_MIC LATE speaker hw init ret=%d pcal=%d amp_shutdown=%d dac=%d amp=%d\n",
                      ret, pcal_ret, amp_shutdown_ret, dac_ret, amp_ret);

    vTaskDelete(NULL);
}
#endif

#if PI_AI_MIC_LED_HEARTBEAT_ENABLE
#ifndef PI_AI_MIC_GPO_ALL_SAFE_HIGH
#define PI_AI_MIC_GPO_ALL_SAFE_HIGH              0xFFu
#endif
#define PI_AI_MIC_LED_HEARTBEAT_MS               500u
#define PI_AI_MIC_LED_HEARTBEAT_START_DELAY_MS   2000u

static void pi_ai_mic_led_heartbeat_task(void *arg)
{
    (void) arg;

    port_t gpo = GPO_TILE_0_PORT_8C;
    uint32_t state = 0;

    port_enable(gpo);
    port_write_control_word(gpo, XS1_SETC_DRIVE_DRIVE);
    vTaskDelay(pdMS_TO_TICKS(PI_AI_MIC_LED_HEARTBEAT_START_DELAY_MS));

    for (;;) {
        uint32_t gpo_value = PI_AI_MIC_GPO_ALL_SAFE_HIGH;

        if (state) {
            gpo_value &= ~(1u << GPO_LED_RED_PIN);
        } else {
            gpo_value &= ~(1u << GPO_LED_GREEN_PIN);
        }

        port_out(gpo, gpo_value);
        state ^= 1u;
        vTaskDelay(pdMS_TO_TICKS(PI_AI_MIC_LED_HEARTBEAT_MS));
    }
}
#endif

#if PI_AI_MIC_DETECT_LED_MONITOR_ENABLE && PI_AI_MIC_DETECT_LED_MONITOR_DIRECT_GPO_ENABLE
#ifndef PI_AI_MIC_GPO_ALL_SAFE_HIGH
#define PI_AI_MIC_GPO_ALL_SAFE_HIGH              0xFFu
#endif
#ifndef PI_AI_MIC_BIT
#define PI_AI_MIC_BIT(n)                         (1u << (n))
#endif
#ifndef PCAL6408A_I2C_ADDR
#define PCAL6408A_I2C_ADDR                       0x20
#endif
#ifndef PCAL6408A_INPUT_PORT
#define PCAL6408A_INPUT_PORT                     0x00
#endif

static void pi_ai_mic_detect_led_gpo_write(port_t gpo_port, uint32_t gpo_value)
{
    /* Keep the runtime path identical to the validated standalone detector:
     * configure the GPO port once, then only update the output value. */
    port_out(gpo_port, gpo_value);
    port_sync(gpo_port);
}

static void pi_ai_mic_detect_led_monitor_task(void *arg)
{
    (void) arg;

    port_t gpo_port = GPO_TILE_0_PORT_8C;
    unsigned log_count = 0;
    uint32_t last_gpo_value = PI_AI_MIC_GPO_ALL_SAFE_HIGH;
    uint8_t last_jack = 0;
    bool last_jack_valid = false;
    uint8_t last_log_jack = 0;
    uint8_t last_log_mic = 0;
    bool log_state_valid = false;
    unsigned fail_count = 0;

    vTaskDelay(pdMS_TO_TICKS(PI_AI_MIC_DETECT_LED_MONITOR_START_DELAY_MS));

    /*
     * The generic GPO servicer stays alive for control-plane compatibility, but
     * it skips the LED port in this profile. Raw port access is intentional: it
     * is the path already validated by the standalone PCAL detector.
     */
    port_enable(gpo_port);
    port_write_control_word(gpo_port, XS1_SETC_DRIVE_DRIVE);
    pi_ai_mic_detect_led_gpo_write(gpo_port, last_gpo_value);

    for (;;) {
        bool valid = false;
        bool jack_inserted = false;
        bool mic_square = false;
        int ret = pi_ai_mic_read_detect_state(&valid, &jack_inserted, &mic_square);
        uint32_t gpo_value = last_gpo_value;

        if (ret == 0 && valid) {
            uint8_t jack = jack_inserted ? 1u : 0u;
            uint8_t mic = mic_square ? 1u : 0u;
            int route_ret = 0;
            bool route_write = false;

            gpo_value = PI_AI_MIC_GPO_ALL_SAFE_HIGH;
            if (jack) {
                gpo_value &= ~PI_AI_MIC_BIT(GPO_LED_RED_PIN);
            }
            if (mic) {
                gpo_value &= ~PI_AI_MIC_BIT(GPO_LED_GREEN_PIN);
            }

            pi_ai_mic_boot_detect_valid = 1;
            pi_ai_mic_boot_detect_jack = jack ? 1 : 0;
            pi_ai_mic_boot_detect_mic_square = mic ? 1 : 0;
            fail_count = 0;
            if (!last_jack_valid || jack != last_jack) {
#if PI_AI_MIC_JACK_AMP_EDGE_MONITOR_ENABLE
                route_ret = pi_ai_mic_apply_output_route(jack != 0,
                                                         PI_AI_MIC_SPEAKER_ENABLE_WHEN_NO_JACK);
                route_write = true;
#endif
                last_jack = jack;
                last_jack_valid = true;
            }

            if (!log_state_valid || jack != last_log_jack || mic != last_log_mic || route_write || log_count == 0) {
                printf("PI_AI_MIC detect task jack=%u mic_square=%u gpo=0x%02x route=%s write=%u ret=%d\n",
                       jack,
                       mic,
                       (unsigned)gpo_value,
                       jack ? "headphone" : "speaker",
                       route_write ? 1u : 0u,
                       route_ret);
                last_log_jack = jack;
                last_log_mic = mic;
                log_state_valid = true;
            }
            pi_ai_mic_detect_led_gpo_write(gpo_port, gpo_value);
            last_gpo_value = gpo_value;
        } else {
            pi_ai_mic_boot_detect_valid = 0;
            fail_count++;
            pi_ai_mic_detect_led_gpo_write(gpo_port, last_gpo_value);
            log_state_valid = false;
            if (log_count == 0) {
                printf("PI_AI_MIC detect LED PCAL read failed ret=%d fail_count=%u keeping_gpo=0x%02x\n",
                       ret,
                       fail_count,
                       (unsigned)last_gpo_value);
            }
        }

        log_count++;
        if (log_count >= PI_AI_MIC_DETECT_LED_MONITOR_LOG_PERIOD) {
            log_count = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(PI_AI_MIC_DETECT_LED_MONITOR_PERIOD_MS));
    }
}
#endif

#if PI_AI_MIC_I2C_WAVEFORM_TEST_ENABLE
#define PI_AI_MIC_I2C_WAVEFORM_HALF_PERIOD_TICKS 500u
#define PI_AI_MIC_I2C_WAVEFORM_LED_TOGGLE_CYCLES 50000u
#define PI_AI_MIC_I2C_WAVEFORM_SDA_PATTERN       0xA5u
#define PI_AI_MIC_GPO_ALL_SAFE_HIGH              0xFFu

static void pi_ai_mic_tile0_waveform_test(void)
{
    port_t scl = PORT_I2C_SCL;
    port_t sda = PORT_I2C_SDA;
    port_t gpo = GPO_TILE_0_PORT_8C;
    hwtimer_t timer = hwtimer_alloc();
    uint32_t cycles = 0;
    uint32_t bit_index = 0;
    uint32_t led_state = 0;

    port_enable(scl);
    port_enable(sda);
    port_enable(gpo);

    port_write_control_word(scl, XS1_SETC_DRIVE_DRIVE);
    port_write_control_word(sda, XS1_SETC_DRIVE_DRIVE);
    port_write_control_word(gpo, XS1_SETC_DRIVE_DRIVE);

    port_out(scl, 1);
    port_out(sda, 1);
    port_out(gpo, PI_AI_MIC_GPO_ALL_SAFE_HIGH & ~(1u << GPO_LED_RED_PIN));

    printf("PI_AI_MIC I2C waveform test: push-pull SCL=PORT_I2C_SCL/X0D37 SDA=PORT_I2C_SDA/X0D38\n");
    printf("PI_AI_MIC I2C waveform test: LED alternates, SCL clocks continuously, SDA pattern=0x%02x\n",
           PI_AI_MIC_I2C_WAVEFORM_SDA_PATTERN);

    for (;;) {
        uint32_t sda_bit = (PI_AI_MIC_I2C_WAVEFORM_SDA_PATTERN >> (7 - (bit_index & 0x7))) & 0x1;

        port_out(scl, 0);
        port_out(sda, sda_bit);
        hwtimer_delay(timer, PI_AI_MIC_I2C_WAVEFORM_HALF_PERIOD_TICKS);

        port_out(scl, 1);
        hwtimer_delay(timer, PI_AI_MIC_I2C_WAVEFORM_HALF_PERIOD_TICKS);

        bit_index++;
        cycles++;

        if (cycles >= PI_AI_MIC_I2C_WAVEFORM_LED_TOGGLE_CYCLES) {
            uint32_t gpo_value;

            cycles = 0;
            led_state ^= 1;

            gpo_value = PI_AI_MIC_GPO_ALL_SAFE_HIGH;
            if (led_state) {
                gpo_value &= ~(1u << GPO_LED_RED_PIN);
            } else {
                gpo_value &= ~(1u << GPO_LED_GREEN_PIN);
            }
            port_out(gpo, gpo_value);
        }
    }
}
#endif

#if (PI_AI_MIC_I2C_ELECTRICAL_TEST_ENABLE && PI_AI_MIC_I2C_ELECTRICAL_TILE0_ENABLE) || \
        (PI_AI_MIC_I2C_FORCED_PROBE_TEST_ENABLE && PI_AI_MIC_I2C_FORCED_PROBE_TILE0_ENABLE) || \
        (PI_AI_MIC_I2C_PORT_SCAN_TEST_ENABLE && PI_AI_MIC_I2C_PORT_SCAN_TILE0_ENABLE) || \
        (PI_AI_MIC_I2C_PIN_LOCATOR_TEST_ENABLE && PI_AI_MIC_I2C_PIN_LOCATOR_TILE0_ENABLE)
#define PI_AI_MIC_I2C_SCAN_HALF_PERIOD_TICKS     500u
#define PI_AI_MIC_I2C_SCAN_PAUSE_TICKS           50000u
#define PI_AI_MIC_I2C_SCAN_LED_TOGGLE_CYCLES     80u
#define PI_AI_MIC_GPO_ALL_SAFE_HIGH              0xFFu

typedef struct {
    port_t gpo;
    uint32_t led_state;
    uint32_t led_counter;
} pi_ai_mic_tile0_scan_led_t;

typedef struct {
    const char *name;
    port_t port;
} pi_ai_mic_scan_port_t;

static const pi_ai_mic_scan_port_t pi_ai_mic_i2c_scan_ports[] = {
    { "1A", XS1_PORT_1A },
    { "1B", XS1_PORT_1B },
    { "1C", XS1_PORT_1C },
    { "1D", XS1_PORT_1D },
    { "1E", XS1_PORT_1E },
    { "1F", XS1_PORT_1F },
    { "1G", XS1_PORT_1G },
    { "1H", XS1_PORT_1H },
    { "1I", XS1_PORT_1I },
    { "1J", XS1_PORT_1J },
    { "1K", XS1_PORT_1K },
    { "1L", XS1_PORT_1L },
    { "1M", XS1_PORT_1M },
    { "1N", XS1_PORT_1N },
    { "1O", XS1_PORT_1O },
    { "1P", XS1_PORT_1P },
};

static void pi_ai_mic_scan_delay(hwtimer_t timer)
{
    hwtimer_delay(timer, PI_AI_MIC_I2C_SCAN_HALF_PERIOD_TICKS);
}

static void pi_ai_mic_i2c_release(port_t port)
{
    port_write_control_word(port, XS1_SETC_DRIVE_PULL_UP);
    port_out(port, 1);
}

static void pi_ai_mic_i2c_drive_low(port_t port)
{
    port_write_control_word(port, XS1_SETC_DRIVE_PULL_UP);
    port_out(port, 0);
}

static uint32_t pi_ai_mic_i2c_read(port_t port)
{
    return port_peek(port) & 1u;
}

static void pi_ai_mic_i2c_stop(port_t scl, port_t sda, hwtimer_t timer)
{
    pi_ai_mic_i2c_drive_low(sda);
    pi_ai_mic_scan_delay(timer);
    pi_ai_mic_i2c_release(scl);
    pi_ai_mic_scan_delay(timer);
    pi_ai_mic_i2c_release(sda);
    pi_ai_mic_scan_delay(timer);
}

static void pi_ai_mic_i2c_recover(port_t scl, port_t sda, hwtimer_t timer)
{
    pi_ai_mic_i2c_release(sda);

    for (unsigned i = 0; i < 9; i++) {
        pi_ai_mic_i2c_drive_low(scl);
        pi_ai_mic_scan_delay(timer);
        pi_ai_mic_i2c_release(scl);
        pi_ai_mic_scan_delay(timer);
    }

    pi_ai_mic_i2c_stop(scl, sda, timer);
}

static int pi_ai_mic_i2c_start(port_t scl, port_t sda, hwtimer_t timer)
{
    pi_ai_mic_i2c_release(sda);
    pi_ai_mic_i2c_release(scl);
    pi_ai_mic_scan_delay(timer);

    if (!pi_ai_mic_i2c_read(scl) || !pi_ai_mic_i2c_read(sda)) {
        return -1;
    }

    pi_ai_mic_i2c_drive_low(sda);
    pi_ai_mic_scan_delay(timer);
    pi_ai_mic_i2c_drive_low(scl);
    pi_ai_mic_scan_delay(timer);
    return 0;
}

static int pi_ai_mic_i2c_write_byte(port_t scl, port_t sda, hwtimer_t timer, uint8_t byte)
{
    for (int bit = 7; bit >= 0; bit--) {
        if ((byte >> bit) & 1u) {
            pi_ai_mic_i2c_release(sda);
        } else {
            pi_ai_mic_i2c_drive_low(sda);
        }

        pi_ai_mic_scan_delay(timer);
        pi_ai_mic_i2c_release(scl);
        pi_ai_mic_scan_delay(timer);
        pi_ai_mic_i2c_drive_low(scl);
        pi_ai_mic_scan_delay(timer);
    }

    pi_ai_mic_i2c_release(sda);
    pi_ai_mic_scan_delay(timer);
    pi_ai_mic_i2c_release(scl);
    pi_ai_mic_scan_delay(timer);
    int ack = pi_ai_mic_i2c_read(sda) == 0;
    pi_ai_mic_i2c_drive_low(scl);
    pi_ai_mic_scan_delay(timer);

    return ack;
}

static int pi_ai_mic_i2c_probe_address(port_t scl, port_t sda, hwtimer_t timer, uint8_t address)
{
    int ack = 0;

    if (pi_ai_mic_i2c_start(scl, sda, timer) == 0) {
        ack = pi_ai_mic_i2c_write_byte(scl, sda, timer, (uint8_t)(address << 1));
    }

    pi_ai_mic_i2c_stop(scl, sda, timer);
    return ack;
}

static void pi_ai_mic_scan_set_led(port_t gpo, uint32_t led_state)
{
    uint32_t gpo_value = PI_AI_MIC_GPO_ALL_SAFE_HIGH;

    if (led_state) {
        gpo_value &= ~(1u << GPO_LED_RED_PIN);
    } else {
        gpo_value &= ~(1u << GPO_LED_GREEN_PIN);
    }

    port_out(gpo, gpo_value);
}

static void pi_ai_mic_tile0_deep_scan_tick(void *arg)
{
    pi_ai_mic_tile0_scan_led_t *led = (pi_ai_mic_tile0_scan_led_t *)arg;

    led->led_counter++;
    if (led->led_counter >= PI_AI_MIC_I2C_SCAN_LED_TOGGLE_CYCLES) {
        led->led_counter = 0;
        led->led_state ^= 1;
        pi_ai_mic_scan_set_led(led->gpo, led->led_state);
    }
}

static void pi_ai_mic_tile0_i2c_port_scan_test(void)
{
    pi_ai_mic_deep_i2c_scan_run(
            "TILE0",
            pi_ai_mic_deep_i2c_tile0_lines,
            sizeof(pi_ai_mic_deep_i2c_tile0_lines) / sizeof(pi_ai_mic_deep_i2c_tile0_lines[0]),
            NULL,
            NULL);

    hwtimer_t timer = hwtimer_alloc();
    port_t gpo = GPO_TILE_0_PORT_8C;
    unsigned scan_pass = 0;
    uint32_t led_state = 0;
    uint32_t led_counter = 0;

    port_enable(gpo);
    pi_ai_mic_scan_set_led(gpo, led_state);

    printf("PI_AI_MIC I2C port scan start: probing PCAL6408A 0x20 and DAC3101 0x18\n");
    printf("PI_AI_MIC I2C port scan candidates: XS1_PORT_1A..XS1_PORT_1P, distinct SCL/SDA pairs\n");

    for (;;) {
        printf("PI_AI_MIC I2C scan pass %u begin\n", scan_pass++);

        for (size_t scl_i = 0; scl_i < sizeof(pi_ai_mic_i2c_scan_ports) / sizeof(pi_ai_mic_i2c_scan_ports[0]); scl_i++) {
            for (size_t sda_i = 0; sda_i < sizeof(pi_ai_mic_i2c_scan_ports) / sizeof(pi_ai_mic_i2c_scan_ports[0]); sda_i++) {
                if (scl_i == sda_i) {
                    continue;
                }

                const pi_ai_mic_scan_port_t *scl = &pi_ai_mic_i2c_scan_ports[scl_i];
                const pi_ai_mic_scan_port_t *sda = &pi_ai_mic_i2c_scan_ports[sda_i];

                port_enable(scl->port);
                port_enable(sda->port);
                pi_ai_mic_i2c_release(scl->port);
                pi_ai_mic_i2c_release(sda->port);
                hwtimer_delay(timer, PI_AI_MIC_I2C_SCAN_PAUSE_TICKS);

                uint32_t idle_scl = pi_ai_mic_i2c_read(scl->port);
                uint32_t idle_sda = pi_ai_mic_i2c_read(sda->port);

                pi_ai_mic_i2c_recover(scl->port, sda->port, timer);

                int ack_pcal = pi_ai_mic_i2c_probe_address(scl->port, sda->port, timer, 0x20);
                int ack_dac = pi_ai_mic_i2c_probe_address(scl->port, sda->port, timer, 0x18);

                if (ack_pcal || ack_dac) {
                    printf("PI_AI_MIC I2C FOUND SCL=XS1_PORT_%s SDA=XS1_PORT_%s idle=%u/%u PCAL6408A_0x20=%d DAC3101_0x18=%d\n",
                           scl->name,
                           sda->name,
                           idle_scl,
                           idle_sda,
                           ack_pcal,
                           ack_dac);
                } else {
                    printf("PI_AI_MIC I2C scan SCL=%s SDA=%s idle=%u/%u nack\n",
                           scl->name,
                           sda->name,
                           idle_scl,
                           idle_sda);
                }

                port_disable(sda->port);
                port_disable(scl->port);

                led_counter++;
                if (led_counter >= PI_AI_MIC_I2C_SCAN_LED_TOGGLE_CYCLES) {
                    led_counter = 0;
                    led_state ^= 1;
                    pi_ai_mic_scan_set_led(gpo, led_state);
                }
            }
        }
    }
}
#endif

#if PI_AI_MIC_I2C_PIN_LOCATOR_TEST_ENABLE && PI_AI_MIC_I2C_PIN_LOCATOR_TILE0_ENABLE
static void pi_ai_mic_tile0_i2c_pin_locator_test(void)
{
    port_t gpo = GPO_TILE_0_PORT_8C;
    pi_ai_mic_tile0_scan_led_t led = {
        .gpo = gpo,
        .led_state = 0,
        .led_counter = 0,
    };

    port_enable(gpo);
    port_out(gpo, PI_AI_MIC_GPO_ALL_SAFE_HIGH);

    pi_ai_mic_i2c_pin_locator_run(
            "TILE0",
            pi_ai_mic_deep_i2c_tile0_lines,
            sizeof(pi_ai_mic_deep_i2c_tile0_lines) / sizeof(pi_ai_mic_deep_i2c_tile0_lines[0]),
            pi_ai_mic_tile0_deep_scan_tick,
            &led);
}
#endif

#if PI_AI_MIC_I2C_ELECTRICAL_TEST_ENABLE && PI_AI_MIC_I2C_ELECTRICAL_TILE0_ENABLE
static void pi_ai_mic_tile0_i2c_electrical_test(void)
{
    port_t gpo = GPO_TILE_0_PORT_8C;
    pi_ai_mic_tile0_scan_led_t led = {
        .gpo = gpo,
        .led_state = 0,
        .led_counter = 0,
    };

    port_enable(gpo);
    port_out(gpo, PI_AI_MIC_GPO_ALL_SAFE_HIGH);

    pi_ai_mic_i2c_electrical_test_run(
            "TILE0",
            pi_ai_mic_deep_i2c_tile0_lines,
            sizeof(pi_ai_mic_deep_i2c_tile0_lines) / sizeof(pi_ai_mic_deep_i2c_tile0_lines[0]),
            pi_ai_mic_tile0_deep_scan_tick,
            &led);
}
#endif

#if PI_AI_MIC_I2C_FORCED_PROBE_TEST_ENABLE && PI_AI_MIC_I2C_FORCED_PROBE_TILE0_ENABLE
static void pi_ai_mic_tile0_i2c_forced_probe_test(void)
{
    port_t gpo = GPO_TILE_0_PORT_8C;
    pi_ai_mic_tile0_scan_led_t led = {
        .gpo = gpo,
        .led_state = 0,
        .led_counter = 0,
    };

    port_enable(gpo);
    port_write_control_word(gpo, XS1_SETC_DRIVE_DRIVE);
    pi_ai_mic_scan_set_led(gpo, led.led_state);

    pi_ai_mic_i2c_forced_probe_run(
            "TILE0",
            pi_ai_mic_deep_i2c_tile0_lines,
            sizeof(pi_ai_mic_deep_i2c_tile0_lines) / sizeof(pi_ai_mic_deep_i2c_tile0_lines[0]),
            pi_ai_mic_tile0_deep_scan_tick,
            &led);
}
#endif

#if ( 0 < DFU_CONTROL ) && appconfUSB_ENABLED
/// @brief function to send to tile 1 the information to configure the DFU image context
static void send_dfu_info()
{
    // Send all the necessary information of the DFU Image Context
    rtos_dfu_image_t * dfu_image_ctx = get_dfu_image_ctx();
    rtos_intertile_t * intertile_ctx = get_intertile_ctx();
    uint32_t value = dfu_image_ctx->factory_image_ctx.startAddress;
    rtos_intertile_tx(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, &value, sizeof(value));
    value = dfu_image_ctx->factory_image_ctx.size;
    rtos_intertile_tx(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, &value, sizeof(value));
    value = dfu_image_ctx->factory_image_ctx.version;
    rtos_intertile_tx(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, &value, sizeof(value));
    value = dfu_image_ctx->upgrade_image_ctx.startAddress;
    rtos_intertile_tx(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, &value, sizeof(value));
    value = dfu_image_ctx->upgrade_image_ctx.size;
    rtos_intertile_tx(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, &value, sizeof(value));
    value = dfu_image_ctx->upgrade_image_ctx.version;
    rtos_intertile_tx(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, &value, sizeof(value));
    value = dfu_image_ctx->data_partition_base_addr;
    rtos_intertile_tx(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, &value, sizeof(value));
}
#endif
/// @brief this tiles rtos entry
static void rtos_app(void* args)
{
    debug_printf("in os on tile[%d]\n", THIS_XCORE_TILE);

    servicer_t** servicers = (servicer_t**)args;

    platform_start();

#if PI_AI_MIC_PRE_RTOS_SPEAKER_HW_INIT && PI_AI_MIC_LATE_SPEAKER_HW_INIT
    xTaskCreate(
        pi_ai_mic_late_speaker_hw_init_task,
        "PI_AI_MIC late DAC",
        RTOS_THREAD_STACK_SIZE(pi_ai_mic_late_speaker_hw_init_task),
        NULL,
        appconfTEST_TASK_PRIORITY,
        NULL
    );
#endif

#if PI_AI_MIC_LED_HEARTBEAT_ENABLE
    xTaskCreate(
        pi_ai_mic_led_heartbeat_task,
        "PI_AI_MIC LED",
        RTOS_THREAD_STACK_SIZE(pi_ai_mic_led_heartbeat_task),
        NULL,
        appconfSTARTUP_TASK_PRIORITY,
        NULL
    );
#endif

#if PI_AI_MIC_DETECT_LED_MONITOR_ENABLE && PI_AI_MIC_DETECT_LED_MONITOR_DIRECT_GPO_ENABLE
    xTaskCreate(
        pi_ai_mic_detect_led_monitor_task,
        "PI_AI_MIC detect",
        RTOS_THREAD_STACK_SIZE(pi_ai_mic_detect_led_monitor_task),
        NULL,
        appconfSTARTUP_TASK_PRIORITY,
        NULL
    );
#endif

#if ( 0 < DFU_CONTROL ) && appconfUSB_ENABLED
    send_dfu_info();
#endif

#if !(PI_AI_MIC_DETECT_LED_MONITOR_ENABLE && \
      PI_AI_MIC_DETECT_LED_MONITOR_DIRECT_GPO_ENABLE && \
      !PI_AI_MIC_DETECT_LED_MONITOR_GPO_SERVICER_ENABLE)
    xTaskCreate(
        gpo_servicer,
        "GPO task",
        RTOS_THREAD_STACK_SIZE(gpo_servicer),
        servicers[GPO_SERVICER_INDEX],
        appconfTEST_TASK_PRIORITY,
        NULL
    );
#endif

    xTaskCreate(
        servicer_task,
        "PP servicer",
        RTOS_THREAD_STACK_SIZE(servicer_task),
        servicers[PP_SERVICER_INDEX],
        appconfTEST_TASK_PRIORITY,
        NULL
    );

#if ((DFU_CONTROL > 0) && (!appconfUSB_ENABLED))
    xTaskCreate(
        dfu_servicer,
        "DFU servicer",
        RTOS_THREAD_STACK_SIZE(servicer_task),
        servicers[DFU_SERVICER_INDEX],
        appconfTEST_TASK_PRIORITY,
        NULL
    );
#endif

    xTaskCreate(
        servicer_task,
        "USB Buffer servicer",
        RTOS_THREAD_STACK_SIZE(servicer_task),
        servicers[USB_BUFFER_SERVICER_INDEX],
        appconfTEST_TASK_PRIORITY,
        NULL
    );

    xTaskCreate(
        supply_pll_status_task,
        "supply_pll_status_task",
        RTOS_THREAD_STACK_SIZE(supply_pll_status_task),
        NULL,
        appconfTEST_TASK_PRIORITY,
        NULL
    );

    /* When we spin up the servicers, they block the intertile context. This
     * means that we need to spin up the endpoint proxy afterwards, so that
     * when USB starts we are ready to accept communication straight away */

#if appconfUSB_ENABLED
    ep_proxy_start(configMAX_PRIORITIES-1);
#endif

    // done
    vTaskDelete(NULL);
}

DECLARE_JOB(wrap_XUD_Main, (chanend_t, chanend_t));
void wrap_XUD_Main(chanend_t c_ep0_out_a, chanend_t c_sof)
{
    XUD_EpType epTypeTableOut[RTOS_USB_ENDPOINT_COUNT_MAX];
    XUD_EpType epTypeTableIn[RTOS_USB_ENDPOINT_COUNT_MAX];
    channel_t channel_ep_out[RTOS_USB_ENDPOINT_COUNT_MAX];
    channel_t channel_ep_in[RTOS_USB_ENDPOINT_COUNT_MAX];

    if(get_core_burn_status() != 0){
        SET_FAST_MODE();
    }
    // The order in which things happen
    // 1. offtile ep0 starts and finishes registering servicers on the device_control ctx it hosts.
    // 2. It indicates to ep0 proxy over chan_ep0_proxy that ep0 init is done.
    // 3. ep0 proxy indicates to XUD_Main over chan_ep0_out that XUD_Main can be started.
    int num_out_endpoints = chan_in_word(c_ep0_out_a);
    int num_in_endpoints = chan_in_word(c_ep0_out_a);
    chan_in_buf_byte(c_ep0_out_a, (uint8_t *)&epTypeTableOut[0], num_out_endpoints*sizeof(XUD_EpType));
    chan_in_buf_byte(c_ep0_out_a, (uint8_t *)&epTypeTableIn[0], num_in_endpoints*sizeof(XUD_EpType));
    chan_in_buf_byte(c_ep0_out_a, (uint8_t *)&channel_ep_out[0], num_out_endpoints*sizeof(channel_t));
    chan_in_buf_byte(c_ep0_out_a, (uint8_t *)&channel_ep_in[0], num_in_endpoints*sizeof(channel_t));
    XUD_PwrConfig pwrConfig = chan_in_word(c_ep0_out_a);
    XUD_BusSpeed_t desiredSpeed = chan_in_word(c_ep0_out_a);

    chanend_t c_ep_out[RTOS_USB_ENDPOINT_COUNT_MAX];
    chanend_t c_ep_in[RTOS_USB_ENDPOINT_COUNT_MAX];

    // Package chanends into the format XUD expects them
    for (int i = 0; i < num_out_endpoints; i++)
    {
        if(epTypeTableOut[i] != XUD_EPTYPE_DIS)
        {
            c_ep_out[i] = channel_ep_out[i].end_a;
        }
    }
    for (int i = 0; i < num_in_endpoints; i++)
    {
        if(epTypeTableIn[i] != XUD_EPTYPE_DIS)
        {
            c_ep_in[i] = channel_ep_in[i].end_a;
        }
    }

    hwtimer_realloc_xc_timer();

    XUD_Main(c_ep_out,
            num_out_endpoints,
            c_ep_in,
            num_in_endpoints,
            c_sof,
            (XUD_EpType *) epTypeTableOut,
            (XUD_EpType *) epTypeTableIn,
            desiredSpeed,
            pwrConfig);

    hwtimer_free_xc_timer();
}

/// @brief this tiles entry point
void main_tile0(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3)
{
#if PI_AI_MIC_I2C_ELECTRICAL_TEST_ENABLE && PI_AI_MIC_I2C_ELECTRICAL_TILE0_ENABLE
    (void)c0;
    (void)c1;
    (void)c2;
    (void)c3;
#if defined(__XS3A__) && PI_AI_MIC_XSCOPE_ENABLE
    xscope_config_io(XSCOPE_IO_BASIC);
#endif
    pi_ai_mic_tile0_i2c_electrical_test();
#elif PI_AI_MIC_I2C_FORCED_PROBE_TEST_ENABLE && PI_AI_MIC_I2C_FORCED_PROBE_TILE0_ENABLE
    (void)c0;
    (void)c1;
    (void)c2;
    (void)c3;
#if defined(__XS3A__) && PI_AI_MIC_XSCOPE_ENABLE
    xscope_config_io(XSCOPE_IO_BASIC);
#endif
    pi_ai_mic_tile0_i2c_forced_probe_test();
#elif PI_AI_MIC_I2C_PIN_LOCATOR_TEST_ENABLE && PI_AI_MIC_I2C_PIN_LOCATOR_TILE0_ENABLE
    (void)c0;
    (void)c1;
    (void)c2;
    (void)c3;
#if defined(__XS3A__) && PI_AI_MIC_XSCOPE_ENABLE
    xscope_config_io(XSCOPE_IO_BASIC);
#endif
    pi_ai_mic_tile0_i2c_pin_locator_test();
#elif PI_AI_MIC_I2C_PORT_SCAN_TEST_ENABLE && PI_AI_MIC_I2C_PORT_SCAN_TILE0_ENABLE
    (void)c0;
    (void)c1;
    (void)c2;
    (void)c3;
#if defined(__XS3A__) && PI_AI_MIC_XSCOPE_ENABLE
    xscope_config_io(XSCOPE_IO_BASIC);
#endif
    pi_ai_mic_tile0_i2c_port_scan_test();
#elif PI_AI_MIC_I2C_WAVEFORM_TEST_ENABLE
    (void)c0;
    (void)c1;
    (void)c2;
    (void)c3;
#if defined(__XS3A__) && PI_AI_MIC_XSCOPE_ENABLE
    xscope_config_io(XSCOPE_IO_BASIC);
#endif
    pi_ai_mic_tile0_waveform_test();
#endif

#if defined(__XS3A__) && PI_AI_MIC_XSCOPE_ENABLE
    xscope_config_io(XSCOPE_IO_BASIC);
#endif

#if PI_AI_MIC_PRE_RTOS_SPEAKER_HW_INIT
    pi_ai_mic_pre_rtos_speaker_hw_init();
#endif

    (void)c3;
    servicer_t* tile_0_servicers[NUM_TILE_0_SERVICERS]; // Array of pointers
    // Initialise control related things
    servicer_t servicer_pp;
    servicer_t servicer_gpo;
    servicer_t servicer_usb_buffer;
    tile_0_servicers[PP_SERVICER_INDEX] = &servicer_pp;
    tile_0_servicers[GPO_SERVICER_INDEX] = &servicer_gpo;
    tile_0_servicers[USB_BUFFER_SERVICER_INDEX] = &servicer_usb_buffer;

    pp_servicer_init(tile_0_servicers[PP_SERVICER_INDEX]);
    gpo_servicer_init(tile_0_servicers[GPO_SERVICER_INDEX]);
    usb_buffer_servicer_init(tile_0_servicers[USB_BUFFER_SERVICER_INDEX]);

#if (DFU_CONTROL > 0) && (!appconfUSB_ENABLED)
    servicer_t servicer_dfu;
    tile_0_servicers[DFU_SERVICER_INDEX] = &servicer_dfu;
    dfu_servicer_init(tile_0_servicers[DFU_SERVICER_INDEX]);
#endif

    // Open new cross tile paths tile0 c0 <-> tile 1 c1 using existing channel c1 <-> c0
    c0 = chanend_alloc();
    chanend_set_dest(c0, chan_in_word(c1));
    chan_out_word(c1, c0);

    // Now share info with other tile
    chan_out_byte(c0, is_spi_slave_boot()); // Send tile[0] boot info over to tile[1]
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    /* Freeze the pre-RTOS P6 decision on tile 1 before BeClear starts. */
    const uint8_t mic_geometry_boot =
        (pi_ai_mic_boot_detect_valid ? 0x01u : 0x00u) |
        (pi_ai_mic_boot_detect_mic_square ? 0x02u : 0x00u);
    chan_out_byte(c0, mic_geometry_boot);
#endif
    chanend_t c_mic_to_audio = c0;
    set_core_burn_status(chan_in_byte(c0)); // Receive core burn status from tile[1] and set for later use
    set_usb_bit_depth(chan_in_byte(c0), chan_in_byte(c0)); // Receive USB bit depths from tile[1] for local use

    // Open new cross tile paths tile0 c2 <-> tile 1 c2 using existing channel c1 <-> c0
    c2 = chanend_alloc();
    chanend_set_dest(c2, chan_in_word(c1));
    chan_out_word(c1, c2);
    chanend_t c_aec_to_pp = c2;

    // Open new cross tile paths tile0 chan_usb_to_i2s <-> tile 1 chan_usb_to_i2s using existing channel c0 <-> c1
    chanend_t chan_usb_to_i2s = chanend_alloc();
    chanend_set_dest(chan_usb_to_i2s, chan_in_word(c1));
    chan_out_word(c1, chan_usb_to_i2s);

    // Open new cross tile paths tile0 c_ep0_proxy <-> tile 1 c_ep0_proxy using existing channel c0 <-> c1
    chanend_t c_ep0_proxy;
    c_ep0_proxy = chanend_alloc();
    chanend_set_dest(c_ep0_proxy, chan_in_word(c1));
    chan_out_word(c1, c_ep0_proxy);

    chanend_t c_ep_hid_proxy;
    c_ep_hid_proxy = chanend_alloc();
    chanend_set_dest(c_ep_hid_proxy, chan_in_word(c1));
    chan_out_word(c1, c_ep_hid_proxy);

    chanend_t c_ep_proxy_xfer_complete;
    c_ep_proxy_xfer_complete = chanend_alloc();
    chanend_set_dest(c_ep_proxy_xfer_complete, chan_in_word(c1));
    chan_out_word(c1, c_ep_proxy_xfer_complete);

#if appconfUSB_ENABLED
    // Allocate some channels for USB. The rest are allocated in usb_proxy.c

    channel_t channel_ep0_out = chan_alloc();
    channel_t channel_ep_audio_out = chan_alloc();
    channel_t channel_ep0_in = chan_alloc();
    channel_t channel_ep_audio_in = chan_alloc();

    channel_t channel_sof = chan_alloc();

    // Can't seem to send more than 4 args so send ref to struct http://bugzilla/show_bug.cgi?id=18745
    usb_buffer_args_t usb_task_args = {
        .chan_ep_audio_out = channel_ep_audio_out.end_b,
        .chan_ep_audio_in = channel_ep_audio_in.end_b,
        .chan_usb_to_i2s = chan_usb_to_i2s,
        .chan_sof = channel_sof.end_b
    };

     ep_proxy_init(channel_ep0_out,
                    channel_ep0_in,
                    channel_ep_audio_out,
                    channel_ep_audio_in,
                    c_ep0_proxy,
                    c_ep_hid_proxy,
                    c_ep_proxy_xfer_complete);

     init_vol_muls(); // Initialise the USB volume multipliers. This needs to be done before XUD is started
#endif

    PAR_JOBS(
        PJOB(INTERRUPT_PERMITTED(pp_task), (&c_aec_to_pp, &servicer_pp.res_info[PP_RESID - PP_SERVICER_RESID].control_pkt_queue)),               // Spawns three high priority logical cores
        PJOB(mic_array_task, (c_mic_to_audio)),
#if (appconfUSB_ENABLED != 0)
        PJOB(usb_buffer, (&usb_task_args)),
        PJOB(wrap_XUD_Main, (channel_ep0_out.end_a, channel_sof.end_a)),
#endif
        PJOB(tile_common_start_rtos_app, (c1, rtos_app, RTOS_THREAD_STACK_SIZE(rtos_app), &tile_0_servicers))
    );
}
