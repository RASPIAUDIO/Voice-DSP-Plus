// Copyright 2022-2024 XMOS LIMITED.
// This Software is subject to the terms of the XCORE VocalFusion Licence.

// Also please see the .xn file in bsp_config for port mapping

#ifndef APP_CONF_H_
#define APP_CONF_H_

/* Application tile specifiers */
#include "platform.h"
#include "BeClearCommon.h"

/* App data plane configuration */
// CMake-configured
#if !(INT_DEVICE || INT_HOST || UA) || ((INT_DEVICE + INT_HOST + UA) > 1)
#error Must define either INT_DEVICE or INT_HOST or UA
#endif // CONFIG CHECK

#ifndef appconfLRCLK_NOMINAL_HZ
#define appconfLRCLK_NOMINAL_HZ                 16000
#endif  //appconfLRCLK_NOMINAL_HZ

#ifndef appconfUSB_IN_OUT_NOMINAL_HZ
#define appconfUSB_IN_OUT_NOMINAL_HZ            16000
#endif  //appconfUSB_OUT_NOMINAL_HZ

#ifndef appconfUSB_IN_NOMINAL_HZ
#define appconfUSB_IN_NOMINAL_HZ                appconfUSB_IN_OUT_NOMINAL_HZ
#endif  //appconfUSB_IN_NOMINAL_HZ

#ifndef PI_AI_MIC_USB_RAW_MIC_4CH
#define PI_AI_MIC_USB_RAW_MIC_4CH               0
#endif  // PI_AI_MIC_USB_RAW_MIC_4CH

#ifndef PI_AI_MIC_USB_SPLIT_AUDIO
#define PI_AI_MIC_USB_SPLIT_AUDIO               0
#endif  // PI_AI_MIC_USB_SPLIT_AUDIO

#ifndef PI_AI_MIC_USB_RAW_MIC_2CH
#define PI_AI_MIC_USB_RAW_MIC_2CH               0
#endif  // PI_AI_MIC_USB_RAW_MIC_2CH

#ifndef PI_AI_MIC_USB_MEDIA_2CH
#define PI_AI_MIC_USB_MEDIA_2CH                 0
#endif  // PI_AI_MIC_USB_MEDIA_2CH

#ifndef PI_AI_MIC_USB_PROCESSED_MIC_2CH
#define PI_AI_MIC_USB_PROCESSED_MIC_2CH         0
#endif  // PI_AI_MIC_USB_PROCESSED_MIC_2CH

#ifndef PI_AI_MIC_USB_PROCESSED_2CH
#define PI_AI_MIC_USB_PROCESSED_2CH             PI_AI_MIC_USB_PROCESSED_MIC_2CH
#endif  // PI_AI_MIC_USB_PROCESSED_2CH

#ifndef PI_AI_MIC_USB_PROCESSED_2CH_16K
#define PI_AI_MIC_USB_PROCESSED_2CH_16K         0
#endif  // PI_AI_MIC_USB_PROCESSED_2CH_16K

#ifndef PI_AI_MIC_USB_PROCESSED_2CH_GENERIC_IO
#define PI_AI_MIC_USB_PROCESSED_2CH_GENERIC_IO  0
#endif  // PI_AI_MIC_USB_PROCESSED_2CH_GENERIC_IO

#ifndef PI_AI_MIC_RPI_I2S_PROCESSED_2CH
#define PI_AI_MIC_RPI_I2S_PROCESSED_2CH         0
#endif  // PI_AI_MIC_RPI_I2S_PROCESSED_2CH

#ifndef PI_AI_MIC_USB_OUTPUT_ONLY_DESCRIPTOR
#define PI_AI_MIC_USB_OUTPUT_ONLY_DESCRIPTOR    0
#endif  // PI_AI_MIC_USB_OUTPUT_ONLY_DESCRIPTOR

#ifndef PI_AI_MIC_USB_DEBUG_6CH
#define PI_AI_MIC_USB_DEBUG_6CH                 0
#endif  // PI_AI_MIC_USB_DEBUG_6CH

#ifndef PI_AI_MIC_USB_DEBUG_6CH_16K
#define PI_AI_MIC_USB_DEBUG_6CH_16K             0
#endif  // PI_AI_MIC_USB_DEBUG_6CH_16K

#ifndef PI_AI_MIC_USB_DEBUG_6CH_PROCESSED_TAIL
#define PI_AI_MIC_USB_DEBUG_6CH_PROCESSED_TAIL  0
#endif  // PI_AI_MIC_USB_DEBUG_6CH_PROCESSED_TAIL

#ifndef PI_AI_MIC_RAW_MIC_PRODUCT_ID
#define PI_AI_MIC_RAW_MIC_PRODUCT_ID            0x4F31
#endif  // PI_AI_MIC_RAW_MIC_PRODUCT_ID

#ifndef PI_AI_MIC_DEBUG_6CH_PRODUCT_ID
#define PI_AI_MIC_DEBUG_6CH_PRODUCT_ID          0x4F32
#endif  // PI_AI_MIC_DEBUG_6CH_PRODUCT_ID

