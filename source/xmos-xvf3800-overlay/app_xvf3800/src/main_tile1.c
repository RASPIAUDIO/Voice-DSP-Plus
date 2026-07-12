// Copyright 2022-2024 XMOS LIMITED.
// This Software is subject to the terms of the XCORE VocalFusion Licence.
/// tile 1 main. Starts all top level baremetal and rtos threads.

#define DEBUG_UNIT MAIN_TILE1
#ifndef DEBUG_PRINT_ENABLE_MAIN_TILE1
    #define DEBUG_PRINT_ENABLE_MAIN_TILE1 0
#endif
#include "debug_print.h"

#include "xcore/chanend.h"
#include "xcore/hwtimer.h"
#include "xcore/parallel.h"
#include "xcore/port.h"
#include "FreeRTOS.h"
#include "task.h"
#ifdef __XS3A__
#include <xscope.h>
#endif

#include <stddef.h>
#include <string.h>

// bsp
#include "platform/platform_init.h"
#include "platform/app_pll_ctrl.h"
#include "tile_common.h"
#include "data_plane.h"
#include "app_conf.h"
#if (PI_AI_MIC_I2C_ELECTRICAL_TEST_ENABLE && PI_AI_MIC_I2C_ELECTRICAL_TILE1_ENABLE) || \
        (PI_AI_MIC_I2C_FORCED_PROBE_TEST_ENABLE && PI_AI_MIC_I2C_FORCED_PROBE_TILE1_ENABLE) || \
        (PI_AI_MIC_I2C_PORT_SCAN_TEST_ENABLE && PI_AI_MIC_I2C_PORT_SCAN_TILE1_ENABLE) || \
        (PI_AI_MIC_I2C_PIN_LOCATOR_TEST_ENABLE && PI_AI_MIC_I2C_PIN_LOCATOR_TILE1_ENABLE)
#include "pi_ai_mic_i2c_scan.h"
#endif
#include "servicer.h"
#include "pll_servicer.h"
#include "platform/driver_instances.h"
#include "usb_support.h"
#include "tusb.h"
#include "usb_audio.h"
#include "usb_hid.h"
#include "usb_descriptors.h"
#include "user_config/pi_ai_mic_hw.h"
#include "user_config/pi_ai_mic_neopixel.h"

#include "rtos_usb.h" // tile[1] version, so provides usb_driver_init

TaskHandle_t io_config_servicer_parent_task = NULL;

#if appconfUSB_ENABLED
#if ( 0 < DFU_CONTROL )
/// @brief function to receive from tile 0 the information to configure the DFU image context
static void receive_dfu_info()
{
    rtos_intertile_t * intertile_ctx = get_intertile_ctx();
    rtos_dfu_image_t * dfu_image_ctx = get_dfu_image_ctx();
    // Receive all the necessary information for the DFU Image Context
    rtos_intertile_rx_len(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, RTOS_OSAL_WAIT_FOREVER);
    rtos_intertile_rx_data(intertile_ctx, &dfu_image_ctx->factory_image_ctx.startAddress, sizeof(uint32_t));
    rtos_intertile_rx_len(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, RTOS_OSAL_WAIT_FOREVER);
    rtos_intertile_rx_data(intertile_ctx, &dfu_image_ctx->factory_image_ctx.size, sizeof(uint32_t));
    rtos_intertile_rx_len(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, RTOS_OSAL_WAIT_FOREVER);
    rtos_intertile_rx_data(intertile_ctx, &dfu_image_ctx->factory_image_ctx.version, sizeof(uint32_t));
    rtos_intertile_rx_len(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, RTOS_OSAL_WAIT_FOREVER);
    rtos_intertile_rx_data(intertile_ctx, &dfu_image_ctx->upgrade_image_ctx.startAddress, sizeof(uint32_t));
    rtos_intertile_rx_len(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, RTOS_OSAL_WAIT_FOREVER);
    rtos_intertile_rx_data(intertile_ctx, &dfu_image_ctx->upgrade_image_ctx.size, sizeof(uint32_t));
    rtos_intertile_rx_len(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, RTOS_OSAL_WAIT_FOREVER);
    rtos_intertile_rx_data(intertile_ctx, &dfu_image_ctx->upgrade_image_ctx.version, sizeof(uint32_t));
    rtos_intertile_rx_len(intertile_ctx, appconfUSB_MANAGER_SYNC_PORT, RTOS_OSAL_WAIT_FOREVER);
    rtos_intertile_rx_data(intertile_ctx, &dfu_image_ctx->data_partition_base_addr, sizeof(uint32_t));
    rtos_dfu_image_print_debug(dfu_image_ctx);
}
#endif

