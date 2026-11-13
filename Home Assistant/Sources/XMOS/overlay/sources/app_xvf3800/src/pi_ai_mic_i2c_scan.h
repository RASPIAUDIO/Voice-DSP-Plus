// PI_AI_MIC bring-up helper: brute-force bit-banged I2C probe.
// This is intentionally self-contained so it can run before the RTOS/audio stack.
#ifndef PI_AI_MIC_I2C_SCAN_H_
#define PI_AI_MIC_I2C_SCAN_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "xcore/hwtimer.h"
#include "xcore/port.h"

#define PI_AI_MIC_DEEP_I2C_SCAN_HALF_PERIOD_TICKS  500u
#define PI_AI_MIC_DEEP_I2C_SCAN_SETTLE_TICKS       20000u
#define PI_AI_MIC_FORCED_ACK_RELEASE_TICKS         20u
#define PI_AI_MIC_DEEP_I2C_SCAN_MAX_PORTS          2u
#define PI_AI_MIC_PIN_LOCATOR_HALF_PERIOD_TICKS    5000000u
#define PI_AI_MIC_PIN_LOCATOR_HALF_CYCLES          80u
#define PI_AI_MIC_ELECTRICAL_MAX_LINES             64u

typedef void (*pi_ai_mic_deep_i2c_scan_tick_cb_t)(void *arg);

typedef struct {
    const char *name;
    port_t port;
    uint32_t mask;
    uint32_t released_value;
    uint16_t physical_id;
} pi_ai_mic_deep_i2c_line_t;

typedef struct {
    port_t port;
    uint32_t value;
} pi_ai_mic_deep_i2c_port_state_t;

typedef struct {
    pi_ai_mic_deep_i2c_port_state_t states[PI_AI_MIC_DEEP_I2C_SCAN_MAX_PORTS];
    size_t state_count;
} pi_ai_mic_deep_i2c_pair_t;

static const uint8_t pi_ai_mic_deep_i2c_probe_addresses[] = {
    0x18, 0x19,             // DAC3101 common addresses
    0x20, 0x21, 0x22, 0x23, // PCAL6408A family address range
    0x24, 0x25, 0x26, 0x27,
    0x2C,                   // XVF3800 host-control address used by common examples
};

static const uint8_t pi_ai_mic_i2c_control_addresses[] = {
    0x3E, 0x5A, 0x7E,       // Expected absent; used to reject floating/all-ACK buses.
};

static const pi_ai_mic_deep_i2c_line_t pi_ai_mic_deep_i2c_tile0_lines[] = {
    { "T0:X0D37/1N", XS1_PORT_1N, 1u, 1u, 37u },
    { "T0:X0D38/1O", XS1_PORT_1O, 1u, 1u, 38u },
    { "T0:X0D37/8D1", XS1_PORT_8D, 1u << 1, 0xFFu, 37u },
    { "T0:X0D38/8D2", XS1_PORT_8D, 1u << 2, 0xFFu, 38u },
    { "T0:X0D37/16B9", XS1_PORT_16B, 1u << 9, 0xFFFFu, 37u },
    { "T0:X0D38/16B10", XS1_PORT_16B, 1u << 10, 0xFFFFu, 38u },
    { "T0:X0D36/1M", XS1_PORT_1M, 1u, 1u, 36u },
    { "T0:X0D39/1P", XS1_PORT_1P, 1u, 1u, 39u },
    { "T0:X0D36/8D0", XS1_PORT_8D, 1u << 0, 0xFFu, 36u },
    { "T0:X0D39/8D3", XS1_PORT_8D, 1u << 3, 0xFFu, 39u },
    { "T0:X0D40/8D4", XS1_PORT_8D, 1u << 4, 0xFFu, 40u },
    { "T0:X0D41/8D5", XS1_PORT_8D, 1u << 5, 0xFFu, 41u },
    { "T0:X0D42/8D6", XS1_PORT_8D, 1u << 6, 0xFFu, 42u },
    { "T0:X0D43/8D7", XS1_PORT_8D, 1u << 7, 0xFFu, 43u },
    { "T0:X0D36/16B8", XS1_PORT_16B, 1u << 8, 0xFFFFu, 36u },
    { "T0:X0D39/16B11", XS1_PORT_16B, 1u << 11, 0xFFFFu, 39u },
    { "T0:X0D40/16B12", XS1_PORT_16B, 1u << 12, 0xFFFFu, 40u },
    { "T0:X0D41/16B13", XS1_PORT_16B, 1u << 13, 0xFFFFu, 41u },
    { "T0:X0D42/16B14", XS1_PORT_16B, 1u << 14, 0xFFFFu, 42u },
    { "T0:X0D43/16B15", XS1_PORT_16B, 1u << 15, 0xFFFFu, 43u },
    { "T0:X0D04/4B0", XS1_PORT_4B, 1u << 0, 0x0Fu, 4u },
    { "T0:X0D05/4B1", XS1_PORT_4B, 1u << 1, 0x0Fu, 5u },
    { "T0:X0D06/4B2", XS1_PORT_4B, 1u << 2, 0x0Fu, 6u },
    { "T0:X0D07/4B3", XS1_PORT_4B, 1u << 3, 0x0Fu, 7u },
    { "T0:X0D04/8A2", XS1_PORT_8A, 1u << 2, 0xFFu, 4u },
    { "T0:X0D05/8A3", XS1_PORT_8A, 1u << 3, 0xFFu, 5u },
    { "T0:X0D06/8A4", XS1_PORT_8A, 1u << 4, 0xFFu, 6u },
    { "T0:X0D07/8A5", XS1_PORT_8A, 1u << 5, 0xFFu, 7u },
    { "T0:X0D29/4F1", XS1_PORT_4F, 1u << 1, 0x0Fu, 29u },
    { "T0:X0D30/4F2", XS1_PORT_4F, 1u << 2, 0x0Fu, 30u },
    { "T0:X0D31/4F3", XS1_PORT_4F, 1u << 3, 0x0Fu, 31u },
    { "T0:X0D32/4E2", XS1_PORT_4E, 1u << 2, 0x0Fu, 32u },
    { "T0:X0D33/4E3", XS1_PORT_4E, 1u << 3, 0x0Fu, 33u },
    { "T0:X0D00/1A", XS1_PORT_1A, 1u, 1u, 0u },
    { "T0:X0D01/1B", XS1_PORT_1B, 1u, 1u, 1u },
    { "T0:X0D10/1C", XS1_PORT_1C, 1u, 1u, 10u },
    { "T0:X0D11/1D", XS1_PORT_1D, 1u, 1u, 11u },
    { "T0:X0D35/1L", XS1_PORT_1L, 1u, 1u, 35u },
};