#ifndef PI_AI_MIC_USB_2CH_PRODUCT_ID
#define PI_AI_MIC_USB_2CH_PRODUCT_ID            0x4F33
#endif  // PI_AI_MIC_USB_2CH_PRODUCT_ID

#ifndef PI_AI_MIC_USB_MEDIA_2CH_PRODUCT_ID
#define PI_AI_MIC_USB_MEDIA_2CH_PRODUCT_ID      0x4F3C
#endif  // PI_AI_MIC_USB_MEDIA_2CH_PRODUCT_ID

#ifndef PI_AI_MIC_USB_PROCESSED_2CH_PRODUCT_ID
#define PI_AI_MIC_USB_PROCESSED_2CH_PRODUCT_ID  0x4F62
#endif  // PI_AI_MIC_USB_PROCESSED_2CH_PRODUCT_ID

#ifndef PI_AI_MIC_RAW4_SPEAKER_PRODUCT_ID
#define PI_AI_MIC_RAW4_SPEAKER_PRODUCT_ID       0x4F42
#endif  // PI_AI_MIC_RAW4_SPEAKER_PRODUCT_ID

#ifndef PI_AI_MIC_USB_RAW_MIC_ONLY_DESCRIPTOR
#define PI_AI_MIC_USB_RAW_MIC_ONLY_DESCRIPTOR   0
#endif  // PI_AI_MIC_USB_RAW_MIC_ONLY_DESCRIPTOR

#ifndef PI_AI_MIC_USB_RAW_MIC_BIT_DEPTH_IN
#define PI_AI_MIC_USB_RAW_MIC_BIT_DEPTH_IN      16
#endif  // PI_AI_MIC_USB_RAW_MIC_BIT_DEPTH_IN
#ifndef PI_AI_MIC_USB_RAW_MIC_GAIN_SHIFT
#define PI_AI_MIC_USB_RAW_MIC_GAIN_SHIFT        0
#endif  // PI_AI_MIC_USB_RAW_MIC_GAIN_SHIFT

#ifndef appconfUSB_OUT_NOMINAL_HZ
#define appconfUSB_OUT_NOMINAL_HZ               appconfUSB_IN_OUT_NOMINAL_HZ
#endif  //appconfUSB_OUT_NOMINAL_HZ

#ifndef appconfEXTERNAL_MCLK
#define appconfEXTERNAL_MCLK                    0 // On intdev builds only, we can accept an external MCLK if needed (no sw_pll clock recovery)
#endif  //appconfEXTERNAL_MCLK

#ifndef appconfUSB_CHANNELS_IN
#if PI_AI_MIC_USB_DEBUG_6CH
#define appconfUSB_CHANNELS_IN                  6
#elif PI_AI_MIC_USB_RAW_MIC_4CH
#define appconfUSB_CHANNELS_IN                  4
#else
#define appconfUSB_CHANNELS_IN                  2 // TODO make 1 and ensure all buffer handling works
#endif
#endif  //appconfUSB_CHANNELS_IN

#ifndef appconfUSB_CHANNELS_OUT
#define appconfUSB_CHANNELS_OUT                 2
#endif  //appconfUSB_CHANNELS_OUT

#ifndef appconfSPATIAL
// set to 1 to pan the auto select beam onto left and right
// channels using DoA in the post_shf_dsp function. Note that
// this also requires linking against fwk_xvf::pan_pot, see
// example spatial build configs for more info.
#define appconfSPATIAL 0
#endif

#if defined(MIC_ARRAY_TYPE_SQR) && MIC_ARRAY_TYPE_SQR
#define MIC_ARRAY_TYPE BECLEAR_CIRCULAR_ARRAY
#elif defined(MIC_ARRAY_TYPE_LIN) && MIC_ARRAY_TYPE_LIN
#define MIC_ARRAY_TYPE BECLEAR_LINEAR_ARRAY
#else
#error "Incorrectly set mic array type"
#endif

#if INT_DEVICE
#define appconfI2S_ROLE_MASTER                  0
#define appconfFIXED_MCLK_APP_PLL               0
#if appconfEXTERNAL_MCLK
    #define appconfRECOVER_MCLK_I2S_APP_PLL     0
#else
    #define appconfRECOVER_MCLK_I2S_APP_PLL     1
#endif
#define appconfUSB_ENABLED                      0
#define HID_CONTROL                             0
#define MIC_ARRAY_CONFIG_MCLK_FREQ              12288000
#define appconfNUM_I2S_PINS_IN                  1
#define appconfPROCESSED_MIC_OUT_PORT           PORT_I2S_DATA1
#define appconfFAR_END_IN_PORT                  PORT_I2S_DATA0
#define DFU_CONTROL                             1


