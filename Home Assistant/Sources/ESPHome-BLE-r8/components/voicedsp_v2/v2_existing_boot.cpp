#include "v2_component.h"
#include "v2_existing_boot.h"
#include "v2_boot_bus_guard.h"
#include "v2_aec_bus_clear.h"
#include "esphome/core/log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

namespace esphome::voicedsp_v2 {
int VoiceDSPV2::start_existing_(const vdsp_v2_host_ops &ops, const vdsp_v2_policy_limits &limits) {
  static const char *TAG = "v2_existing_boot";
  spi_status_.store(3); // Existing image, no SPI transfer performed.
  ESP_LOGI(TAG, "STARTUP_TIMING event=existing_probe uptime_ms=%lu", static_cast<unsigned long>(now_(this)));
  if (esp_reset_reason() == ESP_RST_POWERON && !existing_delay_done_) {
    // The USB window starts on first enumeration, later than ESP32 power-on.
    // Leave its master entirely alone during initial cold startup. This delay
    // never licenses a reset or flash; passive ownership checks still follow.
    ESP_LOGI(TAG, "Cold boot: leave USB recovery bus untouched for 7500ms");
    for (unsigned i = 0; i < 75 && !shutdown_.load(); ++i) wait_(this, 100);
  }
  auto read = [this](uint8_t reg, uint8_t &value) {
    return transfer_(this, 0x20, &reg, 1, &value, 1) == 0;
  };
  bool checked_reset = false, clear_attempted = false;
  auto read_pin = [](unsigned pin) { return gpio_get_level(pin ? GPIO_NUM_6 : GPIO_NUM_5) != 0; };
  auto delay_us = [](unsigned us) { esp_rom_delay_us(us); };
  const int result = wait_existing_boot(
      [this, &ops, &limits, &read, &checked_reset, &clear_attempted, &read_pin, &delay_us](bool resume) {
        const auto bus_state = observe_boot_bus(read_pin,
            []() { return static_cast<uint32_t>(esp_timer_get_time()); }, delay_us);
        if (bus_state == BootBusState::SDA_STALLED && !clear_attempted) {
          clear_attempted = true;
          const int cleared = v2_aec_bus_clear(read_pin,
              [](unsigned pin, bool release) { gpio_set_level(pin ? GPIO_NUM_6 : GPIO_NUM_5, release ? 1 : 0); }, delay_us);
          ESP_LOGI("v2_existing_boot", "EXISTING_BUS_CLEAR static_100ms rc=%d", cleared);
          if (cleared < 0) return -1;
        } else if (bus_state != BootBusState::QUIET) return -1;
        if (!checked_reset) {
          uint8_t config{}, output{}, input{};
          if (!read(6, config) || !read(2, output) || !read(0, input)) return -1;
          checked_reset = true;
          if (production_reset_held(config, output, input)) {
            const uint8_t release[]{6, 0xe7};
            if (transfer_(this, 0x20, release, 2, nullptr, 0)) return -54;
            ESP_LOGI("v2_existing_boot", "QSPI reset hold released; no image sent, no flash written");
            // Released cold QSPI startup has the same recovery ownership window.
            for (unsigned i = 0; i < 75 && !shutdown_.load(); ++i) wait_(this, 100);
            return -3;
          }
        }
        int error = vdsp_v2_host_init(&host_, &ops, 48000, &limits, true);
        if (!error) error = resume ? vdsp_v2_host_resume(&host_) : vdsp_v2_host_start(&host_, esp_random());
        if (resume) {
          startup_.resume_attempted = true;
          startup_.resume_result = error;
        } else {
          startup_.bootstrap_result = error;
          startup_.bootstrap_claimed = host_.claimed_bus;
        }
        if (!error) ESP_LOGI("v2_existing_boot", "EXISTING_READY path=%s protocol=0.6.0 exact_build=unverified spi_bytes=0",
                            resume ? "runtime_resume" : "bootstrap");
        return error;
      },
      [this]() { return host_.claimed_bus; },
      [this]() { return shutdown_.load(); },
      [this]() { return now_(this); },
      [this](unsigned ms) { wait_(this, ms); }, 18000);
  if (result) {
    spi_status_.store(-54);
    ESP_LOGE(TAG, "Existing image unavailable rc=%d claimed=%d; no implicit reset, SPI or QSPI write", result, host_.claimed_bus);
  }
  return result;
}
}  // namespace esphome::voicedsp_v2