static const pi_ai_mic_deep_i2c_line_t pi_ai_mic_deep_i2c_tile1_lines[] = {
    { "T1:X1D00/1A", XS1_PORT_1A, 1u, 1u, 1000u },
    { "T1:X1D01/1B", XS1_PORT_1B, 1u, 1u, 1001u },
    { "T1:X1D10/1C", XS1_PORT_1C, 1u, 1u, 1010u },
    { "T1:X1D11/1D", XS1_PORT_1D, 1u, 1u, 1011u },
    { "T1:1E", XS1_PORT_1E, 1u, 1u, 1012u },
    { "T1:X1D13/1F", XS1_PORT_1F, 1u, 1u, 1013u },
    { "T1:X1D22/1G", XS1_PORT_1G, 1u, 1u, 1022u },
    { "T1:1H", XS1_PORT_1H, 1u, 1u, 1023u },
    { "T1:1I", XS1_PORT_1I, 1u, 1u, 1024u },
    { "T1:1J", XS1_PORT_1J, 1u, 1u, 1025u },
    { "T1:X1D34/1K", XS1_PORT_1K, 1u, 1u, 1034u },
    { "T1:1L", XS1_PORT_1L, 1u, 1u, 1026u },
    { "T1:1M", XS1_PORT_1M, 1u, 1u, 1027u },
    { "T1:1N", XS1_PORT_1N, 1u, 1u, 1028u },
    { "T1:1O", XS1_PORT_1O, 1u, 1u, 1029u },
    { "T1:1P", XS1_PORT_1P, 1u, 1u, 1030u },
    { "T1:X1D09/4A3", XS1_PORT_4A, 1u << 3, 0x0Fu, 1009u },
    { "T1:X1D16/4D0", XS1_PORT_4D, 1u << 0, 0x0Fu, 1016u },
    { "T1:X1D17/4D1", XS1_PORT_4D, 1u << 1, 0x0Fu, 1017u },
    { "T1:X1D18/4D2", XS1_PORT_4D, 1u << 2, 0x0Fu, 1018u },
    { "T1:X1D19/4D3", XS1_PORT_4D, 1u << 3, 0x0Fu, 1019u },
    { "T1:X1D16/8B2", XS1_PORT_8B, 1u << 2, 0xFFu, 1016u },
    { "T1:X1D17/8B3", XS1_PORT_8B, 1u << 3, 0xFFu, 1017u },
    { "T1:X1D18/8B4", XS1_PORT_8B, 1u << 4, 0xFFu, 1018u },
    { "T1:X1D19/8B5", XS1_PORT_8B, 1u << 5, 0xFFu, 1019u },
};

static void pi_ai_mic_deep_i2c_delay(hwtimer_t timer)
{
    hwtimer_delay(timer, PI_AI_MIC_DEEP_I2C_SCAN_HALF_PERIOD_TICKS);
}

static pi_ai_mic_deep_i2c_port_state_t *pi_ai_mic_deep_i2c_find_state(
        pi_ai_mic_deep_i2c_pair_t *pair,
        port_t port)
{
    for (size_t i = 0; i < pair->state_count; i++) {
        if (pair->states[i].port == port) {
            return &pair->states[i];
        }
    }

    return NULL;
}

static pi_ai_mic_deep_i2c_port_state_t *pi_ai_mic_deep_i2c_add_state(
        pi_ai_mic_deep_i2c_pair_t *pair,
        port_t port,
        uint32_t released_value)
{
    pi_ai_mic_deep_i2c_port_state_t *state = pi_ai_mic_deep_i2c_find_state(pair, port);
    if (state != NULL) {
        state->value |= released_value;
        return state;
    }

    if (pair->state_count >= PI_AI_MIC_DEEP_I2C_SCAN_MAX_PORTS) {
        return NULL;
    }

    state = &pair->states[pair->state_count++];
    state->port = port;
    state->value = released_value;
    return state;
}