static void configure_uac2_bit_depth(void)
{
    uint8_t bit_depth_in = 0;
    uint8_t bit_depth_out = 0;
    get_usb_bit_depth(&bit_depth_in, &bit_depth_out);

    // tud_descriptor_configuration_cb() returns a pointer to a const array of uint8_t.
    // Cast away the constancy to allow individual element modifications below.
    uint8_t *desc_ptr = (uint8_t*)tud_descriptor_configuration_cb(0);
    size_t offset;

#if AUDIO_OUTPUT_ENABLED
    // Audio OP EP: Type I Format Type Descriptor, _subslotsize and _bitresolution
    offset = uac2_bytes_per_sample_rx_offset;
    desc_ptr[offset] = bit_depth_out / 8;
    desc_ptr[offset+1] = bit_depth_out;
#endif

#if AUDIO_INPUT_ENABLED
    // Audio IP EP: Type I Format Type Descriptor, _subslotsize and _bitresolution
    offset = uac2_bytes_per_sample_tx_offset;
    desc_ptr[offset] = bit_depth_in / 8;
    desc_ptr[offset+1] = bit_depth_in;
#endif

#if AUDIO_OUTPUT_ENABLED
    // Audio OP EP: Standard AS Isochronous Audio Data Endpoint Descriptor, _maxEPsize
    offset = uac2_ep_out_sz_offset;
    uint16_t audio_func_1_ep_out_sz = (AUDIO_FRAMES_PER_USB_FRAME * (bit_depth_out / 8) * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX);
    desc_ptr[offset] = TU_U16_LOW(audio_func_1_ep_out_sz);
    desc_ptr[offset+1] = TU_U16_HIGH(audio_func_1_ep_out_sz);
#endif

#if AUDIO_INPUT_ENABLED
    // Audio IP EP: Standard AS Isochronous Audio Data Endpoint Descriptor, _maxEPsize
    offset = uac2_ep_in_sz_offset;
    uint16_t audio_func_1_ep_in_sz = (AUDIO_FRAMES_PER_USB_FRAME * (bit_depth_in / 8) * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX);
    desc_ptr[offset] = TU_U16_LOW(audio_func_1_ep_in_sz);
    desc_ptr[offset+1] = TU_U16_HIGH(audio_func_1_ep_in_sz);
#endif
}
#endif