#elif INT_HOST
#define appconfI2S_ROLE_MASTER                  1
#define appconfFIXED_MCLK_APP_PLL               1
#define appconfRECOVER_MCLK_I2S_APP_PLL         0
#define appconfUSB_ENABLED                      0
#define HID_CONTROL                             0
#define MIC_ARRAY_CONFIG_MCLK_FREQ              24576000
#define appconfNUM_I2S_PINS_IN                  1
#define appconfPROCESSED_MIC_OUT_PORT           PORT_I2S_DATA0
#define appconfFAR_END_IN_PORT                  PORT_I2S_DATA1

#else // UA
#define appconfI2S_ROLE_MASTER                  1
#define appconfFIXED_MCLK_APP_PLL               0
#define appconfUSB_ENABLED                      1
#define MIC_ARRAY_CONFIG_MCLK_FREQ              12288000
#define appconfRECOVER_MCLK_I2S_APP_PLL         0 // The clock is recovered instead in usb_buffer
#define appconfNUM_I2S_PINS_IN                  1
#define appconfPROCESSED_MIC_OUT_PORT           PORT_I2S_DATA0 //TODO this is actually far end out
#define appconfFAR_END_IN_PORT                  PORT_I2S_DATA1
#define DFU_CONTROL                             1
#if defined(PI_AI_MIC_DISABLE_HID_CONTROL) && PI_AI_MIC_DISABLE_HID_CONTROL
#define HID_CONTROL                             0
#else
#define HID_CONTROL                             1
#endif
#endif // End INT_DEVICE/INT_HOST/UA

#ifndef IO_EXPANDER_ENABLED
#define IO_EXPANDER_ENABLED (0)
#endif

#if !HID_CONTROL
#define IO_EXPANDER_ENABLED (0) // If HID is not enabled, disable IO Expander as well
#endif

#define appconfFAR_END_OUT_PORT                 PORT_I2S_DATA2 // Optional output pin when far-end DSP enabled on INT configs

#define appconfINT_PACKED_BIT_DEPTH             32 // INTegrated, not INTeger - as opposed to UA

#define appconfBCLK_NOMINAL_HZ                  (appconfLRCLK_NOMINAL_HZ * 64)
#define appconfMCLK_NOMINAL_HZ                  MIC_ARRAY_CONFIG_MCLK_FREQ
#ifndef appconfNUM_I2S_PINS_OUT                     // This handles the case for the extra I2S output pin
    #define appconfNUM_I2S_PINS_OUT             1
#endif

#define MIC_ARRAY_CONFIG_USE_DC_ELIMINATION     1
#define MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME      1

#define MIC_ARRAY_CONFIG_PDM_FREQ               3072000
#define MIC_ARRAY_CONFIG_MIC_IN_COUNT           8   // We use an 8b port
#define MIC_ARRAY_CONFIG_MIC_COUNT              4   // Of which we care about 4b
#define MIC_ARRAY_CONFIG_USE_DDR                0

#define appconfSHF_NOMINAL_HZ                   16000

// If unset, burn command will not actually enable burn
#define appconfBURN_MODE_SUPPORTED              1

/* App control plane configuration */
#define appconfI2C_MASTER_RATE_KHZ              100
#ifndef appconfSPI_CTRL_ENABLED
    #define appconfSPI_CTRL_ENABLED 0 //spi control is disabled by default
#endif
#ifndef appconfI2C_CTRL_ENABLED
    #define appconfI2C_CTRL_ENABLED 0 //i2c control is disabled by default
#endif

#ifndef appconfUSB_CTRL_ENABLED
    #define appconfUSB_CTRL_ENABLED 0
#endif

#define APP_CONTROL_TRANSPORT_COUNT ((appconfSPI_CTRL_ENABLED) + (appconfI2C_CTRL_ENABLED) + (appconfUSB_CTRL_ENABLED))

#define GPI_DEBOUNCE_MS                         25

/* Intertile port settings under RTOS */
#define appconfRPC_DATA_PORT                    10

/* FreeRTOS Task Priorities */
#define appconfSTARTUP_TASK_PRIORITY            (configMAX_PRIORITIES - 2)
#define appconfTEST_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define appconfRPC_TASK_PRIORITY                (configMAX_PRIORITIES - 2)

#define appconfDEVICE_CONTROL_I2C_PORT          1
#define appconfDEVICE_CONTROL_USB_PORT          2
#define appconfDEVICE_CONTROL_SPI_PORT          3
#define appconfDEVICE_CONTROL_GPIO_PORT         4
#define appconfUSB_MANAGER_SYNC_PORT            5
#define appconfUSB_COMMUNICATE_PLL_STATUS_PORT  6

/* Task Priorities */
#define appconfDEVICE_CONTROL_I2C_CLIENT_PRIORITY   (configMAX_PRIORITIES-1)
#define appconfDEVICE_CONTROL_SPI_CLIENT_PRIORITY   (configMAX_PRIORITIES-1)
#define appconfDEVICE_CONTROL_GPIO_CLIENT_PRIORITY   (configMAX_PRIORITIES-1)
#define appconfDEVICE_CONTROL_USB_CLIENT_PRIORITY   (configMAX_PRIORITIES-1)