static void pi_ai_mic_deep_i2c_pair_init(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *scl,
        const pi_ai_mic_deep_i2c_line_t *sda)
{
    pair->state_count = 0;

    (void)pi_ai_mic_deep_i2c_add_state(pair, scl->port, scl->released_value);
    (void)pi_ai_mic_deep_i2c_add_state(pair, sda->port, sda->released_value);

    for (size_t i = 0; i < pair->state_count; i++) {
        port_enable(pair->states[i].port);
        port_write_control_word(pair->states[i].port, XS1_SETC_DRIVE_PULL_UP);
        port_out(pair->states[i].port, pair->states[i].value);
        port_sync(pair->states[i].port);
    }
}

static void pi_ai_mic_deep_i2c_pair_deinit(pi_ai_mic_deep_i2c_pair_t *pair)
{
    for (size_t i = 0; i < pair->state_count; i++) {
        port_write_control_word(pair->states[i].port, XS1_SETC_DRIVE_PULL_UP);
        port_out(pair->states[i].port, pair->states[i].value);
        port_sync(pair->states[i].port);
        port_disable(pair->states[i].port);
    }
}

static void pi_ai_mic_deep_i2c_release(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *line)
{
    pi_ai_mic_deep_i2c_port_state_t *state = pi_ai_mic_deep_i2c_find_state(pair, line->port);
    if (state == NULL) {
        return;
    }

    state->value |= line->mask;
    port_out(line->port, state->value);
    port_sync(line->port);
}

static void pi_ai_mic_deep_i2c_drive_low(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *line)
{
    pi_ai_mic_deep_i2c_port_state_t *state = pi_ai_mic_deep_i2c_find_state(pair, line->port);
    if (state == NULL) {
        return;
    }

    state->value &= ~line->mask;
    port_out(line->port, state->value);
    port_sync(line->port);
}

static uint32_t pi_ai_mic_deep_i2c_read(const pi_ai_mic_deep_i2c_line_t *line)
{
    port_sync(line->port);
    return (port_peek(line->port) & line->mask) ? 1u : 0u;
}

static int pi_ai_mic_deep_i2c_line_is_single_bit(
        const pi_ai_mic_deep_i2c_line_t *line)
{
    return line->mask == 1u && line->released_value == 1u;
}

static void pi_ai_mic_i2c_pin_locator_run(
        const char *tag,
        const pi_ai_mic_deep_i2c_line_t *lines,
        size_t line_count,
        pi_ai_mic_deep_i2c_scan_tick_cb_t tick_cb,
        void *tick_arg)
{
    hwtimer_t timer = hwtimer_alloc();
    unsigned long pass = 0;

    printf("PI_AI_MIC %s I2C pin locator start: %lu candidates, one candidate every ~4s\n",
           tag,
           (unsigned long)line_count);
    printf("PI_AI_MIC locator: 1-bit ports use push-pull; multi-bit aliases use pull-up/open-drain style\n");

    for (;;) {
        printf("PI_AI_MIC %s I2C pin locator pass %lu begin\n", tag, pass);

        for (size_t i = 0; i < line_count; i++) {
            const pi_ai_mic_deep_i2c_line_t *line = &lines[i];
            int push_pull = pi_ai_mic_deep_i2c_line_is_single_bit(line);
            uint32_t high_value = line->released_value;
            uint32_t low_value = line->released_value & ~line->mask;

            port_enable(line->port);
            port_out(line->port, high_value);
            port_write_control_word(line->port,
                                    push_pull ? XS1_SETC_DRIVE_DRIVE : XS1_SETC_DRIVE_PULL_UP);
            hwtimer_delay(timer, PI_AI_MIC_DEEP_I2C_SCAN_SETTLE_TICKS);

            printf("PI_AI_MIC LOCATOR tag=%s line=%s phys=%u port_mode=%s idle=%lu toggling\n",
                   tag,
                   line->name,
                   (unsigned)line->physical_id,
                   push_pull ? "push_pull" : "pull_up",
                   (unsigned long)pi_ai_mic_deep_i2c_read(line));

            for (unsigned half = 0; half < PI_AI_MIC_PIN_LOCATOR_HALF_CYCLES; half++) {
                port_out(line->port, (half & 1u) ? high_value : low_value);
                hwtimer_delay(timer, PI_AI_MIC_PIN_LOCATOR_HALF_PERIOD_TICKS);

                if (tick_cb != NULL) {
                    tick_cb(tick_arg);
                }
            }

            port_write_control_word(line->port, XS1_SETC_DRIVE_PULL_UP);
            port_out(line->port, high_value);
            port_disable(line->port);
        }

        printf("PI_AI_MIC %s I2C pin locator pass %lu done\n", tag, pass);
        pass++;
    }
}

