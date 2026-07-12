/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "usb_descriptors.h"
#include "tusb.h"
#include "class/dfu/dfu_device.h"
#include "device_control_usb.h"
#include "tile_common.h" // For get_usb_bit_depth()
#include "hid_telephony_device.h"

#define NUM_CONFIGURATIONS  0x1

#if appconfUSB_ENABLED

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
#if defined(PI_AI_MIC_USB_PROCESSED_2CH) && PI_AI_MIC_USB_PROCESSED_2CH
#define PI_AI_MIC_USB_PRODUCT_ID PI_AI_MIC_USB_PROCESSED_2CH_PRODUCT_ID
#if defined(PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE) && PI_AI_MIC_RUNTIME_GEOMETRY_PROFILE_ENABLE
#if appconfUSB_IN_OUT_NOMINAL_HZ == 48000
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Assistant Auto"
#else
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC 16k Auto Geometry"
#endif
#elif defined(PI_AI_MIC_KEYBOARD_SHORTCUT_PERIODIC_TEST_ENABLE) && PI_AI_MIC_KEYBOARD_SHORTCUT_PERIODIC_TEST_ENABLE
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Assistant Auto10s"
#elif defined(PI_AI_MIC_USER_BUTTON_KEYBOARD_ENABLE) && PI_AI_MIC_USER_BUTTON_KEYBOARD_ENABLE
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Assistant Shortcut"
#elif defined(PI_AI_MIC_ASSISTANT_PROFILE_SOFT) && PI_AI_MIC_ASSISTANT_PROFILE_SOFT
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Assistant Soft"
#elif defined(PI_AI_MIC_ASSISTANT_PROFILE_LINEIN_6DB) && PI_AI_MIC_ASSISTANT_PROFILE_LINEIN_6DB
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Assistant LineIn -6dB"
#elif defined(PI_AI_MIC_ASSISTANT_PROFILE_48K_LINEIN_6DB) && PI_AI_MIC_ASSISTANT_PROFILE_48K_LINEIN_6DB
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Assistant 48k LineIn -6dB"
#elif defined(PI_AI_MIC_ASSISTANT_PROFILE_48K_AUTOROUTE_LINEIN_6DB) && PI_AI_MIC_ASSISTANT_PROFILE_48K_AUTOROUTE_LINEIN_6DB
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Assistant 48k AutoRoute"
#elif defined(PI_AI_MIC_ASSISTANT_PROFILE_HPONLY_0DB) && PI_AI_MIC_ASSISTANT_PROFILE_HPONLY_0DB
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Assistant HP 0dB"
#elif defined(PI_AI_MIC_ASSISTANT_PROFILE_HPONLY) && PI_AI_MIC_ASSISTANT_PROFILE_HPONLY
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Assistant HP Only"
#elif defined(PI_AI_MIC_ASSISTANT_PROFILE_BALANCED) && PI_AI_MIC_ASSISTANT_PROFILE_BALANCED
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Assistant Balanced"
#elif defined(PI_AI_MIC_ASSISTANT_PROFILE_AGGRESSIVE) && PI_AI_MIC_ASSISTANT_PROFILE_AGGRESSIVE
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Assistant Aggressive"
#elif defined(PI_AI_MIC_ASSISTANT_PROFILE_HOT) && PI_AI_MIC_ASSISTANT_PROFILE_HOT
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Assistant Hot"
#elif defined(PI_AI_MIC_DOA_NEOPIXEL_ENABLE) && PI_AI_MIC_DOA_NEOPIXEL_ENABLE
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC BeClear DOA Ring"
#else
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC BeClear 2ch"
#endif
#elif defined(PI_AI_MIC_USB_PROCESSED_MIC_2CH) && PI_AI_MIC_USB_PROCESSED_MIC_2CH
#define PI_AI_MIC_USB_PRODUCT_ID PI_AI_MIC_USB_PROCESSED_MIC_2CH_PRODUCT_ID
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC BeClear 2ch"
#elif defined(PI_AI_MIC_USB_DEBUG_6CH) && PI_AI_MIC_USB_DEBUG_6CH
#define PI_AI_MIC_USB_PRODUCT_ID PI_AI_MIC_DEBUG_6CH_PRODUCT_ID
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC 6ch Debug"
#elif defined(PI_AI_MIC_USB_SPLIT_AUDIO) && PI_AI_MIC_USB_SPLIT_AUDIO
#define PI_AI_MIC_USB_PRODUCT_ID PI_AI_MIC_USB_SPLIT_PRODUCT_ID
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Split Speaker"
#elif defined(PI_AI_MIC_USB_MEDIA_2CH) && PI_AI_MIC_USB_MEDIA_2CH
#define PI_AI_MIC_USB_PRODUCT_ID PI_AI_MIC_USB_MEDIA_2CH_PRODUCT_ID
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC USB Speakers"
#elif defined(PI_AI_MIC_USB_RAW_MIC_2CH) && PI_AI_MIC_USB_RAW_MIC_2CH
#define PI_AI_MIC_USB_PRODUCT_ID PI_AI_MIC_USB_2CH_PRODUCT_ID
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Speaker 2ch"
#elif defined(PI_AI_MIC_USB_RAW_MIC_4CH) && PI_AI_MIC_USB_RAW_MIC_4CH
#define PI_AI_MIC_USB_PRODUCT_ID PI_AI_MIC_RAW4_SPEAKER_PRODUCT_ID
#if defined(PI_AI_MIC_USB_RAW_MIC_ONLY_DESCRIPTOR) && PI_AI_MIC_USB_RAW_MIC_ONLY_DESCRIPTOR
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Raw4 Mic"
#else
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC Raw4 Speaker"
#endif
#elif defined(PI_AI_MIC_BOARD) && PI_AI_MIC_BOARD
#define PI_AI_MIC_USB_PRODUCT_ID PRODUCT_ID
#define PI_AI_MIC_USB_PRODUCT_STR "PI AI MIC USB 2ch"
#else
#define PI_AI_MIC_USB_PRODUCT_ID PRODUCT_ID
#define PI_AI_MIC_USB_PRODUCT_STR PRODUCT_STR
#endif

#if defined(PI_AI_MIC_USB_DEBUG_6CH) && PI_AI_MIC_USB_DEBUG_6CH
#if defined(PI_AI_MIC_FORCE_MIC_GEOMETRY_SQUARE) && PI_AI_MIC_FORCE_MIC_GEOMETRY_SQUARE
#define PI_AI_MIC_USB_SERIAL_STR "PIAIMIC6S16"
#elif defined(PI_AI_MIC_FORCE_MIC_GEOMETRY_LINE) && PI_AI_MIC_FORCE_MIC_GEOMETRY_LINE
#define PI_AI_MIC_USB_SERIAL_STR "PIAIMIC6L16"
#else
#define PI_AI_MIC_USB_SERIAL_STR "PIAIMIC6D16"
#endif
#else
#define PI_AI_MIC_USB_SERIAL_STR SERIAL_NUMBER_STR
#endif

tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0201,   // For BOS descriptor! https://microchip.my.site.com/s/article/Does-a-USB2-1-Specification-Exist

    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = VENDOR_ID,
    .idProduct          = PI_AI_MIC_USB_PRODUCT_ID,
    .bcdDevice          = BCD_DEVICE,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = NUM_CONFIGURATIONS
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const* tud_descriptor_device_cb(void)
{
    return (uint8_t const*) &desc_device;
}

// MSOS 2.0 descriptor copied from the examples in https://github.com/xmos/xcore_iot/blob/develop/test/usb/tinyusb_demos/webusb_serial/src/usb_descriptors.c#L152
// and https://github.com/pololu/libusbp/blob/master/test/firmware/wixel/main.c#L460
#define MS_OS_20_DESC_BASE_LEN  0x0A + /* Header length */ \
                                0x08 + /* Length of configuration subset header */ \
                                0x08 + 0x14 + 0x84 /* Length of function subset for ITF_XMOS_DEV_CTRL */
#if DFU_CONTROL > 0
#define MS_OS_20_DESC_LEN  MS_OS_20_DESC_BASE_LEN + \
                           0x08 + 0x14 + 0x84 /* Length of function subset for ITF_NUM_DFU_MODE */
#else
#define MS_OS_20_DESC_LEN  MS_OS_20_DESC_BASE_LEN
#endif

#define REQUEST_GET_MS_DESCRIPTOR    0x20

// Microsoft OS 2.0 Descriptors, Table 8
#define MS_OS_20_DESCRIPTOR_INDEX 7

#define BOS_TOTAL_LEN      (TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

uint8_t const desc_bos[] =
{
  // total length, number of device caps
  TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 1),

  // Microsoft OS 2.0 descriptor
  TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, REQUEST_GET_MS_DESCRIPTOR)
};

// The descriptor below was created using the Composite Device example in https://github.com/hathach/tinyusb/discussions/823#discussioncomment-7473503
uint8_t const desc_ms_os_20[] =
{
  // Set header: length, type, windows version, total length
  U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

  // Configuration subset header: length, type, configuration index, reserved, configuration total length
  U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION), 0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A),

  // Function Subset header for ITF_XMOS_DEV_CTRL: length, type, first interface, reserved, subset length
  U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), ITF_XMOS_DEV_CTRL, 0, U16_TO_U8S_LE(0x08 + 0x14 + 0x84),
  // MS OS 2.0 Compatible ID descriptor for ITF_NUM_DFU_MODE: length, type, compatible ID, sub compatible ID
  U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // sub-compatible
  // MS OS 2.0 Registry property descriptor: length, type
  U16_TO_U8S_LE(0x84), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
  U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A), // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
  'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
  'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
  U16_TO_U8S_LE(0x0050), // wPropertyDataLength
  // bPropertyData: "{1D8E2449-1106-462A-A4A4-33BB83F38AFB}"
  // UUID generated using https://guidgenerator.com/
  '{', 0x00, '1', 0x00, 'D', 0x00, '8', 0x00, 'E', 0x00, '2', 0x00, '4', 0x00, '4', 0x00, '9', 0x00, '-', 0x00,
  '1', 0x00, '1', 0x00, '0', 0x00, '6', 0x00, '-', 0x00, '4', 0x00, '6', 0x00, '2', 0x00, 'A', 0x00, '-', 0x00,
  'A', 0x00, '4', 0x00, 'A', 0x00, '4', 0x00, '-', 0x00, '3', 0x00, '3', 0x00, 'B', 0x00, 'B', 0x00, '8', 0x00,
  '3', 0x00, 'F', 0x00, '3', 0x00, '8', 0x00, 'A', 0x00, 'F', 0x00, 'B', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00,

#if DFU_CONTROL > 0
  // Function Subset header for ITF_NUM_DFU_MODE: length, type, first interface, reserved, subset length
  U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), ITF_NUM_DFU_MODE, 0, U16_TO_U8S_LE(0x08 + 0x14 + 0x84),
  // MS OS 2.0 Compatible ID descriptor for ITF_NUM_DFU_MODE: length, type, compatible ID, sub compatible ID
  U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // sub-compatible
  // MS OS 2.0 Registry property descriptor: length, type
  U16_TO_U8S_LE(0x84), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
  U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A), // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
  'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
  'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
  U16_TO_U8S_LE(0x0050), // wPropertyDataLength
  // bPropertyData: "{D432C8B1-23B2-49E9-B5E9-EC6CAB04A194}".
  // UUID generated using https://guidgenerator.com/
  '{', 0x00, 'D', 0x00, '4', 0x00, '3', 0x00, '2', 0x00, 'C', 0x00, '8', 0x00, 'B', 0x00, '1', 0x00, '-', 0x00,
  '2', 0x00, '3', 0x00, 'B', 0x00, '2', 0x00, '-', 0x00, '4', 0x00, '9', 0x00, 'E', 0x00, '9', 0x00, '-', 0x00,
  'B', 0x00, '5', 0x00, 'E', 0x00, '9', 0x00, '-', 0x00, 'E', 0x00, 'C', 0x00, '6', 0x00, 'C', 0x00, 'A', 0x00,
  'B', 0x00, '0', 0x00, '4', 0x00, 'A', 0x00, '1', 0x00, '9', 0x00, '4', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00