/* XCORE resources */

/* Ports */
#define MIC_ARRAY_CONFIG_PORT_MCLK              PORT_PDM_MCLK
#define MIC_ARRAY_CONFIG_PORT_PDM_CLK           PORT_PDM_CLK
#define MIC_ARRAY_CONFIG_PORT_PDM_DATA          PORT_PDM_DATA
/* See also modules/fwk_xvf/modules/bsp/XK-VOICE-SQ66-EVK/xvf3800_qf60.xn for port map */

/* PI_AI_MIC custom hardware */
#ifndef PI_AI_MIC_BOARD
#define PI_AI_MIC_BOARD                         1
#endif
#ifndef PI_AI_MIC_PRE_RTOS_SPEAKER_SINE_INIT
#define PI_AI_MIC_PRE_RTOS_SPEAKER_SINE_INIT    0
#endif
#ifndef PI_AI_MIC_PRE_RTOS_SPEAKER_HW_INIT
#define PI_AI_MIC_PRE_RTOS_SPEAKER_HW_INIT      PI_AI_MIC_PRE_RTOS_SPEAKER_SINE_INIT
#endif
#ifndef PI_AI_MIC_LATE_SPEAKER_HW_INIT
#define PI_AI_MIC_LATE_SPEAKER_HW_INIT          PI_AI_MIC_PRE_RTOS_SPEAKER_HW_INIT
#endif
#ifndef PI_AI_MIC_PRE_RTOS_SPEAKER_AMP_FULL
#define PI_AI_MIC_PRE_RTOS_SPEAKER_AMP_FULL     0
#endif
#ifndef PI_AI_MIC_PRE_RTOS_SPEAKER_AMP_CTRL2_ONLY
#define PI_AI_MIC_PRE_RTOS_SPEAKER_AMP_CTRL2_ONLY 0
#endif
#ifndef PI_AI_MIC_DISABLE_RTOS_I2C_MASTER
#define PI_AI_MIC_DISABLE_RTOS_I2C_MASTER       1
#endif
#ifndef PI_AI_MIC_LED_HEARTBEAT_ENABLE
#define PI_AI_MIC_LED_HEARTBEAT_ENABLE          1
#endif
#ifndef PI_AI_MIC_DETECT_LED_MONITOR_ENABLE
#define PI_AI_MIC_DETECT_LED_MONITOR_ENABLE     0
#endif
#ifndef PI_AI_MIC_DETECT_LED_MONITOR_DIRECT_GPO_ENABLE
#define PI_AI_MIC_DETECT_LED_MONITOR_DIRECT_GPO_ENABLE 0
#endif
#ifndef PI_AI_MIC_DETECT_LED_MONITOR_DEVICE_CONTROL_ENABLE
#define PI_AI_MIC_DETECT_LED_MONITOR_DEVICE_CONTROL_ENABLE 0
#endif
#ifndef PI_AI_MIC_DETECT_LED_MONITOR_GPO_SERVICER_ENABLE
#define PI_AI_MIC_DETECT_LED_MONITOR_GPO_SERVICER_ENABLE 0
#endif
#ifndef PI_AI_MIC_DETECT_LED_MONITOR_GPO_SERVICER_POLL_ENABLE
#define PI_AI_MIC_DETECT_LED_MONITOR_GPO_SERVICER_POLL_ENABLE 1
#endif
#ifndef PI_AI_MIC_DETECT_LED_MONITOR_TIMER_POLL_ENABLE
#define PI_AI_MIC_DETECT_LED_MONITOR_TIMER_POLL_ENABLE 0
#endif
#ifndef PI_AI_MIC_DETECT_LED_MONITOR_START_DELAY_MS
#define PI_AI_MIC_DETECT_LED_MONITOR_START_DELAY_MS 3000
#endif
#ifndef PI_AI_MIC_DETECT_LED_MONITOR_PERIOD_MS
#define PI_AI_MIC_DETECT_LED_MONITOR_PERIOD_MS  100
#endif
#ifndef PI_AI_MIC_DETECT_LED_MONITOR_LOG_PERIOD
#define PI_AI_MIC_DETECT_LED_MONITOR_LOG_PERIOD 20
#endif
#ifndef PI_AI_MIC_DETECT_LED_DIAG_PULSE_ENABLE
#define PI_AI_MIC_DETECT_LED_DIAG_PULSE_ENABLE 0
#endif
#ifndef PI_AI_MIC_DETECT_LED_DIAG_PULSE_MS
#define PI_AI_MIC_DETECT_LED_DIAG_PULSE_MS 80
#endif
#ifndef PI_AI_MIC_BOOT_DETECT_LED_DEVICE_CONTROL_ENABLE
#define PI_AI_MIC_BOOT_DETECT_LED_DEVICE_CONTROL_ENABLE 0
#endif
#ifndef PI_AI_MIC_USB_HEADPHONE_ONLY
#define PI_AI_MIC_USB_HEADPHONE_ONLY            1
#endif
#ifndef PI_AI_MIC_HARD_SPEAKER_MUTE
#define PI_AI_MIC_HARD_SPEAKER_MUTE             0
#endif
#ifndef PI_AI_MIC_JACK_MONITOR_ENABLE
#define PI_AI_MIC_JACK_MONITOR_ENABLE           1
#endif
#ifndef PI_AI_MIC_JACK_AMP_CUT_MONITOR_ENABLE
#define PI_AI_MIC_JACK_AMP_CUT_MONITOR_ENABLE   0
#endif
#ifndef PI_AI_MIC_JACK_AMP_EDGE_MONITOR_ENABLE
#define PI_AI_MIC_JACK_AMP_EDGE_MONITOR_ENABLE  0
#endif
#ifndef PI_AI_MIC_FORCE_SPEAKER_ROUTE
#define PI_AI_MIC_FORCE_SPEAKER_ROUTE           0
#endif
#ifndef PI_AI_MIC_SPEAKER_USE_XMOS_STEREO_ROUTE
#define PI_AI_MIC_SPEAKER_USE_XMOS_STEREO_ROUTE 0
#endif
#ifndef PI_AI_MIC_USB_SPEAKER_MONO_MIX_ON_ROUTE
#define PI_AI_MIC_USB_SPEAKER_MONO_MIX_ON_ROUTE 0
#endif
#ifndef PI_AI_MIC_JACK_MONITOR_PERIOD_MS
#define PI_AI_MIC_JACK_MONITOR_PERIOD_MS        100
#endif
#ifndef PI_AI_MIC_JACK_MONITOR_START_DELAY_MS
#define PI_AI_MIC_JACK_MONITOR_START_DELAY_MS   1000
#endif
#ifndef PI_AI_MIC_JACK_MONITOR_DEBOUNCE_COUNT
#define PI_AI_MIC_JACK_MONITOR_DEBOUNCE_COUNT   3
#endif
#ifndef PI_AI_MIC_HEADPHONE_ANALOG_VOL
#define PI_AI_MIC_HEADPHONE_ANALOG_VOL          0x80
#endif
#ifndef PI_AI_MIC_HEADPHONE_MODE_SPEAKER_ANALOG_VOL
#define PI_AI_MIC_HEADPHONE_MODE_SPEAKER_ANALOG_VOL PI_AI_MIC_HEADPHONE_ANALOG_VOL
#endif
#ifndef PI_AI_MIC_HEADPHONE_DRIVER_REG
#define PI_AI_MIC_HEADPHONE_DRIVER_REG          0x06
#endif
#ifndef PI_AI_MIC_HEADPHONE_LINEOUT_CTRL
#define PI_AI_MIC_HEADPHONE_LINEOUT_CTRL        0x00
#endif
#ifndef PI_AI_MIC_DAC_DIGITAL_VOL
#define PI_AI_MIC_DAC_DIGITAL_VOL              0x00
#endif
#ifndef PI_AI_MIC_DAC_DAT_PATH_STEREO
#define PI_AI_MIC_DAC_DAT_PATH_STEREO          0xD4
#endif
#ifndef PI_AI_MIC_DAC_DAT_PATH_MONO_LEFT
#define PI_AI_MIC_DAC_DAT_PATH_MONO_LEFT       0xD8
#endif
#ifndef PI_AI_MIC_DAC_DAT_PATH_SPEAKER_DIFF
#define PI_AI_MIC_DAC_DAT_PATH_SPEAKER_DIFF    0x90
#endif
#ifndef PI_AI_MIC_SPEAKER_ANALOG_VOL
#define PI_AI_MIC_SPEAKER_ANALOG_VOL            0xA8
#endif
#ifndef PI_AI_MIC_SPEAKER_GENELEC_BIQUADS
#define PI_AI_MIC_SPEAKER_GENELEC_BIQUADS       0
#endif
#ifndef PI_AI_MIC_MIC_GEOMETRY_STRAP_ENABLE
#define PI_AI_MIC_MIC_GEOMETRY_STRAP_ENABLE     1
#endif
#ifndef PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
#define PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE 0
#endif
#ifndef PI_AI_MIC_FORCE_MIC_GEOMETRY_SQUARE
#define PI_AI_MIC_FORCE_MIC_GEOMETRY_SQUARE     0
#endif
#ifndef PI_AI_MIC_FORCE_MIC_GEOMETRY_LINE
#define PI_AI_MIC_FORCE_MIC_GEOMETRY_LINE       0
#endif
#ifndef PI_AI_MIC_MIC_DETECT_SQUARE_LEVEL
#define PI_AI_MIC_MIC_DETECT_SQUARE_LEVEL       1
#endif
#ifndef PI_AI_MIC_MIC_GEOMETRY_SAMPLE_COUNT
#define PI_AI_MIC_MIC_GEOMETRY_SAMPLE_COUNT     7
#endif
#ifndef PI_AI_MIC_MIC_GEOMETRY_SAMPLE_MIN_MATCH
#define PI_AI_MIC_MIC_GEOMETRY_SAMPLE_MIN_MATCH 6
#endif
#ifndef PI_AI_MIC_MIC_GEOMETRY_SAMPLE_INTERVAL_TICKS
#define PI_AI_MIC_MIC_GEOMETRY_SAMPLE_INTERVAL_TICKS 1000000u
#endif
#ifndef PI_AI_MIC_MIC_GEOMETRY_FALLBACK_SQUARE
#define PI_AI_MIC_MIC_GEOMETRY_FALLBACK_SQUARE  0
#endif
#ifndef PI_AI_MIC_SPEAKER_ENABLE_WHEN_NO_JACK
#define PI_AI_MIC_SPEAKER_ENABLE_WHEN_NO_JACK   (!PI_AI_MIC_USB_HEADPHONE_ONLY)
#endif
#ifndef PI_AI_MIC_DEFAULT_SPEAKER_ON_JACK_READ_FAIL
#define PI_AI_MIC_DEFAULT_SPEAKER_ON_JACK_READ_FAIL 0
#endif
#ifndef PI_AI_MIC_START_WITH_SPEAKER_ROUTE
#define PI_AI_MIC_START_WITH_SPEAKER_ROUTE      0
#endif
#ifndef PI_AI_MIC_RUNTIME_JACK_READ_FOR_AUDIO
#define PI_AI_MIC_RUNTIME_JACK_READ_FOR_AUDIO   0
#endif
#ifndef PI_AI_MIC_XSCOPE_ENABLE
#define PI_AI_MIC_XSCOPE_ENABLE                 0
#endif
#ifndef PI_AI_MIC_XSCOPE_MIC_CAPTURE_ENABLE
#define PI_AI_MIC_XSCOPE_MIC_CAPTURE_ENABLE     PI_AI_MIC_XSCOPE_ENABLE
#endif
#define PI_AI_MIC_XSCOPE_MIC_CAPTURE_START      4096
#define PI_AI_MIC_XSCOPE_MIC_CAPTURE_COUNT      8192
#define PI_AI_MIC_XSCOPE_MIC_CAPTURE_DECIM      4
#ifndef PI_AI_MIC_TEST_SINE_ENABLE
#define PI_AI_MIC_TEST_SINE_ENABLE              0
#endif
#ifndef PI_AI_MIC_USB_IN_TEST_SINE_ENABLE
#define PI_AI_MIC_USB_IN_TEST_SINE_ENABLE       0
#endif
#ifndef PI_AI_MIC_SINE_GAIN
#define PI_AI_MIC_SINE_GAIN                     16
#endif
#define PI_AI_MIC_I2C_WAVEFORM_TEST_ENABLE      0
#define PI_AI_MIC_I2C_ELECTRICAL_TEST_ENABLE    0
#define PI_AI_MIC_I2C_ELECTRICAL_TILE0_ENABLE   0
#define PI_AI_MIC_I2C_ELECTRICAL_TILE1_ENABLE   0
#define PI_AI_MIC_I2C_FORCED_PROBE_TEST_ENABLE  0
#define PI_AI_MIC_I2C_FORCED_PROBE_TILE0_ENABLE 0
#define PI_AI_MIC_I2C_FORCED_PROBE_TILE1_ENABLE 0
#define PI_AI_MIC_I2C_PORT_SCAN_TEST_ENABLE     0
#define PI_AI_MIC_I2C_PORT_SCAN_TILE0_ENABLE    0
#define PI_AI_MIC_I2C_PORT_SCAN_TILE1_ENABLE    0
#define PI_AI_MIC_I2C_PIN_LOCATOR_TEST_ENABLE   0
#define PI_AI_MIC_I2C_PIN_LOCATOR_TILE0_ENABLE  0
#define PI_AI_MIC_I2C_PIN_LOCATOR_TILE1_ENABLE  0
#ifndef PI_AI_MIC_JACK_DET_PULL_MODE
#define PI_AI_MIC_JACK_DET_PULL_MODE            1
#endif
#define PI_AI_MIC_JACK_INSERTED_LEVEL           1
#ifndef PI_AI_MIC_DOA_NEOPIXEL_ENABLE
#define PI_AI_MIC_DOA_NEOPIXEL_ENABLE           0
#endif
#ifndef PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT
#define PI_AI_MIC_DOA_NEOPIXEL_LED_COUNT        24
#endif
#ifndef PI_AI_MIC_DOA_NEOPIXEL_PERIOD_MS
#define PI_AI_MIC_DOA_NEOPIXEL_PERIOD_MS        100
#endif
#ifndef PI_AI_MIC_DOA_NEOPIXEL_BRIGHTNESS
#define PI_AI_MIC_DOA_NEOPIXEL_BRIGHTNESS       24
#endif
#ifndef PI_AI_MIC_DOA_NEOPIXEL_BOOT_ANIM_ENABLE
#define PI_AI_MIC_DOA_NEOPIXEL_BOOT_ANIM_ENABLE 1
#endif
#ifndef PI_AI_MIC_DOA_NEOPIXEL_BOOT_ANIM_FRAMES
#define PI_AI_MIC_DOA_NEOPIXEL_BOOT_ANIM_FRAMES 48
#endif
#ifndef PI_AI_MIC_DOA_NEOPIXEL_SELFTEST
#define PI_AI_MIC_DOA_NEOPIXEL_SELFTEST         0
#endif
#ifndef PI_AI_MIC_DOA_RAW_RING_ENABLE
#define PI_AI_MIC_DOA_RAW_RING_ENABLE            1
#endif
#ifndef PI_AI_MIC_DOA_RING_OFFSET_180
#define PI_AI_MIC_DOA_RING_OFFSET_180            0
#endif
#ifndef PI_AI_MIC_DOA_NEOPIXEL_REVERSE_ORDER
#define PI_AI_MIC_DOA_NEOPIXEL_REVERSE_ORDER    0
#endif
#ifndef PI_AI_MIC_DOA_NEOPIXEL_MIRROR_90_270
#define PI_AI_MIC_DOA_NEOPIXEL_MIRROR_90_270    0
#endif
#ifndef PI_AI_MIC_DOA_RING_OPPOSITE_FILTER_ENABLE
#define PI_AI_MIC_DOA_RING_OPPOSITE_FILTER_ENABLE 0
#endif
#ifndef PI_AI_MIC_DOA_RING_OPPOSITE_WINDOW_RAD
#define PI_AI_MIC_DOA_RING_OPPOSITE_WINDOW_RAD  0.523599f
#endif
#ifndef PI_AI_MIC_DOA_RING_OPPOSITE_STABLE_FRAMES
#define PI_AI_MIC_DOA_RING_OPPOSITE_STABLE_FRAMES 90
#endif
#ifndef PI_AI_MIC_DOA_AUTO_SELECTED_INDEX
#define PI_AI_MIC_DOA_AUTO_SELECTED_INDEX        3
#endif
#ifndef PI_AI_MIC_RAW_DOA_PCB_ORDER
#define PI_AI_MIC_RAW_DOA_PCB_ORDER              0
#endif
#ifndef PI_AI_MIC_RAW_DOA_DELAY_SIGN
#define PI_AI_MIC_RAW_DOA_DELAY_SIGN             0
#endif
#ifndef PI_AI_MIC_RAW_DOA_ABS_CORRELATION
#define PI_AI_MIC_RAW_DOA_ABS_CORRELATION        1
#endif
#ifndef PI_AI_MIC_DOA_NEOPIXEL_PORT
#define PI_AI_MIC_DOA_NEOPIXEL_PORT             PORT_GPI_1
#endif
#ifndef PI_AI_MIC_SHF_MIC_REMAP_ENABLE
#define PI_AI_MIC_SHF_MIC_REMAP_ENABLE          0
#endif
#ifndef PI_AI_MIC_SHF_MIC_REMAP_SQUARE_ONLY
#define PI_AI_MIC_SHF_MIC_REMAP_SQUARE_ONLY     0
#endif
#ifndef PI_AI_MIC_SHF_MIC_REMAP_0
#define PI_AI_MIC_SHF_MIC_REMAP_0               0
#endif
#ifndef PI_AI_MIC_SHF_MIC_REMAP_1
#define PI_AI_MIC_SHF_MIC_REMAP_1               1
#endif
#ifndef PI_AI_MIC_SHF_MIC_REMAP_2
#define PI_AI_MIC_SHF_MIC_REMAP_2               2
#endif
#ifndef PI_AI_MIC_SHF_MIC_REMAP_3
#define PI_AI_MIC_SHF_MIC_REMAP_3               3
#endif