static void pi_ai_mic_deep_i2c_stop(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *scl,
        const pi_ai_mic_deep_i2c_line_t *sda,
        hwtimer_t timer)
{
    pi_ai_mic_deep_i2c_drive_low(pair, sda);
    pi_ai_mic_deep_i2c_delay(timer);
    pi_ai_mic_deep_i2c_release(pair, scl);
    pi_ai_mic_deep_i2c_delay(timer);
    pi_ai_mic_deep_i2c_release(pair, sda);
    pi_ai_mic_deep_i2c_delay(timer);
}

static void pi_ai_mic_deep_i2c_recover(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *scl,
        const pi_ai_mic_deep_i2c_line_t *sda,
        hwtimer_t timer)
{
    pi_ai_mic_deep_i2c_release(pair, sda);

    for (unsigned i = 0; i < 9; i++) {
        pi_ai_mic_deep_i2c_drive_low(pair, scl);
        pi_ai_mic_deep_i2c_delay(timer);
        pi_ai_mic_deep_i2c_release(pair, scl);
        pi_ai_mic_deep_i2c_delay(timer);
    }

    pi_ai_mic_deep_i2c_stop(pair, scl, sda, timer);
}

static void pi_ai_mic_deep_i2c_start(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *scl,
        const pi_ai_mic_deep_i2c_line_t *sda,
        hwtimer_t timer)
{
    pi_ai_mic_deep_i2c_release(pair, sda);
    pi_ai_mic_deep_i2c_release(pair, scl);
    pi_ai_mic_deep_i2c_delay(timer);
    pi_ai_mic_deep_i2c_drive_low(pair, sda);
    pi_ai_mic_deep_i2c_delay(timer);
    pi_ai_mic_deep_i2c_drive_low(pair, scl);
    pi_ai_mic_deep_i2c_delay(timer);
}

static int pi_ai_mic_deep_i2c_write_byte(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *scl,
        const pi_ai_mic_deep_i2c_line_t *sda,
        hwtimer_t timer,
        uint8_t byte)
{
    for (int bit = 7; bit >= 0; bit--) {
        if ((byte >> bit) & 1u) {
            pi_ai_mic_deep_i2c_release(pair, sda);
        } else {
            pi_ai_mic_deep_i2c_drive_low(pair, sda);
        }

        pi_ai_mic_deep_i2c_delay(timer);
        pi_ai_mic_deep_i2c_release(pair, scl);
        pi_ai_mic_deep_i2c_delay(timer);
        pi_ai_mic_deep_i2c_drive_low(pair, scl);
        pi_ai_mic_deep_i2c_delay(timer);
    }

    pi_ai_mic_deep_i2c_release(pair, sda);
    pi_ai_mic_deep_i2c_delay(timer);
    pi_ai_mic_deep_i2c_release(pair, scl);
    pi_ai_mic_deep_i2c_delay(timer);
    int ack = pi_ai_mic_deep_i2c_read(sda) == 0;
    pi_ai_mic_deep_i2c_drive_low(pair, scl);
    pi_ai_mic_deep_i2c_delay(timer);

    return ack;
}

static int pi_ai_mic_deep_i2c_probe_address(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *scl,
        const pi_ai_mic_deep_i2c_line_t *sda,
        hwtimer_t timer,
        uint8_t address)
{
    pi_ai_mic_deep_i2c_start(pair, scl, sda, timer);
    int ack = pi_ai_mic_deep_i2c_write_byte(pair, scl, sda, timer, (uint8_t)(address << 1));
    pi_ai_mic_deep_i2c_stop(pair, scl, sda, timer);
    return ack;
}

static int pi_ai_mic_deep_i2c_is_expected_pair(
        const pi_ai_mic_deep_i2c_line_t *scl,
        const pi_ai_mic_deep_i2c_line_t *sda)
{
    return (scl->physical_id == 37u && sda->physical_id == 38u) ||
           (scl->physical_id == 38u && sda->physical_id == 37u);
}

static int pi_ai_mic_forced_i2c_line_supported(
        const pi_ai_mic_deep_i2c_line_t *line)
{
    /* Avoid 16-bit aliases in forced mode: they drive too many unrelated pins. */
    return line->released_value <= 0xFFu;
}

static int pi_ai_mic_forced_i2c_pair_init(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *scl,
        const pi_ai_mic_deep_i2c_line_t *sda)
{
    if (scl->port == sda->port) {
        return -1;
    }

    pair->state_count = 0;
    if (pi_ai_mic_deep_i2c_add_state(pair, scl->port, scl->released_value) == NULL ||
            pi_ai_mic_deep_i2c_add_state(pair, sda->port, sda->released_value) == NULL) {
        return -1;
    }

    for (size_t i = 0; i < pair->state_count; i++) {
        port_enable(pair->states[i].port);
        port_write_control_word(pair->states[i].port, XS1_SETC_DRIVE_DRIVE);
        port_out(pair->states[i].port, pair->states[i].value);
        port_sync(pair->states[i].port);
    }

    return 0;
}

static void pi_ai_mic_forced_i2c_pair_deinit(pi_ai_mic_deep_i2c_pair_t *pair)
{
    for (size_t i = 0; i < pair->state_count; i++) {
        port_write_control_word(pair->states[i].port, XS1_SETC_DRIVE_PULL_UP);
        port_out(pair->states[i].port, pair->states[i].value);
        port_sync(pair->states[i].port);
        port_disable(pair->states[i].port);
    }
}