/// @brief this tiles rtos entry
static void rtos_app(void* args)
{
    servicer_t** servicers = (servicer_t**)args;

    platform_start();

#if PI_AI_MIC_DOA_NEOPIXEL_ENABLE
    (void) pi_ai_mic_neopixel_start_task();
#endif

#if appconfUSB_ENABLED
#if ( 0 < DFU_CONTROL )
    receive_dfu_info();
#endif

#if (HID_CONTROL)
    hid_in_servicer_args_t *hid_in_servicer_args;
    hid_in_task_args_t *hid_task_args;
    hid_init(&hid_in_servicer_args, &hid_task_args);

    xTaskCreate(
        hid_in_servicer,
        "HID IN Servicer",
        RTOS_THREAD_STACK_SIZE(hid_in_servicer),
        hid_in_servicer_args,
        appconfTEST_TASK_PRIORITY,
        NULL
    );
#endif
#endif

    io_config_servicer_parent_task = xTaskGetCurrentTaskHandle();

    xTaskCreate(
        io_config_servicer,
        "IO config servicer",
        RTOS_THREAD_STACK_SIZE(io_config_servicer),
        servicers[IO_CONFIG_SERVICER_INDEX],
        appconfTEST_TASK_PRIORITY,
        NULL
    );

    // Wait to be signalled from the IO config servicer (that starts the DAC) before creating the other Servicer tasks
    // It is important that the remaining servicers don't start till the IO config task has started the DAC and signalled the GPO task to start the control IO (SPI or I2C slave) tasks.
    // This is needed because the IO config servicer uses the intertile to communicate to the GPO task during DAC init and for signaling control start. If we let the other tile 1
    // servicers start meanwhile, this would cause them to block the intertile context when trying to register on the device control contexts for I2C or SPI slave which will only start once
    // IO config task finishes DAC init and signal GPO task to start the I2C or SPI slave control tasks.
#if !appconfUSB_ENABLED
    uint32_t val;
    xTaskNotifyWait(0, ~0, &val, portMAX_DELAY);
#endif
    // Delay start of any other tasks that will attempt to register to a device_control context over intertile
    xTaskCreate(
        servicer_task,
        "AEC servicer",
        RTOS_THREAD_STACK_SIZE(servicer_task),
        servicers[AEC_SERVICER_INDEX],
        appconfTEST_TASK_PRIORITY,
        NULL
    );

    xTaskCreate(
        servicer_task,
        "Audio servicer",
        RTOS_THREAD_STACK_SIZE(servicer_task),
        servicers[AUDIO_SERVICER_INDEX],
        appconfTEST_TASK_PRIORITY,
        NULL
    );

    xTaskCreate(
        application_servicer,
        "Application servicer",
        RTOS_THREAD_STACK_SIZE(application_servicer),
        servicers[APPLICATION_SERVICER_INDEX],
        appconfTEST_TASK_PRIORITY,
        NULL
    );

    xTaskCreate(
        servicer_task,
        "PLL servicer",
        RTOS_THREAD_STACK_SIZE(servicer_task),
        servicers[PLL_SERVICER_INDEX],
        appconfTEST_TASK_PRIORITY,
        NULL
    );

#if appconfUSB_ENABLED
    control_ret_t dc_ret = device_control_resources_register(device_control_usb_ctx,
                                               pdMS_TO_TICKS(5000));

    if (dc_ret != CONTROL_SUCCESS) {
        rtos_printf("Device control resources failed to register for USB on tile %d\n", THIS_XCORE_TILE);
    } else {
        rtos_printf("Device control resources registered for USB on tile %d\n", THIS_XCORE_TILE);
    }
    xassert(dc_ret == CONTROL_SUCCESS);

    // Configure UAC2 depth related fields in the descriptor
    configure_uac2_bit_depth();

    usb_manager_start(configMAX_PRIORITIES-1);

#if (HID_CONTROL)
    xTaskCreate((TaskFunction_t) hid_task, // Task for handling the HID endpoint
                    "hid_task",
                    portTASK_STACK_DEPTH(hid_task),
                    hid_task_args,
                    appconfTEST_TASK_PRIORITY,
                    NULL);
#endif
#endif

    // done
    vTaskDelete(NULL);
}

volatile uint8_t user_reserved[USER_MEMORY_RESERVED_BYTES];

#if PI_AI_MIC_I2C_PORT_SCAN_TEST_ENABLE && PI_AI_MIC_I2C_PORT_SCAN_TILE1_ENABLE
static void pi_ai_mic_tile1_i2c_port_scan_test(void)
{
    pi_ai_mic_deep_i2c_scan_run(
            "TILE1",
            pi_ai_mic_deep_i2c_tile1_lines,
            sizeof(pi_ai_mic_deep_i2c_tile1_lines) / sizeof(pi_ai_mic_deep_i2c_tile1_lines[0]),
            NULL,
            NULL);
}
#endif

#if PI_AI_MIC_I2C_ELECTRICAL_TEST_ENABLE && PI_AI_MIC_I2C_ELECTRICAL_TILE1_ENABLE
static void pi_ai_mic_tile1_i2c_electrical_test(void)
{
    pi_ai_mic_i2c_electrical_test_run(
            "TILE1",
            pi_ai_mic_deep_i2c_tile1_lines,
            sizeof(pi_ai_mic_deep_i2c_tile1_lines) / sizeof(pi_ai_mic_deep_i2c_tile1_lines[0]),
            NULL,
            NULL);
}
#endif

#if PI_AI_MIC_I2C_FORCED_PROBE_TEST_ENABLE && PI_AI_MIC_I2C_FORCED_PROBE_TILE1_ENABLE
static void pi_ai_mic_tile1_i2c_forced_probe_test(void)
{
    pi_ai_mic_i2c_forced_probe_run(
            "TILE1",
            pi_ai_mic_deep_i2c_tile1_lines,
            sizeof(pi_ai_mic_deep_i2c_tile1_lines) / sizeof(pi_ai_mic_deep_i2c_tile1_lines[0]),
            NULL,
            NULL);
}
#endif