/* Direct XMOS GPO bit assignments on GPO_TILE_0_PORT_8C. */
#define GPO_DAC_RST_N_PIN                       3
#define GPO_SQ_NLIN_PIN                         4
#define GPO_INT_N_PIN                           5
#define GPO_LED_RED_PIN                         6
#define GPO_LED_GREEN_PIN                       7

/* PCAL6408A bit assignments from the PI_AI_MIC schematic. */
#define PCAL_XVF_RST_N_PIN                      0
#define PCAL_INT_N_PIN                          1
#define PCAL_DAC_RST_N_PIN                      2
#define PCAL_BOOT_SEL_PIN                       3
#define PCAL_AMP_CTRL1_PIN                      4
#define PCAL_JACK_DET_PIN                       5
#define PCAL_MIC_DETECT_PIN                     6
#define PCAL_AMP_CTRL2_PIN                      7

/* Clock blocks */
/* tile 0 */
#define MIC_ARRAY_CONFIG_CLOCK_BLOCK_A          XS1_CLKBLK_1 // Only one block used for SDR mics
#define SPI_SLAVE_CLKBLK                        XS1_CLKBLK_2
#define FLASH_CLKBLK                            XS1_CLKBLK_3
#define RESERVED_FOR_XUD_A                      XS1_CLKBLK_4 // Defined within lib_xud
#define RESERVED_FOR_XUD_B                      XS1_CLKBLK_5 // Defined within lib_xud