static void pi_ai_mic_forced_i2c_drive(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *line,
        uint32_t value)
{
    pi_ai_mic_deep_i2c_port_state_t *state = pi_ai_mic_deep_i2c_find_state(pair, line->port);
    if (state == NULL) {
        return;
    }

    if (value) {
        state->value |= line->mask;
    } else {
        state->value &= ~line->mask;
    }

    port_write_control_word(line->port, XS1_SETC_DRIVE_DRIVE);
    port_out(line->port, state->value);
    port_sync(line->port);
}

static void pi_ai_mic_forced_i2c_release_for_ack(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *line)
{
    pi_ai_mic_deep_i2c_port_state_t *state = pi_ai_mic_deep_i2c_find_state(pair, line->port);
    if (state == NULL) {
        return;
    }

    state->value |= line->mask;
    port_write_control_word(line->port, XS1_SETC_DRIVE_PULL_UP);
    port_out(line->port, state->value);
    port_sync(line->port);
}

static void pi_ai_mic_forced_i2c_stop(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *scl,
        const pi_ai_mic_deep_i2c_line_t *sda,
        hwtimer_t timer)
{
    pi_ai_mic_forced_i2c_drive(pair, sda, 0u);
    pi_ai_mic_deep_i2c_delay(timer);
    pi_ai_mic_forced_i2c_drive(pair, scl, 1u);
    pi_ai_mic_deep_i2c_delay(timer);
    pi_ai_mic_forced_i2c_drive(pair, sda, 1u);
    pi_ai_mic_deep_i2c_delay(timer);
}

static void pi_ai_mic_forced_i2c_recover(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *scl,
        const pi_ai_mic_deep_i2c_line_t *sda,
        hwtimer_t timer)
{
    pi_ai_mic_forced_i2c_drive(pair, sda, 1u);

    for (unsigned i = 0; i < 9; i++) {
        pi_ai_mic_forced_i2c_drive(pair, scl, 0u);
        pi_ai_mic_deep_i2c_delay(timer);
        pi_ai_mic_forced_i2c_drive(pair, scl, 1u);
        pi_ai_mic_deep_i2c_delay(timer);
    }

    pi_ai_mic_forced_i2c_stop(pair, scl, sda, timer);
}

static void pi_ai_mic_forced_i2c_start(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *scl,
        const pi_ai_mic_deep_i2c_line_t *sda,
        hwtimer_t timer)
{
    pi_ai_mic_forced_i2c_drive(pair, sda, 1u);
    pi_ai_mic_forced_i2c_drive(pair, scl, 1u);
    pi_ai_mic_deep_i2c_delay(timer);
    pi_ai_mic_forced_i2c_drive(pair, sda, 0u);
    pi_ai_mic_deep_i2c_delay(timer);
    pi_ai_mic_forced_i2c_drive(pair, scl, 0u);
    pi_ai_mic_deep_i2c_delay(timer);
}

static int pi_ai_mic_forced_i2c_write_byte(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *scl,
        const pi_ai_mic_deep_i2c_line_t *sda,
        hwtimer_t timer,
        uint8_t byte)
{
    for (int bit = 7; bit >= 0; bit--) {
        pi_ai_mic_forced_i2c_drive(pair, sda, (byte >> bit) & 1u);
        pi_ai_mic_deep_i2c_delay(timer);
        pi_ai_mic_forced_i2c_drive(pair, scl, 1u);
        pi_ai_mic_deep_i2c_delay(timer);
        pi_ai_mic_forced_i2c_drive(pair, scl, 0u);
        pi_ai_mic_deep_i2c_delay(timer);
    }

    /* Precharge before release lets a no-pullup bus stay high long enough to distinguish NACK. */
    pi_ai_mic_forced_i2c_drive(pair, sda, 1u);
    pi_ai_mic_forced_i2c_release_for_ack(pair, sda);
    hwtimer_delay(timer, PI_AI_MIC_FORCED_ACK_RELEASE_TICKS);
    pi_ai_mic_forced_i2c_drive(pair, scl, 1u);
    pi_ai_mic_deep_i2c_delay(timer);
    int ack = pi_ai_mic_deep_i2c_read(sda) == 0;
    pi_ai_mic_forced_i2c_drive(pair, scl, 0u);
    pi_ai_mic_deep_i2c_delay(timer);
    pi_ai_mic_forced_i2c_drive(pair, sda, 1u);

    return ack;
}

static int pi_ai_mic_forced_i2c_probe_address(
        pi_ai_mic_deep_i2c_pair_t *pair,
        const pi_ai_mic_deep_i2c_line_t *scl,
        const pi_ai_mic_deep_i2c_line_t *sda,
        hwtimer_t timer,
        uint8_t address)
{
    pi_ai_mic_forced_i2c_start(pair, scl, sda, timer);
    int ack = pi_ai_mic_forced_i2c_write_byte(pair, scl, sda, timer, (uint8_t)(address << 1));
    pi_ai_mic_forced_i2c_stop(pair, scl, sda, timer);
    return ack;
}

