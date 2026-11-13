/* Host variant: never runs a master transaction, even before the RTOS.
 * Use the vendor I2C slave engine, not a new bit-level bus implementation.
 */
#include "voicedsp_v2_host_boot.h"
#include "i2c.h"
#include "platform/platform_conf.h"
#include <string.h>
#if !INT_DEVICE || appconfI2S_ROLE_MASTER || !appconfI2C_CTRL_ENABLED || appconfUSB_ENABLED || appconfSPI_CTRL_ENABLED
#error Host candidate requires external I2S clocks and I2C slave control only
#endif
#if I2C_SLAVE_ADDR != 0x2c
#error Bootstrap and runtime must share reviewed XMOS address 0x2c
#endif
#if ON_TILE(0)
I2C_CALLBACK_ATTR static i2c_slave_ack_t boot_read_request(void *context)
{ vdsp_v2_host_boot_read_begin(context); return I2C_SLAVE_ACK; }
I2C_CALLBACK_ATTR static i2c_slave_ack_t boot_write_request(void *context)
{ vdsp_v2_host_boot_write_begin(context); return I2C_SLAVE_ACK; }
I2C_CALLBACK_ATTR static uint8_t boot_read_byte(void *context)
{ return vdsp_v2_host_boot_read(context); }
I2C_CALLBACK_ATTR static i2c_slave_ack_t boot_write_byte(void *context, uint8_t value)
{ return vdsp_v2_host_boot_receive(context,value) ? I2C_SLAVE_ACK : I2C_SLAVE_NACK; }
I2C_CALLBACK_ATTR static void boot_stop(void *context)
{ vdsp_v2_host_boot_stop(context); }
I2C_CALLBACK_ATTR static int boot_shutdown(void *context)
{ return ((vdsp_v2_host_boot *)context)->committed; }
#endif
void vdsp_v2_pre_boot(void)
{
#if ON_TILE(0)
    /* PCAL P0.2 stays input. Release X0D29 before the host probes the DAC. */
    hwtimer_t reset_timer = hwtimer_alloc();
    port_enable(GPO_TILE_0_PORT_8C);
    port_out(GPO_TILE_0_PORT_8C, 0xf7u);
    port_sync(GPO_TILE_0_PORT_8C);
    hwtimer_delay(reset_timer, 1000000u);
    port_out(GPO_TILE_0_PORT_8C, 0xffu);
    port_sync(GPO_TILE_0_PORT_8C);
    hwtimer_delay(reset_timer, 1000000u);
    hwtimer_free(reset_timer);
    vdsp_v2_host_boot state;
    (void)vdsp_v2_host_boot_init(&state,appconfLRCLK_NOMINAL_HZ/1000);
    const i2c_callback_group_t callbacks = {
        .ack_read_request=boot_read_request, .ack_write_request=boot_write_request,
        .master_requires_data=boot_read_byte, .master_sent_data=boot_write_byte,
        .stop_bit=boot_stop, .shutdown=boot_shutdown, .app_data=&state
    };
    vdsp_v2_set_boot_geometry(VDSP_MIC_UNKNOWN);
    /* Blocks awaiting a validated host record + STOP. Shutdown disables both
     * ports; the RTOS slave takes them later. No opposing master is created.
     */
    i2c_slave(&callbacks,PORT_I2C_SCL,PORT_I2C_SDA,I2C_SLAVE_ADDR);
    /* Runtime GPO takes ownership before the host configures analog audio. */
    port_disable(GPO_TILE_0_PORT_8C);
    memcpy(vdsp_v2_host_record,state.accepted,VDSP_HOST_BOOT_SIZE);
    vdsp_v2_set_boot_geometry(state.accepted[5]);
    vdsp_v2_boot_port0 = state.accepted[6]; vdsp_v2_boot_port1 = state.accepted[7];
    /* These results record HOST declarations, not independent XMOS probing. */
    vdsp_v2_boot_pcal_result = vdsp_v2_boot_dac_reset_result = 0;
    pi_ai_mic_boot_detect_valid = 1;
    pi_ai_mic_boot_detect_jack = (vdsp_v2_boot_port0 & 0x20) != 0;
    pi_ai_mic_boot_detect_mic_square = state.accepted[5] != VDSP_MIC_EXTERNAL_LINEAR;
#endif
}