#endif
};
uint8_t const * tud_descriptor_bos_cb(void)
{
  return desc_bos;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

// Number of Alternate Interface (each for 1 flash partition)
#define ALT_COUNT   2

#define FUNC_ATTRS (DFU_ATTR_CAN_UPLOAD | DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_WILL_DETACH | DFU_ATTR_MANIFESTATION_TOLERANT)

#define UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL (AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS)
#define TUD_AUDIO_DESC_FEATURE_UNIT_SIX_CHANNEL_LEN (6+(6+1)*4)
#define TUD_AUDIO_DESC_FEATURE_UNIT_SIX_CHANNEL(_unitid, _srcid, _ctrlch0master, _ctrlch1, _ctrlch2, _ctrlch3, _ctrlch4, _ctrlch5, _ctrlch6, _stridx) \
    TUD_AUDIO_DESC_FEATURE_UNIT_SIX_CHANNEL_LEN, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_FEATURE_UNIT, _unitid, _srcid, \
    U32_TO_U8S_LE(_ctrlch0master), U32_TO_U8S_LE(_ctrlch1), U32_TO_U8S_LE(_ctrlch2), U32_TO_U8S_LE(_ctrlch3), \
    U32_TO_U8S_LE(_ctrlch4), U32_TO_U8S_LE(_ctrlch5), U32_TO_U8S_LE(_ctrlch6), _stridx

#if CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX == 6
#define TUD_AUDIO_DESC_MIC_FEATURE_UNIT_LEN TUD_AUDIO_DESC_FEATURE_UNIT_SIX_CHANNEL_LEN
#define TUD_AUDIO_DESC_MIC_FEATURE_UNIT(_unitid, _srcid, _stridx) \
    TUD_AUDIO_DESC_FEATURE_UNIT_SIX_CHANNEL(_unitid, _srcid, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, _stridx)
#elif CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX == 4
#define TUD_AUDIO_DESC_MIC_FEATURE_UNIT_LEN TUD_AUDIO_DESC_FEATURE_UNIT_FOUR_CHANNEL_LEN
#define TUD_AUDIO_DESC_MIC_FEATURE_UNIT(_unitid, _srcid, _stridx) \
    TUD_AUDIO_DESC_FEATURE_UNIT_FOUR_CHANNEL(_unitid, _srcid, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, _stridx)
#elif CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX == 2
#define TUD_AUDIO_DESC_MIC_FEATURE_UNIT_LEN TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL_LEN
#define TUD_AUDIO_DESC_MIC_FEATURE_UNIT(_unitid, _srcid, _stridx) \
    TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL(_unitid, _srcid, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, _stridx)
#elif CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX == 1
#define TUD_AUDIO_DESC_MIC_FEATURE_UNIT_LEN TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN
#define TUD_AUDIO_DESC_MIC_FEATURE_UNIT(_unitid, _srcid, _stridx) \
    TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL(_unitid, _srcid, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, UAC2_FEATURE_UNIT_VOLUME_MUTE_CTRL, _stridx)
#else
#error Unsupported USB microphone channel count
#endif

#if defined(PI_AI_MIC_USB_SPLIT_AUDIO) && PI_AI_MIC_USB_SPLIT_AUDIO

#define SPLIT_SPK_INTERFACE_DESCRIPTORS_LENGTH ( \
        TUD_AUDIO_DESC_CLK_SRC_LEN + \
        TUD_AUDIO_DESC_INPUT_TERM_LEN + \
        TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL_LEN + \
        TUD_AUDIO_DESC_OUTPUT_TERM_LEN)

#define SPLIT_MIC_INTERFACE_DESCRIPTORS_LENGTH ( \
        TUD_AUDIO_DESC_CLK_SRC_LEN + \
        TUD_AUDIO_DESC_INPUT_TERM_LEN + \
        TUD_AUDIO_DESC_MIC_FEATURE_UNIT_LEN + \
        TUD_AUDIO_DESC_OUTPUT_TERM_LEN)

#define SPLIT_SPK_TOTAL_DESCRIPTORS_LENGTH ( \
        TUD_AUDIO_DESC_IAD_LEN + \
        TUD_AUDIO_DESC_STD_AC_LEN + \
        TUD_AUDIO_DESC_CS_AC_LEN + \
        SPLIT_SPK_INTERFACE_DESCRIPTORS_LENGTH + \
        TUD_AUDIO_DESC_STD_AS_INT_LEN + \
        TUD_AUDIO_DESC_STD_AS_INT_LEN + \
        TUD_AUDIO_DESC_CS_AS_INT_LEN + \
        TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN + \
        TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN + \
        TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN)

#define SPLIT_MIC_TOTAL_DESCRIPTORS_LENGTH ( \
        TUD_AUDIO_DESC_IAD_LEN + \
        TUD_AUDIO_DESC_STD_AC_LEN + \
        TUD_AUDIO_DESC_CS_AC_LEN + \
        SPLIT_MIC_INTERFACE_DESCRIPTORS_LENGTH + \
        TUD_AUDIO_DESC_STD_AS_INT_LEN + \
        TUD_AUDIO_DESC_STD_AS_INT_LEN + \
        TUD_AUDIO_DESC_CS_AS_INT_LEN + \
        TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN + \
        TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN + \
        TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN)

const size_t split_uac2_total_descriptors_length =
        SPLIT_SPK_TOTAL_DESCRIPTORS_LENGTH +
        SPLIT_MIC_TOTAL_DESCRIPTORS_LENGTH;

const uint16_t tud_audio_desc_lengths[CFG_TUD_AUDIO] = {
        SPLIT_SPK_TOTAL_DESCRIPTORS_LENGTH,
        SPLIT_MIC_TOTAL_DESCRIPTORS_LENGTH
};

const size_t uac2_as_output_desc_start_offset =
        TUD_CONFIG_DESC_LEN +
        TUD_AUDIO_DESC_IAD_LEN +
        TUD_AUDIO_DESC_STD_AC_LEN +
        TUD_AUDIO_DESC_CS_AC_LEN +
        SPLIT_SPK_INTERFACE_DESCRIPTORS_LENGTH;

const size_t uac2_as_input_desc_start_offset =
        TUD_CONFIG_DESC_LEN +
        SPLIT_SPK_TOTAL_DESCRIPTORS_LENGTH +
        TUD_AUDIO_DESC_IAD_LEN +
        TUD_AUDIO_DESC_STD_AC_LEN +
        TUD_AUDIO_DESC_CS_AC_LEN +
        SPLIT_MIC_INTERFACE_DESCRIPTORS_LENGTH;

const size_t uac2_bytes_per_sample_rx_offset =
        uac2_as_output_desc_start_offset
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_CS_AS_INT_LEN
        + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN
        - 2;

const size_t uac2_bytes_per_sample_tx_offset =
        uac2_as_input_desc_start_offset
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_CS_AS_INT_LEN
        + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN
        -2;

const size_t uac2_ep_out_sz_offset =
        uac2_as_output_desc_start_offset
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_CS_AS_INT_LEN
        + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN
        + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN
        - 3;

const size_t uac2_ep_in_sz_offset =
        uac2_as_input_desc_start_offset
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_CS_AS_INT_LEN
        + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN
        + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN
        -3;

#else

const size_t uac2_interface_descriptors_length =
        TUD_AUDIO_DESC_CLK_SRC_LEN
#if AUDIO_OUTPUT_ENABLED
        + TUD_AUDIO_DESC_INPUT_TERM_LEN
        + TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL_LEN
        + TUD_AUDIO_DESC_OUTPUT_TERM_LEN
#endif
#if AUDIO_INPUT_ENABLED
        + TUD_AUDIO_DESC_INPUT_TERM_LEN
        + TUD_AUDIO_DESC_MIC_FEATURE_UNIT_LEN
        + TUD_AUDIO_DESC_OUTPUT_TERM_LEN
#endif
        ;


const size_t uac2_total_descriptors_length =
        TUD_AUDIO_DESC_IAD_LEN +
        TUD_AUDIO_DESC_STD_AC_LEN +
        TUD_AUDIO_DESC_CS_AC_LEN +
        uac2_interface_descriptors_length +
#if AUDIO_OUTPUT_ENABLED
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_CS_AS_INT_LEN
        + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN
        + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN
        + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN
#endif
#if AUDIO_INPUT_ENABLED
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_CS_AS_INT_LEN
        + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN
        + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN
        + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN
#endif
        ;

// List of audio descriptor lengths which is required by audio driver - you need as many entries as CFG_TUD_AUDIO - unfortunately this is not possible to determine otherwise
const uint16_t tud_audio_desc_lengths[CFG_TUD_AUDIO] = {
        uac2_total_descriptors_length
};

// Offsets in the descriptor array that are used to configure the bit depth related fields in the descriptor at run time
const size_t uac2_as_output_desc_start_offset =
        TUD_CONFIG_DESC_LEN +
        TUD_AUDIO_DESC_IAD_LEN +
        TUD_AUDIO_DESC_STD_AC_LEN +
        TUD_AUDIO_DESC_CS_AC_LEN +
        uac2_interface_descriptors_length
        ;

const size_t uac2_as_input_desc_start_offset =
        uac2_as_output_desc_start_offset
#if AUDIO_OUTPUT_ENABLED
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_CS_AS_INT_LEN
        + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN
        + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN
        + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN
#endif
        ;

// Audio OP EP: Type I Format Type Descriptor, _subslotsize and _bitresolution
const size_t uac2_bytes_per_sample_rx_offset =
        uac2_as_output_desc_start_offset
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_CS_AS_INT_LEN
        + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN
        - 2;

// Audio IP EP: Type I Format Type Descriptor, _subslotsize and _bitresolution
const size_t uac2_bytes_per_sample_tx_offset =
        uac2_as_input_desc_start_offset
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_CS_AS_INT_LEN
        + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN
        -2;

// Audio OP EP: Standard AS Isochronous Audio Data Endpoint Descriptor, _maxEPsize
const size_t uac2_ep_out_sz_offset =
        uac2_as_output_desc_start_offset
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_CS_AS_INT_LEN
        + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN
        + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN
        - 3;

// Audio IP EP: Standard AS Isochronous Audio Data Endpoint Descriptor, _maxEPsize
const size_t uac2_ep_in_sz_offset =
        uac2_as_input_desc_start_offset
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_STD_AS_INT_LEN
        + TUD_AUDIO_DESC_CS_AS_INT_LEN
        + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN
        + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN
        -3;

#endif

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

uint8_t const desc_hid_report[] =
{
#if defined(PI_AI_MIC_HID_KEYBOARD_ONLY_ENABLE) && PI_AI_MIC_HID_KEYBOARD_ONLY_ENABLE
  TUD_HID_REPORT_DESC_KEYBOARD       ( HID_REPORT_ID(REPORT_ID_KEYBOARD_SHORTCUT      )),
#else
  TUD_HID_REPORT_DESC_MISC_BUTTONS   (HID_REPORT_ID(REPORT_ID_MISC_BUTTONS            )),
  TUD_HID_REPORT_DESC_VOLUME_BUTTONS (HID_REPORT_ID(REPORT_ID_VOLUME_BUTTONS            )),
  TUD_HID_REPORT_DESC_TEAMS_ASP      ( HID_REPORT_ID(REPORT_ID_TEAMS_ASP              )),
  TUD_HID_REPORT_DESC_TEAMS_BUTTON   ( HID_REPORT_ID(REPORT_ID_TEAMS_BUTTON           )),
#if defined(PI_AI_MIC_USER_BUTTON_KEYBOARD_ENABLE) && PI_AI_MIC_USER_BUTTON_KEYBOARD_ENABLE
  TUD_HID_REPORT_DESC_KEYBOARD       ( HID_REPORT_ID(REPORT_ID_KEYBOARD_SHORTCUT      )),
#endif
#endif
};

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
  (void) instance;
  return desc_hid_report;
}

#define DEV_CONTROL_DESC_STR_IDX ( 5 )

#if ( DFU_CONTROL == 0 )
  #ifdef TUD_DFU_DESC_LEN
    #undef TUD_DFU_DESC_LEN
  #endif
  #define TUD_DFU_DESC_LEN( _alt_count ) ( 0 )
  #define HID_DESC_STR_IDX ( DEV_CONTROL_DESC_STR_IDX + 1)
#else
  #define DFU_DESC_STR_IDX ( DEV_CONTROL_DESC_STR_IDX + 1 )
  #define HID_DESC_STR_IDX ( DFU_DESC_STR_IDX + 2 )
#endif


#if ( HID_CONTROL )
  #define HID_DESC_LEN ( TUD_HID_DESC_LEN )
#else
  #define HID_DESC_LEN ( 0 )
#endif



#if defined(PI_AI_MIC_USB_SPLIT_AUDIO) && PI_AI_MIC_USB_SPLIT_AUDIO
#define CONFIG_TOTAL_LEN        (TUD_CONFIG_DESC_LEN + \
                                 split_uac2_total_descriptors_length + \
                                 sizeof(tusb_desc_interface_t) /*For the device_control interface descriptor*/ + \
                                 HID_DESC_LEN /*HID*/ + \
                                 TUD_DFU_DESC_LEN(ALT_COUNT))
#else
#define CONFIG_TOTAL_LEN        (TUD_CONFIG_DESC_LEN + \
                                 (CFG_TUD_AUDIO * uac2_total_descriptors_length) + \
                                 sizeof(tusb_desc_interface_t) /*For the device_control interface descriptor*/ + \
                                 HID_DESC_LEN /*HID*/ + \
                                 TUD_DFU_DESC_LEN(ALT_COUNT))
#endif

#define AUDIO_INTERFACE_STRING_INDEX 4


#define AUDIO_TERM_TYPE_AEC_SPEAKERPHONE (0x405)