/* tile 1 */
#define MCLK_CLKBLK                             XS1_CLKBLK_1 // Used for sw_pll
#define UNUSED_CLKBLK_CC                        XS1_CLKBLK_2
#define I2S_CLKBLK                              XS1_CLKBLK_3
#define UNUSED_CLKBLK_EE                        XS1_CLKBLK_4
#define UNUSED_CLKBLK_FF                        XS1_CLKBLK_5

/* Software PLL settings for mclk recovery configurations */
/* see fractions.h and register_setup.h for other pll settings */
#define PLL_RATIO                   (MIC_ARRAY_CONFIG_MCLK_FREQ / appconfLRCLK_NOMINAL_HZ)
#define PLL_CONTROL_LOOP_COUNT_UA   80   // How many SoF periods per control loop iteration. Aim for ~100Hz
#define PLL_CONTROL_LOOP_COUNT_INT  512  // How many refclk ticks (LRCLK) per control loop iteration. Aim for ~100Hz
#define PLL_PPM_RANGE               1000 // Max allowable diff in clk count. For the PID constants we
                                         // have chosen, this number should be larger than the number
                                         // of elements in the look up table as the clk count diff is
                                         // added to the LUT index with a multiplier of 1. Only used for INT mclkless


// Max sample delay on reference signal
#define appconfMAX_SYS_DELAY                    256
// Max delay on the microphones, note that there are 4 mics so this has
// potentially significant memory impact.
#define appconfMAX_MIC_DELAY                    64

