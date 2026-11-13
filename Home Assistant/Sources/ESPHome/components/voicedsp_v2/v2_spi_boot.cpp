#include "v2_component.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "esphome/core/log.h"

namespace esphome::voicedsp_v2 {
// Same pins, PCAL6416 registers and block timing as the validated V2 RAM bridge.
bool VoiceDSPV2::boot_spi_() {
  static const char *TAG = "v2_spi_boot";
  spi_status_.store(-1);
  spi_progress_.store(0);
  if (!spi_image_ || spi_size_ != 864256 || spi_size_ % 4096 || clocks_ready_.load()) return false;
  auto read = [this](uint8_t reg, uint8_t &value) {
    return transfer_(this, 0x20, &reg, 1, &value, 1) == 0;
  };
  auto write = [this](uint8_t reg, uint8_t value) {
    const uint8_t bytes[]{reg, value};
    return transfer_(this, 0x20, bytes, 2, nullptr, 0) == 0;
  };
  uint8_t config, output, input;
  if (!read(6, config) || !read(2, output)) return false;
  // P0 XMOS reset, P2 DAC reset, P3 BOOT_SEL, P4 external amp. P7 stays input.
  output = (output & ~0x15u) | 0x08u;
  config = (config & ~0x1du) | 0x80u;
  if (!write(2, output) || !write(6, config) || !read(0, input) || (input & 0x11u)) return false;
  for (auto pin : {GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_15, GPIO_NUM_16}) gpio_reset_pin(pin);
  wait_(this, 5);
  spi_bus_config_t bus{};
  bus.mosi_io_num = 11; bus.miso_io_num = 13; bus.sclk_io_num = 12;
  bus.quadwp_io_num = -1; bus.quadhd_io_num = -1; bus.max_transfer_sz = 4096;
  if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;
  spi_device_interface_config_t dev{};
  dev.clock_speed_hz = 1000000; dev.mode = 0; dev.spics_io_num = 10; dev.queue_size = 1;
  spi_device_handle_t handle{};
  bool ok = spi_bus_add_device(SPI2_HOST, &dev, &handle) == ESP_OK;
  auto *block = static_cast<uint8_t *>(heap_caps_malloc(4096, MALLOC_CAP_DMA));
  ok = ok && block;
  if (ok) ok = write(6, config | 1u);
  wait_(this, 10);
  spi_status_.store(1);
  ESP_LOGI(TAG, "BEGIN pinned XMOS RAM image bytes=%u 1MHz mode0; QSPI unchanged", unsigned(spi_size_));
  for (size_t index = 0; ok && index < spi_size_ / 4096; ++index) {
    if (shutdown_.load()) { ok = false; break; }
    if (index == 1) wait_(this, 1);
    if (index == 105) wait_(this, 5);
    // Release straps before the last block, ahead of application I2C ownership.
    if (index + 1 == spi_size_ / 4096) ok = write(6, config | 0x0du);
    if (!ok) break;
    for (size_t j = 0; j < 4096; ++j) {
      uint8_t v = spi_image_[index * 4096 + j];
      v = ((v & 0xf0) >> 4) | ((v & 0x0f) << 4);
      v = ((v & 0xcc) >> 2) | ((v & 0x33) << 2);
      block[j] = ((v & 0xaa) >> 1) | ((v & 0x55) << 1);
    }
    spi_transaction_t tx{}; tx.length = 4096 * 8; tx.tx_buffer = block;
    ok = spi_device_transmit(handle, &tx) == ESP_OK;
    // Transfer is not proof of execution: reserve 100% for the host handshake.
    if (ok) spi_progress_.store(unsigned((index + 1) * 95 / (spi_size_ / 4096)));
    esp_rom_delay_us(5);
    if ((index + 1) % 32 == 0) wait_(this, 1);
  }
  if (block) heap_caps_free(block);
  if (handle) spi_bus_remove_device(handle);
  spi_bus_free(SPI2_HOST);
  for (auto pin : {GPIO_NUM_10, GPIO_NUM_11, GPIO_NUM_12, GPIO_NUM_13}) gpio_reset_pin(pin);
  if (!ok) {
    // Leave a failed partial image reset with both output stages disabled.
    write(2, output); write(6, config);
    spi_status_.store(-2);
    ESP_LOGE(TAG, "FAILED; no audio initialization");
    return false;
  }
  spi_status_.store(2);
  ESP_LOGI(TAG, "SENT; waiting for V2 host handshake (not yet execution proof)");
  wait_(this, 800);
  return true;
}
}  // namespace esphome::voicedsp_v2