static unsigned long pi_ai_mic_i2c_forced_probe_once(
        const char *tag,
        const pi_ai_mic_deep_i2c_line_t *lines,
        size_t line_count,
        pi_ai_mic_deep_i2c_scan_tick_cb_t tick_cb,
        void *tick_arg)
{
    hwtimer_t timer = hwtimer_alloc();
    unsigned long found_count = 0;
    unsigned long pair_counter = 0;

    for (size_t scl_i = 0; scl_i < line_count; scl_i++) {
        const pi_ai_mic_deep_i2c_line_t *scl = &lines[scl_i];
        if (!pi_ai_mic_forced_i2c_line_supported(scl)) {
            continue;
        }

        for (size_t sda_i = 0; sda_i < line_count; sda_i++) {
            const pi_ai_mic_deep_i2c_line_t *sda = &lines[sda_i];
            if (!pi_ai_mic_forced_i2c_line_supported(sda) ||
                    scl_i == sda_i ||
                    scl->port == sda->port ||
                    scl->physical_id == sda->physical_id) {
                continue;
            }

            pi_ai_mic_deep_i2c_pair_t pair;
            if (pi_ai_mic_forced_i2c_pair_init(&pair, scl, sda) != 0) {
                continue;
            }
            hwtimer_delay(timer, PI_AI_MIC_DEEP_I2C_SCAN_SETTLE_TICKS);
            pi_ai_mic_forced_i2c_recover(&pair, scl, sda, timer);

            unsigned control_acks = 0;
            for (size_t addr_i = 0;
                 addr_i < sizeof(pi_ai_mic_i2c_control_addresses) / sizeof(pi_ai_mic_i2c_control_addresses[0]);
                 addr_i++) {
                control_acks += (unsigned)pi_ai_mic_forced_i2c_probe_address(
                        &pair,
                        scl,
                        sda,
                        timer,
                        pi_ai_mic_i2c_control_addresses[addr_i]);
            }

            unsigned target_acks = 0;
            for (size_t addr_i = 0;
                 addr_i < sizeof(pi_ai_mic_deep_i2c_probe_addresses) / sizeof(pi_ai_mic_deep_i2c_probe_addresses[0]);
                 addr_i++) {
                uint8_t addr = pi_ai_mic_deep_i2c_probe_addresses[addr_i];
                int ack = pi_ai_mic_forced_i2c_probe_address(&pair, scl, sda, timer, addr);
                if (ack) {
                    target_acks++;
                    if (control_acks == 0u) {
                        found_count++;
                        printf("PI_AI_MIC I2C FOUND tag=%s mode=forced_masked_precharge SCL=%s SDA=%s addr=0x%02X control_acks=%u\n",
                               tag,
                               scl->name,
                               sda->name,
                               (unsigned)addr,
                               control_acks);
                    }
                }
            }

            if (pi_ai_mic_deep_i2c_is_expected_pair(scl, sda) ||
                    target_acks != 0u ||
                    control_acks != 0u ||
                    ((pair_counter & 0x1Fu) == 0u)) {
                printf("PI_AI_MIC %s FORCED_ACK SCL=%s SDA=%s target_acks=%u control_acks=%u %s\n",
                       tag,
                       scl->name,
                       sda->name,
                       target_acks,
                       control_acks,
                       (target_acks != 0u && control_acks == 0u) ? "valid_candidate" :
                       (control_acks != 0u ? "reject_all_ack_or_floating" : "nack"));
            }

            pi_ai_mic_forced_i2c_pair_deinit(&pair);

            if (tick_cb != NULL) {
                tick_cb(tick_arg);
            }

            pair_counter++;
        }
    }

    hwtimer_free(timer);
    printf("PI_AI_MIC %s FORCED_ACK pass done, selected_pairs=%lu found=%lu\n",
           tag,
           pair_counter,
           found_count);
    return found_count;
}

static void pi_ai_mic_i2c_forced_probe_run(
        const char *tag,
        const pi_ai_mic_deep_i2c_line_t *lines,
        size_t line_count,
        pi_ai_mic_deep_i2c_scan_tick_cb_t tick_cb,
        void *tick_arg)
{
    unsigned long pass = 0;

    printf("PI_AI_MIC %s I2C forced probe start: masked ports, push-pull address, precharged/released SDA ACK\n",
           tag);
    printf("PI_AI_MIC forced probe accepts ACK only when control addresses 0x3E/0x5A/0x7E stay NACK\n");

    for (;;) {
        printf("PI_AI_MIC %s FORCED_ACK pass %lu begin\n", tag, pass);
        (void)pi_ai_mic_i2c_forced_probe_once(
                tag,
                lines,
                line_count,
                tick_cb,
                tick_arg);
        pass++;
    }
}