/* Logical cores */
#if ON_TILE(0)
/*  Bare metal task summary
    3 HP for PP
    1 NP for mic_array
    2 NP for USB XUD and buffer (UA only)

    FreeRTOS logical cores
    4 NP for INT (one or two of which will be a slave control peripheral)
    1 NP for UA
*/
#if (appconfUSB_ENABLED == 0)
#define appconfNUM_FREE_RTOS_CORES              4
#else
#define appconfNUM_FREE_RTOS_CORES              2
#endif
#define appconfTOTAL_HEAP_SIZE                  (23 * 1024)

#elif ON_TILE(1)
/*  Bare metal task summary
    4 HP for AEC
    1 NP for i2s/audio
    1 NP for customer DSP
    1 spare

    FreeRTOS logical cores
    1 NP
*/
#define appconfNUM_FREE_RTOS_CORES              1
#if IO_EXPANDER_ENABLED
    #define appconfTOTAL_HEAP_SIZE                  (24 * 1024)
#else
    #define appconfTOTAL_HEAP_SIZE                  (22 * 1024)
#endif
#ifndef RESERVE_USER_MEMORY
#error
#endif

#if RESERVE_USER_MEMORY
#define USER_MEMORY_RESERVED_BYTES              (17 * 1024) // Placeholder for user DSP and control code. See https://github.com/xmos/sw_xvf3800/issues/74
#else
#define USER_MEMORY_RESERVED_BYTES              1  // used as array size so must be non-zero
#endif