#if PI_AI_MIC_I2C_PIN_LOCATOR_TEST_ENABLE && PI_AI_MIC_I2C_PIN_LOCATOR_TILE1_ENABLE
static void pi_ai_mic_tile1_i2c_pin_locator_test(void)
{
    pi_ai_mic_i2c_pin_locator_run(
            "TILE1",
            pi_ai_mic_deep_i2c_tile1_lines,
            sizeof(pi_ai_mic_deep_i2c_tile1_lines) / sizeof(pi_ai_mic_deep_i2c_tile1_lines[0]),
            NULL,
            NULL);
}
#endif

/// @brief this tiles entry point
void main_tile1(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3)
{
#if PI_AI_MIC_I2C_ELECTRICAL_TEST_ENABLE && PI_AI_MIC_I2C_ELECTRICAL_TILE1_ENABLE
    (void)c0;
    (void)c1;
    (void)c2;
    (void)c3;
#if defined(__XS3A__) && PI_AI_MIC_XSCOPE_ENABLE
    xscope_config_io(XSCOPE_IO_BASIC);
#endif
    pi_ai_mic_tile1_i2c_electrical_test();
#elif PI_AI_MIC_I2C_FORCED_PROBE_TEST_ENABLE && PI_AI_MIC_I2C_FORCED_PROBE_TILE1_ENABLE
    (void)c0;
    (void)c1;
    (void)c2;
    (void)c3;
#if defined(__XS3A__) && PI_AI_MIC_XSCOPE_ENABLE
    xscope_config_io(XSCOPE_IO_BASIC);
#endif
    pi_ai_mic_tile1_i2c_forced_probe_test();
#elif PI_AI_MIC_I2C_PIN_LOCATOR_TEST_ENABLE && PI_AI_MIC_I2C_PIN_LOCATOR_TILE1_ENABLE
    (void)c0;
    (void)c1;
    (void)c2;
    (void)c3;
#if defined(__XS3A__) && PI_AI_MIC_XSCOPE_ENABLE
    xscope_config_io(XSCOPE_IO_BASIC);
#endif
    pi_ai_mic_tile1_i2c_pin_locator_test();
#elif PI_AI_MIC_I2C_PORT_SCAN_TEST_ENABLE && PI_AI_MIC_I2C_PORT_SCAN_TILE1_ENABLE
    (void)c0;
    (void)c1;
    (void)c2;
    (void)c3;
#if defined(__XS3A__) && PI_AI_MIC_XSCOPE_ENABLE
    xscope_config_io(XSCOPE_IO_BASIC);
#endif
    pi_ai_mic_tile1_i2c_port_scan_test();
#endif

#if defined(__XS3A__) && PI_AI_MIC_XSCOPE_ENABLE
    xscope_config_io(XSCOPE_IO_BASIC);
#endif

    (void)c3;
    servicer_t* tile_1_servicers[NUM_TILE_1_SERVICERS]; // Array of pointers
    servicer_t servicer_aec;
    servicer_t servicer_audio;
    servicer_t servicer_io_config;
    servicer_t servicer_io_application;
    servicer_t servicer_pll;
    tile_1_servicers[AEC_SERVICER_INDEX] = &servicer_aec;
    tile_1_servicers[AUDIO_SERVICER_INDEX] = &servicer_audio;
    tile_1_servicers[IO_CONFIG_SERVICER_INDEX] = &servicer_io_config;
    tile_1_servicers[APPLICATION_SERVICER_INDEX] = &servicer_io_application;
    tile_1_servicers[PLL_SERVICER_INDEX] = &servicer_pll;

    aec_servicer_init(tile_1_servicers[AEC_SERVICER_INDEX]);
    audio_servicer_init(tile_1_servicers[AUDIO_SERVICER_INDEX]);
    io_config_servicer_init(tile_1_servicers[IO_CONFIG_SERVICER_INDEX]);
    application_servicer_init(tile_1_servicers[APPLICATION_SERVICER_INDEX]);
    pll_servicer_init(tile_1_servicers[PLL_SERVICER_INDEX]);

    // Open new cross tile paths tile1 c1 <-> tile 0 c0 using existing channel c0 <-> c1
    c1 = chanend_alloc();
    chan_out_word(c0, c1);
    chanend_set_dest(c1, chan_in_word(c0));

    // Now exchange pre-boot info with Tile[0]
    set_spi_slave_boot_status(chan_in_byte(c1)); // Pickup tile[0] boot status for use later
#if PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
    const uint8_t mic_geometry_boot = chan_in_byte(c1);
    pi_ai_mic_set_mic_geometry_cached((mic_geometry_boot & 0x01u) != 0,
                                      (mic_geometry_boot & 0x02u) != 0);
#endif
    chan_out_byte(c1, get_core_burn_status()); // Send core burn status to tile[0]

    uint8_t bit_depth_in = 0;
    uint8_t bit_depth_out = 0;
    get_usb_bit_depth(&bit_depth_in, &bit_depth_out);
    chan_out_byte(c1, bit_depth_in);
    chan_out_byte(c1, bit_depth_out);

    chanend_t c_mic_to_audio = c1;

    // Open new cross tile paths tile1 c2 <-> tile 0 c2 using existing channel c0 <-> c1
    c2 = chanend_alloc();
    chan_out_word(c0, c2);
    chanend_set_dest(c2, chan_in_word(c0));
    chanend_t c_aec_to_pp = c2;

    // Open new cross tile paths tile1 c_i2s_to_usb <-> tile 0 c_i2s_to_usb using existing channel c0 <-> c1
    chanend_t c_i2s_to_usb = chanend_alloc();
    chan_out_word(c0, c_i2s_to_usb);
    chanend_set_dest(c_i2s_to_usb, chan_in_word(c0));

    // Open new cross tile paths tile1 c_ep0_proxy <-> tile 0 c_ep0_proxy using existing channel c0 <-> c1
    chanend_t c_ep0_proxy;
    c_ep0_proxy = chanend_alloc();
    chan_out_word(c0, c_ep0_proxy);
    chanend_set_dest(c_ep0_proxy, chan_in_word(c0));

    chanend_t c_ep_hid_proxy;
    c_ep_hid_proxy = chanend_alloc();
    chan_out_word(c0, c_ep_hid_proxy);
    chanend_set_dest(c_ep_hid_proxy, chan_in_word(c0));

    chanend_t c_ep_proxy_xfer_complete;
    c_ep_proxy_xfer_complete = chanend_alloc();
    chan_out_word(c0, c_ep_proxy_xfer_complete);
    chanend_set_dest(c_ep_proxy_xfer_complete, chan_in_word(c0));

    if(appconfFIXED_MCLK_APP_PLL) // Drive a constant MCLK
    {
        app_pll_init(); // Can be called from either tile but use tile[1] to save mem on tile[0]
    }

    debug_printf("Force linking of USER reserved memory %u\n", user_reserved[0]);
    user_reserved[0]++; // forces compiler to link in the buffer

    channel_t c_shf_aec = chan_alloc();
    streaming_channel_t c_audio_to_i2s = s_chan_alloc();
    streaming_channel_t c_i2s_to_audio = s_chan_alloc();

#if appconfUSB_ENABLED
    rtos_intertile_t * intertile_ctx = get_intertile_ctx();

    usb_audio_init(intertile_ctx, appconfTEST_TASK_PRIORITY);
    usb_driver_init(c_ep0_proxy, c_ep_hid_proxy, c_ep_proxy_xfer_complete);
    usb_manager_init();
#endif

    PAR_JOBS(
        PJOB(aec_task,
                (&c_aec_to_pp,
                c_shf_aec.end_a,
                &servicer_aec.res_info[AEC_RESID - AEC_SERVICER_RESID].control_pkt_queue)),                // Spawns four high priority logical cores
        PJOB(audio_manager_task,
                (c_mic_to_audio,
                c_shf_aec.end_b,
                c_audio_to_i2s.end_b,
                c_i2s_to_audio.end_b,
                &servicer_audio.res_info[AUDIO_MGR_RESID - AUDIO_SERVICER_RESID].control_pkt_queue)),
        PJOB(i2s_task, (c_audio_to_i2s.end_a, c_i2s_to_audio.end_a, c_i2s_to_usb)),
        PJOB(reserved_core, ()),
        PJOB(tile_common_start_rtos_app,
                (c0,
                rtos_app,
                RTOS_THREAD_STACK_SIZE(rtos_app),
                &tile_1_servicers))
    );
}
