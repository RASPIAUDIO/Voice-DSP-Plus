#include "v2_component.h"
#include "v2_qspi_program.h"
#include "v2_boot_bus_guard.h"
#include "esphome/core/log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "mbedtls/sha256.h"
#include <cstdio>
#include <cstring>

namespace esphome::voicedsp_v2 {
static const char *TAG = "v2_qspi_install";
static bool matches_sha(const uint8_t *data, size_t size, const char *expected) {
  uint8_t digest[32]; char hex[65];
  if (mbedtls_sha256(data, size, digest, 0)) return false;
  for (unsigned i = 0; i < 32; ++i) std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
  return std::strcmp(hex, expected) == 0;
}

std::string VoiceDSPV2::qspi_diagnostics() const {
  const int status = qspi_status_.load();
  static const char *names[]{"Idle - explicit install only", "Validating pinned bundle",
    "Loading RAM writer", "Waiting for writer", "Erasing boot partition",
    "Writing boot image", "Verifying full image", "Flash verified",
    "Flash verified - starting QSPI", "Flash verified - controller ready"};
  if (status >= 0 && status <= 9) return names[status];
  if (status == -111) return "Flash verified - runtime not ready; no automatic retry";
  char text[100];
  std::snprintf(text, sizeof text, "Install failed code=%d; no automatic retry", status);
  return text;
}

bool VoiceDSPV2::program_qspi_() {
  qspi_running_.store(true);
  struct Finish { std::atomic<bool> &flag; ~Finish() { flag.store(false); } } finish{qspi_running_};
  auto fail = [this](int error) {
    qspi_status_.store(error);
    ESP_LOGE(TAG, "QSPI_INSTALL_FAILED code=%d; request consumed, no retry", error);
    return false;
  };
  if (!qspi_image_ || qspi_size_ != 1818624 || !writer_image_ || writer_size_ != 24576 ||
      clocks_ready_.load() || need_clocks_.load()) return fail(-101);
  if (!matches_sha(qspi_image_, qspi_size_, "55d05d89b1f31df78019080e3e085dfeaceec044d30005b9ba63e38549845c51") ||
      !matches_sha(writer_image_, writer_size_, "08f2cdc8e81946b630c52761fdb16aebe8de524a3ef6dbf6c4759fa7673de851")) return fail(-102);

  // An interrupted request can be followed by a real cold boot. Do not race
  // the recovery master's USB window; only a known I2S identity allows reset.
  for (unsigned i = 0; i < 75; ++i) {
    if (shutdown_.load()) return fail(-103);
    wait_(this, 100);
  }
  auto quiet = observe_boot_bus(
      [](unsigned p) { return gpio_get_level(p ? GPIO_NUM_6 : GPIO_NUM_5) != 0; },
      []() { return uint32_t(esp_timer_get_time()); }, [](unsigned us) { esp_rom_delay_us(us); });
  if (quiet != BootBusState::QUIET) return fail(-104);
  uint8_t raw[16]{};
  bool identity = transfer_(this, 0x2c, nullptr, 0, raw, sizeof raw) == 0 &&
      !std::memcmp(raw, "V2BT\1\0\6\0", 8) && raw[8] == 48 &&
      vdsp_v2_boot_crc(raw, 14) == unsigned(raw[14] | (unsigned(raw[15]) << 8));
  if (!identity) {
    const uint8_t version_query[]{0x30, 0x80, 4}, record_query[]{0x30, 0x89, 17};
    uint8_t version[4]{}, record[17]{};
    identity = !transfer_(this, 0x2c, version_query, 3, version, 4) &&
        !std::memcmp(version, "\0\0\6\0", 4) &&
        !transfer_(this, 0x2c, record_query, 3, record, 17) && record[0] == 0;
    vdsp_v2_host_boot checked{};
    if (identity) {
      identity = vdsp_v2_host_boot_init(&checked, 48) == 0;
      vdsp_v2_host_boot_write_begin(&checked);
      for (unsigned i = 1; identity && i < 17; ++i) identity = vdsp_v2_host_boot_receive(&checked, record[i]);
      vdsp_v2_host_boot_stop(&checked);
      identity = identity && checked.committed;
    }
  }
  if (!identity) return fail(-105);
  qspi_status_.store(2);
  if (!load_spi_(writer_image_, writer_size_, 3)) return fail(-106);
  const uint32_t start = now_(this);
  gpio_set_direction(GPIO_NUM_13, GPIO_MODE_INPUT);
  while (!gpio_get_level(GPIO_NUM_13)) {
    if (shutdown_.load() || uint32_t(now_(this) - start) > 10000) return fail(-107);
    wait_(this, 10);
  }
  wait_(this, 100);

  struct Transport {
    VoiceDSPV2 &self;
    unsigned last_phase{99}, last_percent{101};
    uint32_t now() { return VoiceDSPV2::now_(&self); }
    void wait(unsigned ms) { VoiceDSPV2::wait_(&self, ms); }
    bool cancelled() { return self.shutdown_.load(); }
    int transfer(uint8_t addr, const uint8_t *tx, size_t nt, uint8_t *rx, size_t nr) {
      self.transaction_start_ = now();
      return vdsp_v2_program_bus_transfer(&self.bus_, addr, tx, nt, rx, nr);
    }
    void progress(QspiPhase phase, size_t done, size_t total) {
      static constexpr unsigned phases[]{1, 3, 4, 5, 6, 7};
      const unsigned stage = phases[unsigned(phase)];
      const unsigned percent = total ? unsigned(done * 100 / total) : 0;
      self.qspi_status_.store(int(stage));
      self.qspi_progress_.store(percent);
      if (stage != last_phase || percent != last_percent) {
        ESP_LOGI(TAG, "QSPI_PROGRESS phase=%u percent=%u bytes=%u/%u", stage, percent, unsigned(done), unsigned(total));
        last_phase = stage; last_percent = percent;
      }
    }
  } transport{*this};
  QspiProgram<Transport> program(transport);
  const auto result = program.run(qspi_image_, qspi_size_, 1365137745u);
  if (result != QspiResult::OK) return fail(-120 - int(result));
  ESP_LOGI(TAG, "QSPI_VERIFIED full_readback_crc=1365137745 upper_settings_preserved_from=0x200000");
  auto write = [this](uint8_t reg, uint8_t value) {
    const uint8_t bytes[]{reg, value};
    return transfer_(this, 0x20, bytes, 2, nullptr, 0) == 0;
  };
  if (!write(2, 0) || !write(6, 0xe6)) return fail(-108);
  wait_(this, 20);
  uint8_t reg = 0, input = 0xff;
  if (transfer_(this, 0x20, &reg, 1, &input, 1) || (input & 1u)) return fail(-109);
  if (!write(6, 0xe7)) return fail(-110);
  qspi_status_.store(8);
  for (unsigned i = 0; i < 75; ++i) {
    if (shutdown_.load()) return fail(-103);
    wait_(this, 100);
  }
  existing_delay_done_ = true;
  return true;
}
}  // namespace esphome::voicedsp_v2