#else
#error Not configured for more than 2 tiles
#endif

// Check the rate matches with BeClear at compile time
// This macro looks odd but it is the only way to check a float value at compile time. Credit: Daniel and the Linux kernel
#define STATIC_INT_FLOAT_EQUALITY_CHECK(I, V)    typedef char assert_var[2*!!(I == V)-1];
STATIC_INT_FLOAT_EQUALITY_CHECK(appconfSHF_NOMINAL_HZ, BECLEAR_SAMPLE_FREQUENCY)

#ifndef appconfUSB_AUDIO_SAMPLE_RATE
#define appconfUSB_AUDIO_SAMPLE_RATE appconfUSB_IN_OUT_NOMINAL_HZ
#define appconfAUDIO_PIPELINE_FRAME_ADVANCE     256
#endif


#include "aec_cmds.h"
#define SHF_AEC_FAR_EXTGAIN_CMD BECLEAR_SUPERHANDSFREE_AEC_FAR_EXTGAIN // This command is internally programmed in tud_audio_set_req_entity_cb() when the external gain is applied by the host on the ref signal.
// The command ID define is different between the WB and SWB Beclear versions, so we declare a define that is accessed by tud_audio_set_req_entity_cb() in fwk_xvf to get the
// correct command ID define depending whether we're running the WB or SWB application.

#ifndef PI_AI_MIC_AEC_FAR_EXTGAIN_USB_OFFSET_DB
#define PI_AI_MIC_AEC_FAR_EXTGAIN_USB_OFFSET_DB 0.0f
#endif

#endif /* APP_CONF_H_ */