#if defined(PI_AI_MIC_USB_DEBUG_6CH) && PI_AI_MIC_USB_DEBUG_6CH
#define UAC2_AUDIO_FUNC_CATEGORY AUDIO_FUNC_IO_BOX
#define UAC2_CLOCK_ATTR AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK
#define UAC2_CLOCK_CTRL (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS)
#define UAC2_CLOCK_ASSOC UAC2_ENTITY_MIC_INPUT_TERMINAL
#define UAC2_SPK_OUTPUT_TERMINAL_TYPE AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER
#define UAC2_SPK_OUTPUT_ASSOC 0x00
#define UAC2_MIC_INPUT_TERMINAL_TYPE AUDIO_TERM_TYPE_IN_GENERIC_MIC
#define UAC2_MIC_INPUT_ASSOC UAC2_ENTITY_MIC_OUTPUT_TERMINAL
#define UAC2_MIC_INPUT_CTRL (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS)
#define UAC2_MIC_OUTPUT_ASSOC UAC2_ENTITY_MIC_INPUT_TERMINAL
#define UAC2_MIC_ISO_EP_ATTR (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_SYNCHRONOUS | TUSB_ISO_EP_ATT_DATA)
#define UAC2_MIC_ISO_LOCK_DELAY_UNIT AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC
#define UAC2_MIC_ISO_LOCK_DELAY 0x0003
#define UAC2_SPK_CHANNEL_CONFIG (AUDIO_CHANNEL_CONFIG_FRONT_LEFT | AUDIO_CHANNEL_CONFIG_FRONT_RIGHT)
#define UAC2_MIC_CHANNEL_CONFIG AUDIO_CHANNEL_CONFIG_NON_PREDEFINED
#elif defined(PI_AI_MIC_USB_RAW_MIC_2CH) && PI_AI_MIC_USB_RAW_MIC_2CH
#define UAC2_AUDIO_FUNC_CATEGORY AUDIO_FUNC_IO_BOX
#define UAC2_CLOCK_ATTR AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK
#define UAC2_CLOCK_CTRL (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS)
#define UAC2_CLOCK_ASSOC UAC2_ENTITY_MIC_INPUT_TERMINAL
#define UAC2_SPK_OUTPUT_TERMINAL_TYPE AUDIO_TERM_TYPE_OUT_GENERIC_SPEAKER
#define UAC2_SPK_OUTPUT_ASSOC 0x00
#define UAC2_MIC_INPUT_TERMINAL_TYPE AUDIO_TERM_TYPE_IN_GENERIC_MIC
#define UAC2_MIC_INPUT_ASSOC UAC2_ENTITY_MIC_OUTPUT_TERMINAL
#define UAC2_MIC_INPUT_CTRL (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS)
#define UAC2_MIC_OUTPUT_ASSOC UAC2_ENTITY_MIC_INPUT_TERMINAL
#define UAC2_MIC_ISO_EP_ATTR (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_SYNCHRONOUS | TUSB_ISO_EP_ATT_DATA)
#define UAC2_MIC_ISO_LOCK_DELAY_UNIT AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC
#define UAC2_MIC_ISO_LOCK_DELAY 0x0003
#define UAC2_SPK_CHANNEL_CONFIG (AUDIO_CHANNEL_CONFIG_FRONT_LEFT | AUDIO_CHANNEL_CONFIG_FRONT_RIGHT)
#define UAC2_MIC_CHANNEL_CONFIG (AUDIO_CHANNEL_CONFIG_FRONT_LEFT | AUDIO_CHANNEL_CONFIG_FRONT_RIGHT)
#elif defined(PI_AI_MIC_USB_RAW_MIC_4CH) && PI_AI_MIC_USB_RAW_MIC_4CH
#if defined(PI_AI_MIC_USB_RAW_MIC_ONLY_DESCRIPTOR) && PI_AI_MIC_USB_RAW_MIC_ONLY_DESCRIPTOR
#define UAC2_AUDIO_FUNC_CATEGORY AUDIO_FUNC_MICROPHONE
#else
#define UAC2_AUDIO_FUNC_CATEGORY AUDIO_FUNC_IO_BOX
#endif
#define UAC2_CLOCK_ATTR AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK
#define UAC2_CLOCK_CTRL (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS)
#define UAC2_CLOCK_ASSOC UAC2_ENTITY_MIC_INPUT_TERMINAL
#define UAC2_SPK_OUTPUT_TERMINAL_TYPE AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER
#define UAC2_SPK_OUTPUT_ASSOC 0x00
#define UAC2_MIC_INPUT_TERMINAL_TYPE AUDIO_TERM_TYPE_IN_GENERIC_MIC
#define UAC2_MIC_INPUT_ASSOC UAC2_ENTITY_MIC_OUTPUT_TERMINAL
#define UAC2_MIC_INPUT_CTRL (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS)
#define UAC2_MIC_OUTPUT_ASSOC UAC2_ENTITY_MIC_INPUT_TERMINAL
#define UAC2_MIC_ISO_EP_ATTR (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_SYNCHRONOUS | TUSB_ISO_EP_ATT_DATA)
#define UAC2_MIC_ISO_LOCK_DELAY_UNIT AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC
#define UAC2_MIC_ISO_LOCK_DELAY 0x0003
#define UAC2_SPK_CHANNEL_CONFIG (AUDIO_CHANNEL_CONFIG_FRONT_LEFT | AUDIO_CHANNEL_CONFIG_FRONT_RIGHT)
#define UAC2_MIC_CHANNEL_CONFIG AUDIO_CHANNEL_CONFIG_NON_PREDEFINED
#define UAC2_SPK_ISO_EP_ATTR (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ADAPTIVE | TUSB_ISO_EP_ATT_DATA)
#elif defined(PI_AI_MIC_USB_MEDIA_2CH) && PI_AI_MIC_USB_MEDIA_2CH
#if defined(PI_AI_MIC_USB_OUTPUT_ONLY_DESCRIPTOR) && PI_AI_MIC_USB_OUTPUT_ONLY_DESCRIPTOR
#define UAC2_AUDIO_FUNC_CATEGORY AUDIO_FUNC_DESKTOP_SPEAKER
#define UAC2_CLOCK_ATTR AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK
#define UAC2_CLOCK_CTRL (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS)
#define UAC2_CLOCK_ASSOC UAC2_ENTITY_SPK_INPUT_TERMINAL
#define UAC2_SPK_OUTPUT_TERMINAL_TYPE AUDIO_TERM_TYPE_OUT_GENERIC_SPEAKER
#define UAC2_SPK_OUTPUT_ASSOC 0x00
#define UAC2_MIC_INPUT_TERMINAL_TYPE AUDIO_TERM_TYPE_IN_GENERIC_MIC
#define UAC2_MIC_INPUT_ASSOC UAC2_ENTITY_MIC_OUTPUT_TERMINAL
#define UAC2_MIC_INPUT_CTRL (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS)
#define UAC2_MIC_OUTPUT_ASSOC UAC2_ENTITY_MIC_INPUT_TERMINAL
#else
#define UAC2_AUDIO_FUNC_CATEGORY AUDIO_FUNC_IO_BOX
#define UAC2_CLOCK_ATTR AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK
#define UAC2_CLOCK_CTRL (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS)
#define UAC2_CLOCK_ASSOC UAC2_ENTITY_MIC_INPUT_TERMINAL
#define UAC2_SPK_OUTPUT_TERMINAL_TYPE AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER
#define UAC2_SPK_OUTPUT_ASSOC 0x00
#define UAC2_MIC_INPUT_TERMINAL_TYPE AUDIO_TERM_TYPE_IN_GENERIC_MIC
#define UAC2_MIC_INPUT_ASSOC UAC2_ENTITY_MIC_OUTPUT_TERMINAL
#define UAC2_MIC_INPUT_CTRL (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS)
#define UAC2_MIC_OUTPUT_ASSOC UAC2_ENTITY_MIC_INPUT_TERMINAL
#endif
#define UAC2_MIC_ISO_EP_ATTR (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_SYNCHRONOUS | TUSB_ISO_EP_ATT_DATA)
#define UAC2_MIC_ISO_LOCK_DELAY_UNIT AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC
#define UAC2_MIC_ISO_LOCK_DELAY 0x0003
#define UAC2_SPK_CHANNEL_CONFIG (AUDIO_CHANNEL_CONFIG_FRONT_LEFT | AUDIO_CHANNEL_CONFIG_FRONT_RIGHT)
#define UAC2_MIC_CHANNEL_CONFIG (AUDIO_CHANNEL_CONFIG_FRONT_LEFT | AUDIO_CHANNEL_CONFIG_FRONT_RIGHT)
#define UAC2_SPK_ISO_EP_ATTR (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ADAPTIVE | TUSB_ISO_EP_ATT_DATA)
#else
#define UAC2_AUDIO_FUNC_CATEGORY AUDIO_FUNC_OTHER
#define UAC2_CLOCK_ATTR AUDIO_CLOCK_SOURCE_ATT_INT_PRO_CLK
#define UAC2_CLOCK_CTRL ((AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_VAL_POS) | (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS))
#define UAC2_CLOCK_ASSOC 0x00
#define UAC2_SPK_OUTPUT_TERMINAL_TYPE AUDIO_TERM_TYPE_AEC_SPEAKERPHONE
#define UAC2_SPK_OUTPUT_ASSOC UAC2_ENTITY_MIC_INPUT_TERMINAL
#define UAC2_MIC_INPUT_TERMINAL_TYPE AUDIO_TERM_TYPE_AEC_SPEAKERPHONE
#define UAC2_MIC_INPUT_ASSOC UAC2_ENTITY_SPK_OUTPUT_TERMINAL
#define UAC2_MIC_INPUT_CTRL AUDIO_CTRL_NONE
#define UAC2_MIC_OUTPUT_ASSOC 0x00
#define UAC2_MIC_ISO_EP_ATTR (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_SYNCHRONOUS | TUSB_ISO_EP_ATT_DATA)
#define UAC2_MIC_ISO_LOCK_DELAY_UNIT AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC
#define UAC2_MIC_ISO_LOCK_DELAY 0x0003
#define UAC2_SPK_CHANNEL_CONFIG AUDIO_CHANNEL_CONFIG_NON_PREDEFINED
#define UAC2_MIC_CHANNEL_CONFIG AUDIO_CHANNEL_CONFIG_NON_PREDEFINED
#endif