static uint8_t pi_ai_mic_i2c_electrical_test_line(
        const char *tag,
        const pi_ai_mic_deep_i2c_line_t *line,
        hwtimer_t timer)
{
    int push_pull = pi_ai_mic_deep_i2c_line_is_single_bit(line);
    uint32_t high_value = line->released_value;
    uint32_t low_value = line->released_value & ~line->mask;

    port_enable(line->port);

    port_write_control_word(line->port, XS1_SETC_DRIVE_PULL_UP);
    port_out(line->port, high_value);
    port_sync(line->port);
    hwtimer_delay(timer, PI_AI_MIC_DEEP_I2C_SCAN_SETTLE_TICKS);
    uint32_t release_1 = pi_ai_mic_deep_i2c_read(line);

    port_write_control_word(line->port,
                            push_pull ? XS1_SETC_DRIVE_DRIVE : XS1_SETC_DRIVE_PULL_UP);
    port_out(line->port, high_value);
    port_sync(line->port);
    hwtimer_delay(timer, PI_AI_MIC_DEEP_I2C_SCAN_SETTLE_TICKS);
    uint32_t drive_high = pi_ai_mic_deep_i2c_read(line);

    port_out(line->port, low_value);
    port_sync(line->port);
    hwtimer_delay(timer, PI_AI_MIC_DEEP_I2C_SCAN_SETTLE_TICKS);
    uint32_t drive_low = pi_ai_mic_deep_i2c_read(line);

    port_write_control_word(line->port, XS1_SETC_DRIVE_PULL_UP);
    port_out(line->port, high_value);
    port_sync(line->port);
    hwtimer_delay(timer, PI_AI_MIC_DEEP_I2C_SCAN_SETTLE_TICKS);
    uint32_t release_2 = pi_ai_mic_deep_i2c_read(line);

    port_disable(line->port);

    uint8_t usable = (release_2 != 0u) && (drive_high != 0u) && (drive_low == 0u);
    printf("PI_AI_MIC ELEC tag=%s line=%s phys=%u mode=%s release=%lu high=%lu low=%lu release2=%lu usable=%u\n",
           tag,
           line->name,
           (unsigned)line->physical_id,
           push_pull ? "push_pull" : "open_drain_alias",
           (unsigned long)release_1,
           (unsigned long)drive_high,
           (unsigned long)drive_low,
           (unsigned long)release_2,
           (unsigned)usable);

    return usable;
}

static unsigned long pi_ai_mic_i2c_probe_selected_pairs_once(
        const char *tag,
        const pi_ai_mic_deep_i2c_line_t *lines,
        size_t line_count,
        const uint8_t *line_usable,
        pi_ai_mic_deep_i2c_scan_tick_cb_t tick_cb,
        void *tick_arg)
{
    hwtimer_t timer = hwtimer_alloc();
    unsigned long found_count = 0;
    unsigned long pair_counter = 0;

    for (size_t scl_i = 0; scl_i < line_count; scl_i++) {
        if (!line_usable[scl_i]) {
            continue;
        }

        for (size_t sda_i = 0; sda_i < line_count; sda_i++) {
            const pi_ai_mic_deep_i2c_line_t *scl = &lines[scl_i];
            const pi_ai_mic_deep_i2c_line_t *sda = &lines[sda_i];

            if (!line_usable[sda_i] ||
                    scl_i == sda_i ||
                    scl->physical_id == sda->physical_id) {
                continue;
            }

            pi_ai_mic_deep_i2c_pair_t pair;
            pi_ai_mic_deep_i2c_pair_init(&pair, scl, sda);
            hwtimer_delay(timer, PI_AI_MIC_DEEP_I2C_SCAN_SETTLE_TICKS);

            uint32_t idle_scl = pi_ai_mic_deep_i2c_read(scl);
            uint32_t idle_sda = pi_ai_mic_deep_i2c_read(sda);
            pi_ai_mic_deep_i2c_recover(&pair, scl, sda, timer);

            int found_any = 0;
            if (idle_scl && idle_sda) {
                for (size_t addr_i = 0;
                     addr_i < sizeof(pi_ai_mic_deep_i2c_probe_addresses) / sizeof(pi_ai_mic_deep_i2c_probe_addresses[0]);
                     addr_i++) {
                    uint8_t addr = pi_ai_mic_deep_i2c_probe_addresses[addr_i];
                    int ack = pi_ai_mic_deep_i2c_probe_address(&pair, scl, sda, timer, addr);
                    if (ack) {
                        found_any = 1;
                        found_count++;
                        printf("PI_AI_MIC I2C FOUND tag=%s SCL=%s SDA=%s idle=%lu/%lu addr=0x%02X\n",
                               tag,
                               scl->name,
                               sda->name,
                               (unsigned long)idle_scl,
                               (unsigned long)idle_sda,
                               (unsigned)addr);
                    }
                }
            }

            if (!found_any &&
                    (pi_ai_mic_deep_i2c_is_expected_pair(scl, sda) ||
                     ((pair_counter & 0x1Fu) == 0u))) {
                printf("PI_AI_MIC %s ELEC_ACK SCL=%s SDA=%s idle=%lu/%lu %s\n",
                       tag,
                       scl->name,
                       sda->name,
                       (unsigned long)idle_scl,
                       (unsigned long)idle_sda,
                       (idle_scl && idle_sda) ? "nack" : "not_idle");
            }

            pi_ai_mic_deep_i2c_pair_deinit(&pair);

            if (tick_cb != NULL) {
                tick_cb(tick_arg);
            }

            pair_counter++;
        }
    }

    hwtimer_free(timer);
    printf("PI_AI_MIC %s ELEC_ACK pass done, selected_pairs=%lu found=%lu\n",
           tag,
           pair_counter,
           found_count);
    return found_count;
}