#ifndef UAC2_SPK_ISO_EP_ATTR
#define UAC2_SPK_ISO_EP_ATTR (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_SYNCHRONOUS | TUSB_ISO_EP_ATT_DATA)
#endif

#if defined(PI_AI_MIC_USB_SPLIT_AUDIO) && PI_AI_MIC_USB_SPLIT_AUDIO

uint8_t const desc_configuration[] = {
    // Interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 400),

    /* Speaker USB Audio function: 2ch host-to-device stream. */
    TUD_AUDIO_DESC_IAD(/*_firstitfs*/ ITF_NUM_AUDIO_CONTROL, /*_nitfs*/ 0x02, /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_STD_AC(/*_itfnum*/ ITF_NUM_AUDIO_CONTROL, /*_nEPs*/ 0x00, /*_stridx*/ AUDIO_INTERFACE_STRING_INDEX),
    TUD_AUDIO_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO_FUNC_DESKTOP_SPEAKER, /*_totallen*/ SPLIT_SPK_INTERFACE_DESCRIPTORS_LENGTH, /*_ctrl*/ AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS),
    TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_SPK_CLOCK, /*_attr*/ AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK, /*_ctrl*/ (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), /*_assocTerm*/ UAC2_ENTITY_SPK_INPUT_TERMINAL,  /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_clkid*/ UAC2_ENTITY_SPK_CLOCK, /*_nchannelslogical*/ CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX, /*_channelcfg*/ (AUDIO_CHANNEL_CONFIG_FRONT_LEFT | AUDIO_CHANNEL_CONFIG_FRONT_RIGHT), /*_idxchannelnames*/ 0x00, /*_ctrl*/ AUDIO_CTRL_NONE, /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL(/*_unitid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrlch0master*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch1*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch2*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER, /*_assocTerm*/ 0x00, /*_srcid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_SPK_CLOCK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ ITF_NUM_AUDIO_STREAMING_SPK, /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ ITF_NUM_AUDIO_STREAMING_SPK, /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX, /*_channelcfg*/ (AUDIO_CHANNEL_CONFIG_FRONT_LEFT | AUDIO_CHANNEL_CONFIG_FRONT_RIGHT), /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_TYPE_I_FORMAT(CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_N_BITS_PER_SAMPLE_RX),
    TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ EP_NUM_AUDIO, /*_attr*/ UAC2_SPK_ISO_EP_ATTR, /*_maxEPsize*/ CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ, /*_interval*/ (CFG_TUSB_RHPORT0_MODE & OPT_MODE_HIGH_SPEED) ? UAC2_AUDIO_EP_BINTERVAL : 0x01),
    TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0003),

    /* Microphone USB Audio function: 4ch device-to-host stream. */
    TUD_AUDIO_DESC_IAD(/*_firstitfs*/ ITF_NUM_AUDIO_CONTROL_MIC, /*_nitfs*/ 0x02, /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_STD_AC(/*_itfnum*/ ITF_NUM_AUDIO_CONTROL_MIC, /*_nEPs*/ 0x00, /*_stridx*/ AUDIO_INTERFACE_STRING_INDEX),
    TUD_AUDIO_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO_FUNC_MICROPHONE, /*_totallen*/ SPLIT_MIC_INTERFACE_DESCRIPTORS_LENGTH, /*_ctrl*/ AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS),
    TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_MIC_CLOCK, /*_attr*/ AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK, /*_ctrl*/ (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), /*_assocTerm*/ UAC2_ENTITY_MIC_INPUT_TERMINAL,  /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_clkid*/ UAC2_ENTITY_MIC_CLOCK, /*_nchannelslogical*/ CFG_TUD_AUDIO_FUNC_2_N_CHANNELS_TX, /*_channelcfg*/ UAC2_MIC_CHANNEL_CONFIG, /*_idxchannelnames*/ 0x00, /*_ctrl*/ AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS, /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_MIC_FEATURE_UNIT(/*_unitid*/ UAC2_ENTITY_MIC_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_srcid*/ UAC2_ENTITY_MIC_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_MIC_CLOCK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ ITF_NUM_AUDIO_STREAMING_MIC, /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ ITF_NUM_AUDIO_STREAMING_MIC, /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ CFG_TUD_AUDIO_FUNC_2_N_CHANNELS_TX, /*_channelcfg*/ UAC2_MIC_CHANNEL_CONFIG, /*_stridx*/ 0x00),
    TUD_AUDIO_DESC_TYPE_I_FORMAT(CFG_TUD_AUDIO_FUNC_2_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_2_N_BITS_PER_SAMPLE_TX),
    TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ 0x80 | EP_NUM_AUDIO, /*_attr*/ (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_SYNCHRONOUS | TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ CFG_TUD_AUDIO_FUNC_2_EP_IN_SZ, /*_interval*/ (CFG_TUSB_RHPORT0_MODE & OPT_MODE_HIGH_SPEED) ? UAC2_AUDIO_EP_BINTERVAL : 0x01),
    TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0003),

    // Interface number, string index
    TUD_XMOS_DEVICE_CONTROL_DESCRIPTOR(ITF_XMOS_DEV_CTRL, DEV_CONTROL_DESC_STR_IDX),

#if (0 < DFU_CONTROL)
    // Interface number, Alternate count, starting string index, attributes, detach timeout, transfer size
    TUD_DFU_DESCRIPTOR(ITF_NUM_DFU_MODE, ALT_COUNT, DFU_DESC_STR_IDX, FUNC_ATTRS, 1000, CFG_TUD_DFU_XFER_BUFSIZE),
#endif

#if HID_CONTROL
    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, HID_DESC_STR_IDX, 0, sizeof(desc_hid_report), 0x80 | EP_NUM_HID, CFG_TUD_HID_EP_BUFSIZE, HID_IN_EP_bInterval),
#endif
};

#else

uint8_t const desc_configuration[] = {
    // Interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 400),

    /* Standard Interface Association Descriptor (IAD) */
    TUD_AUDIO_DESC_IAD(/*_firstitfs*/ ITF_NUM_AUDIO_CONTROL, /*_nitfs*/ 1+AUDIO_OUTPUT_ENABLED+AUDIO_INPUT_ENABLED, /*_stridx*/ 0x00),
    /* Standard AC Interface Descriptor(4.7.1) */
    TUD_AUDIO_DESC_STD_AC(/*_itfnum*/ ITF_NUM_AUDIO_CONTROL, /*_nEPs*/ 0x00, /*_stridx*/ AUDIO_INTERFACE_STRING_INDEX),
    /* Class-Specific AC Interface Header Descriptor(4.7.2) */
    TUD_AUDIO_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ UAC2_AUDIO_FUNC_CATEGORY, /*_totallen*/ uac2_interface_descriptors_length, /*_ctrl*/ AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS),
    /* Clock Source Descriptor(4.7.2.1) */
    TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_CLOCK, /*_attr*/ UAC2_CLOCK_ATTR, /*_ctrl*/ UAC2_CLOCK_CTRL, /*_assocTerm*/ UAC2_CLOCK_ASSOC,  /*_stridx*/ 0x00),


#if AUDIO_OUTPUT_ENABLED
    /* Input Terminal Descriptor(4.7.2.4) */
    TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX, /*_channelcfg*/ UAC2_SPK_CHANNEL_CONFIG, /*_idxchannelnames*/ 0x00, /*_ctrl*/ AUDIO_CTRL_NONE, /*_stridx*/ 0x00),

    /* Feature Unit Descriptor(4.7.2.8) */
    TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL(/*_unitid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrlch0master*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch1*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch2*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_stridx*/ 0x00),

    /* Output Terminal Descriptor(4.7.2.5) */
    TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_OUTPUT_TERMINAL, /*_termtype*/ UAC2_SPK_OUTPUT_TERMINAL_TYPE, /*_assocTerm*/ UAC2_SPK_OUTPUT_ASSOC, /*_srcid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_stridx*/ 0x00),
#endif


#if AUDIO_INPUT_ENABLED
    /* Input Terminal Descriptor(4.7.2.4) */
    TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_termtype*/ UAC2_MIC_INPUT_TERMINAL_TYPE, /*_assocTerm*/ UAC2_MIC_INPUT_ASSOC, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX, /*_channelcfg*/ UAC2_MIC_CHANNEL_CONFIG, /*_idxchannelnames*/ 0x00, /*_ctrl*/ UAC2_MIC_INPUT_CTRL, /*_stridx*/ 0x00),

    /* Feature Unit Descriptor(4.7.2.8) */
    TUD_AUDIO_DESC_MIC_FEATURE_UNIT(/*_unitid*/ UAC2_ENTITY_MIC_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_stridx*/ 0x00),

    /* Output Terminal Descriptor(4.7.2.5) */
    TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ UAC2_MIC_OUTPUT_ASSOC, /*_srcid*/ UAC2_ENTITY_MIC_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_stridx*/ 0x00),
#endif

#if AUDIO_OUTPUT_ENABLED
    /* Standard AS Interface Descriptor(4.9.1) */
    /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ ITF_NUM_AUDIO_STREAMING_SPK, /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ 0x00),
    /* Standard AS Interface Descriptor(4.9.1) */
    /* Interface 1, Alternate 1 - alternate interface for data streaming */
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ ITF_NUM_AUDIO_STREAMING_SPK, /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ 0x00),
    /* Class-Specific AS Interface Descriptor(4.9.2) */
    TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX, /*_channelcfg*/ UAC2_SPK_CHANNEL_CONFIG, /*_stridx*/ 0x00),
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */
    TUD_AUDIO_DESC_TYPE_I_FORMAT(CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_N_BITS_PER_SAMPLE_RX),
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */
    TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ EP_NUM_AUDIO, /*_attr*/ UAC2_SPK_ISO_EP_ATTR, /*_maxEPsize*/ CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ, /*_interval*/ (CFG_TUSB_RHPORT0_MODE & OPT_MODE_HIGH_SPEED) ? UAC2_AUDIO_EP_BINTERVAL : 0x01),
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */
    TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0003),
#endif

#if AUDIO_INPUT_ENABLED
    /* Standard AS Interface Descriptor(4.9.1) */
    /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ ITF_NUM_AUDIO_STREAMING_MIC, /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ 0x00),
    /* Standard AS Interface Descriptor(4.9.1) */
    /* Interface 1, Alternate 1 - alternate interface for data streaming */
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ ITF_NUM_AUDIO_STREAMING_MIC, /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ 0x00),
    /* Class-Specific AS Interface Descriptor(4.9.2) */
    TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX, /*_channelcfg*/ UAC2_MIC_CHANNEL_CONFIG, /*_stridx*/ 0x00),
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */
    TUD_AUDIO_DESC_TYPE_I_FORMAT(CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_BITS_PER_SAMPLE_TX),
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */
    TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ 0x80 | EP_NUM_AUDIO, /*_attr*/ UAC2_MIC_ISO_EP_ATTR, /*_maxEPsize*/ CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ, /*_interval*/ (CFG_TUSB_RHPORT0_MODE & OPT_MODE_HIGH_SPEED) ? UAC2_AUDIO_EP_BINTERVAL : 0x01),
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */
    TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ UAC2_MIC_ISO_LOCK_DELAY_UNIT, /*_lockdelay*/ UAC2_MIC_ISO_LOCK_DELAY),