static void pi_ai_mic_i2c_electrical_test_run(
        const char *tag,
        const pi_ai_mic_deep_i2c_line_t *lines,
        size_t line_count,
        pi_ai_mic_deep_i2c_scan_tick_cb_t tick_cb,
        void *tick_arg)
{
    hwtimer_t timer = hwtimer_alloc();
    uint8_t line_usable[PI_AI_MIC_ELECTRICAL_MAX_LINES] = {0};
    unsigned long pass = 0;

    if (line_count > PI_AI_MIC_ELECTRICAL_MAX_LINES) {
        line_count = PI_AI_MIC_ELECTRICAL_MAX_LINES;
    }

    printf("PI_AI_MIC %s I2C electrical test start: %lu candidates, then ACK probe on usable lines\n",
           tag,
           (unsigned long)line_count);

    for (;;) {
        unsigned long usable_count = 0;
        printf("PI_AI_MIC %s ELEC pass %lu begin\n", tag, pass);

        for (size_t i = 0; i < line_count; i++) {
            line_usable[i] = pi_ai_mic_i2c_electrical_test_line(tag, &lines[i], timer);
            if (line_usable[i]) {
                usable_count++;
            }

            if (tick_cb != NULL) {
                tick_cb(tick_arg);
            }
        }

        printf("PI_AI_MIC %s ELEC pass %lu line usable=%lu/%lu\n",
               tag,
               pass,
               usable_count,
               (unsigned long)line_count);

        (void)pi_ai_mic_i2c_probe_selected_pairs_once(
                tag,
                lines,
                line_count,
                line_usable,
                tick_cb,
                tick_arg);

        pass++;
    }
}

static void pi_ai_mic_deep_i2c_scan_run(
        const char *tag,
        const pi_ai_mic_deep_i2c_line_t *lines,
        size_t line_count,
        pi_ai_mic_deep_i2c_scan_tick_cb_t tick_cb,
        void *tick_arg)
{
    hwtimer_t timer = hwtimer_alloc();
    unsigned long scan_pass = 0;

    printf("PI_AI_MIC %s I2C deep scan start: %lu line candidates, addr 0x18/0x19/0x20..0x27/0x2C\n",
           tag,
           (unsigned long)line_count);

    for (;;) {
        unsigned long pair_counter = 0;
        printf("PI_AI_MIC %s I2C deep scan pass %lu begin\n", tag, scan_pass);

        for (size_t scl_i = 0; scl_i < line_count; scl_i++) {
            for (size_t sda_i = 0; sda_i < line_count; sda_i++) {
                const pi_ai_mic_deep_i2c_line_t *scl = &lines[scl_i];
                const pi_ai_mic_deep_i2c_line_t *sda = &lines[sda_i];

                if (scl_i == sda_i || scl->physical_id == sda->physical_id) {
                    continue;
                }

                pi_ai_mic_deep_i2c_pair_t pair;
                pi_ai_mic_deep_i2c_pair_init(&pair, scl, sda);
                hwtimer_delay(timer, PI_AI_MIC_DEEP_I2C_SCAN_SETTLE_TICKS);

                uint32_t idle_scl = pi_ai_mic_deep_i2c_read(scl);
                uint32_t idle_sda = pi_ai_mic_deep_i2c_read(sda);
                pi_ai_mic_deep_i2c_recover(&pair, scl, sda, timer);

                int found_any = 0;
                if (idle_scl && idle_sda) {
                    for (size_t addr_i = 0;
                         addr_i < sizeof(pi_ai_mic_deep_i2c_probe_addresses) / sizeof(pi_ai_mic_deep_i2c_probe_addresses[0]);
                         addr_i++) {
                        uint8_t addr = pi_ai_mic_deep_i2c_probe_addresses[addr_i];
                        int ack = pi_ai_mic_deep_i2c_probe_address(&pair, scl, sda, timer, addr);
                        if (ack) {
                            found_any = 1;
                            printf("PI_AI_MIC I2C FOUND tag=%s SCL=%s SDA=%s idle=%lu/%lu addr=0x%02X\n",
                                   tag,
                                   scl->name,
                                   sda->name,
                                   (unsigned long)idle_scl,
                                   (unsigned long)idle_sda,
                                   (unsigned)addr);
                        }
                    }
                }

                if (!found_any &&
                    (pi_ai_mic_deep_i2c_is_expected_pair(scl, sda) || ((pair_counter & 0x3Fu) == 0u))) {
                    printf("PI_AI_MIC %s I2C scan SCL=%s SDA=%s idle=%lu/%lu %s\n",
                           tag,
                           scl->name,
                           sda->name,
                           (unsigned long)idle_scl,
                           (unsigned long)idle_sda,
                           (idle_scl && idle_sda) ? "nack" : "not_idle");
                }

                pi_ai_mic_deep_i2c_pair_deinit(&pair);

                if (tick_cb != NULL) {
                    tick_cb(tick_arg);
                }

                pair_counter++;
            }
        }

        printf("PI_AI_MIC %s I2C deep scan pass %lu done, pairs=%lu\n",
               tag,
               scan_pass,
               pair_counter);
        scan_pass++;
    }
}

#endif // PI_AI_MIC_I2C_SCAN_H_