#endif

    // Interface number, string index
    TUD_XMOS_DEVICE_CONTROL_DESCRIPTOR(ITF_XMOS_DEV_CTRL, DEV_CONTROL_DESC_STR_IDX),

#if (0 < DFU_CONTROL)
    // Interface number, Alternate count, starting string index, attributes, detach timeout, transfer size
    TUD_DFU_DESCRIPTOR(ITF_NUM_DFU_MODE, ALT_COUNT, DFU_DESC_STR_IDX, FUNC_ATTRS, 1000, CFG_TUD_DFU_XFER_BUFSIZE),
#endif

#if HID_CONTROL
    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, HID_DESC_STR_IDX, 0, sizeof(desc_hid_report), 0x80 | EP_NUM_HID, CFG_TUD_HID_EP_BUFSIZE, HID_IN_EP_bInterval),
#endif
};

#endif

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const* tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index; // for multiple configurations
    return desc_configuration;
}

// device qualifier is mostly similar to device descriptor since we don't change configuration based on speed
tusb_desc_device_qualifier_t const desc_device_qualifier =
{
    .bLength            = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB             = 0x0200,

    .bDeviceClass       = TUSB_CLASS_UNSPECIFIED,
    .bDeviceSubClass    = TUSB_CLASS_UNSPECIFIED,
    .bDeviceProtocol    = TUSB_CLASS_UNSPECIFIED,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = NUM_CONFIGURATIONS,
    .bReserved          = 0x00
};


uint8_t const* tud_descriptor_device_qualifier_cb(void)
{
    return (uint8_t const*) &desc_device_qualifier;
}

uint8_t const* tud_descriptor_other_speed_configuration_cb(uint8_t index)
{
    (void) index; // for multiple configurations

    // Always return our normal descriptor
    return desc_configuration;
}


//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const *string_desc_arr[] = {
        (const char[]) {0x09, 0x04},    // 0: is supported language is English (0x0409)
        MANUFACTURER_STR,               // 1: Manufacturer
        PI_AI_MIC_USB_PRODUCT_STR,       // 2: Product
        PI_AI_MIC_USB_SERIAL_STR,       // 3: Serial, unique for PI_AI_MIC debug profiles to avoid stale Windows endpoint names
        PI_AI_MIC_USB_PRODUCT_STR,       // 4: Audio Interface
        CONTROL_INTERFACE_STR,          // 5: Vendor Interface
#if ( 0 < DFU_CONTROL )
        DFU_FACTORY_INTERFACE_STR,      // 6: DFU factory device
        DFU_UPGRADE_INTERFACE_STR,      // 7: DFU upgrade device
#endif
#if HID_CONTROL
        HID_INTERFACE_STR,              // 8: HID Interface
#endif
        };

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index,
                                         uint16_t langid)
{
    (void) langid;

    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if(index == 33)
        {
            /* Unified Communications Qualification (UCQ) descriptor,
            the set of fields sent to Teams by a telephony HID device indicating the capabilities supported by the device.*/
            const char* ucq_str =  "UCQ01001000001000";
            for (uint8_t i = 0; i < strlen(ucq_str); i++) {
                _desc_str[1 + i] = ucq_str[i];
            }
            _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * strlen(ucq_str) + 2);
            return _desc_str;
        }
        else
        {
            // Convert ASCII string into UTF-16
            if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
                return NULL;

            const char *str = string_desc_arr[index];

            // Cap at max char
            chr_count = strlen(str);
            if (chr_count > 31)
                chr_count = 31;

            for (uint8_t i = 0; i < chr_count; i++) {
                _desc_str[1 + i] = str[i];
            }
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}

/// @brief Modify the relevant descriptor fields to enumerate as a FullSpeed device
void configure_descriptors_for_full_speed()
{
    uint8_t bit_depth_in = 0;
    uint8_t bit_depth_out = 0;
    get_usb_bit_depth(&bit_depth_in, &bit_depth_out);

    // tud_descriptor_configuration_cb() returns a pointer to a const array of uint8_t.
    // Cast away the constancy to allow individual element modifications below.
    uint8_t *desc_ptr = (uint8_t*)tud_descriptor_configuration_cb(0);

    // The _maxEPsize and _interval fields need to be configured
    // in the Standard AS Isochronous Audio Data Endpoint Descriptor for both Audio IP and OP endpoints
    size_t offset;
#if AUDIO_OUTPUT_ENABLED
    // Audio OP EP: Standard AS Isochronous Audio Data Endpoint Descriptor, _maxEPsize
    offset = uac2_ep_out_sz_offset;
    uint16_t audio_func_1_ep_out_sz = (AUDIO_FRAMES_PER_USB_FRAME_FS * (bit_depth_out / 8) * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX);
    desc_ptr[offset] = TU_U16_LOW(audio_func_1_ep_out_sz);
    desc_ptr[offset+1] = TU_U16_HIGH(audio_func_1_ep_out_sz);
    desc_ptr[offset+2] = 1; // bInterval
#endif

#if AUDIO_INPUT_ENABLED
    // Audio IP EP: Standard AS Isochronous Audio Data Endpoint Descriptor, _maxEPsize
    offset = uac2_ep_in_sz_offset;
    uint16_t audio_func_1_ep_in_sz = (AUDIO_FRAMES_PER_USB_FRAME_FS * (bit_depth_in / 8) * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX);
    desc_ptr[offset] = TU_U16_LOW(audio_func_1_ep_in_sz);
    desc_ptr[offset+1] = TU_U16_HIGH(audio_func_1_ep_in_sz);
    desc_ptr[offset+2] = 1; // bInterval
#endif

}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    switch (request->bRequest)
    {
        case REQUEST_GET_MS_DESCRIPTOR:
            if ( request->wIndex == MS_OS_20_DESCRIPTOR_INDEX )
            {
                if (stage != CONTROL_STAGE_SETUP) return true; // nothing to with DATA & ACK stage
                // Send Microsoft OS 2.0 compatible descriptor
                return tud_control_xfer(rhport, request, (void*)(uintptr_t) desc_ms_os_20, MS_OS_20_DESC_LEN);
            }else
            {
                return false;
            }

        case 0: // The USB host driver functions in device_access_usb.c call libusb_control_transfer() with bRequest set to 0
            return device_control_usb_xfer(rhport, stage, request);

        default:
            return false;
    }
}

#else  // appconfUSB_ENABLED
// The various stub functions below have been included to avoid linker warnings
// for build configurations with appconfUSB_ENABLED == 0 or not defined
void configure_descriptors_for_full_speed()
{
}

const uint16_t tud_audio_desc_lengths[1] = {0};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index)
{
    return NULL;
}

uint8_t const* tud_descriptor_device_cb(void)
{
    return NULL;
}

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
    return NULL;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index,
                                         uint16_t langid)
{
    return NULL;
}

#endif  // appconfUSB_ENABLED
