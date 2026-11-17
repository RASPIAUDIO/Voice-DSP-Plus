#pragma once
#include <atomic>
#include <string>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esphome/core/component.h"
#include "esphome/components/i2s_audio/speaker/i2s_audio_speaker.h"
#include "voicedsp_v2_host.h"
#include "voicedsp_v2_host_bus.h"
#include "voicedsp_v2_equalizer.h"
#include "v2_ui_runtime.h"
#include "v2_aec_route_policy.h"
#include "voicedsp_v2_gesture.h"

namespace esphome::voicedsp_v2 {
struct HostCommand { uint32_t epoch; vdsp_v2_audio_route route; unsigned attenuation; unsigned external_gain_db{}; unsigned internal_gain_db{}; uint32_t crossover{}; };
struct StartupTrace {
  bool done{}, resume_attempted{}, bootstrap_claimed{};
  int bootstrap_result{}, resume_result{}, bus_error{};
  unsigned stage{}, address{}, tx0{}, tx1{}, nt{}, nr{};
};
struct HostSnapshot {
  uint32_t captured_ms;
  bool ready, valid, session, power_valid;
  int error;
  unsigned route, reason, microphone, buttons, millivolts, milliamps;
  bool pg, amp_fault, gesture_irq;
  uint32_t inputs_ms;
  vdsp_v2_gesture_snapshot gesture;
  unsigned port0{};
  vdsp_v2_power_sample power_sample{};
  uint32_t power_errors{}, io_sample_ms{};
  int power_error{};
  UiSnapshot ui{};
  StartupTrace startup{};
  vdsp_v2_runtime_trace runtime_failure{};
  uint32_t runtime_failures{};
  uint32_t completed_epoch{};
};

class VoiceDSPV2 : public Component {
 public:
  void set_existing_boot(bool enabled) { existing_boot_ = enabled; }
  bool existing_boot() const { return existing_boot_; }
  bool arm_spi_reload();
  bool arm_qspi_install();
  void set_qspi_images(const uint8_t *image, size_t size, const uint8_t *writer, size_t writer_size) {
    qspi_image_ = image; qspi_size_ = size; writer_image_ = writer; writer_size_ = writer_size;
  }
  int qspi_status() const { return qspi_status_.load(); }
  unsigned qspi_progress() const { return qspi_progress_.load(); }
  std::string qspi_diagnostics() const;
  void setup() override;
  void set_spi_image(const uint8_t *data, size_t size) { spi_image_ = data; spi_size_ = size; }
  int spi_boot_status() const { return spi_status_.load(); }
  unsigned spi_boot_progress() const { return spi_progress_.load(); }
  void loop() override;
  void dump_config() override;
  bool teardown() override;
  void on_shutdown() override { shutdown_.store(true); }
  void set_clock_speaker(i2s_audio::I2SAudioSpeaker *speaker) { speaker_ = speaker; }
  // Main-loop API only. Calls never touch I2C or block on driver operations.
  bool ready() const {
    return !shutdown_.load() && !stopped_.load() && snapshot_.ready && snapshot_.session &&
           snapshot_.valid && snapshot_.error == 0 &&
           snapshot_.completed_epoch == requested_epoch_.load() &&
           static_cast<uint32_t>(now_(nullptr) - snapshot_.captured_ms) < 1000;
  }
  // Explicit user retry is allowed after a codec fault, not after lost ownership.
  bool can_request_audio() const {
    return !shutdown_.load() && !stopped_.load() && !update_requested_ &&
           snapshot_.ready && snapshot_.session && snapshot_.valid &&
           static_cast<uint32_t>(now_(nullptr) - snapshot_.captured_ms) < 1000;
  }
  bool power_diagnostic_fresh() const {
    return !shutdown_.load() && !stopped_.load() && snapshot_.session &&
      static_cast<uint32_t>(now_(nullptr) - snapshot_.captured_ms) < 1000 &&
      vdsp_v2_power_fresh(&snapshot_.power_sample, now_(nullptr), 1000);
  }
  bool io_diagnostic_fresh() const {
    return !shutdown_.load() && !stopped_.load() && snapshot_.session && snapshot_.valid &&
      static_cast<uint32_t>(now_(nullptr) - snapshot_.captured_ms) < 1000 &&
      static_cast<uint32_t>(now_(nullptr) - snapshot_.io_sample_ms) < 1000;
  }
  std::string diagnostics() const;
  bool configure_crossover(int mode, int hz, int left, int right);
  std::string crossover_diagnostics() const;
  void request_xmos_mute(bool muted);
  void request_aec_raw(bool raw) { aec_request_.store(UiRuntime::next_request(aec_request_.load(), raw?1:0)); }
  void request_aec_probe(unsigned mode) { if(mode<=5) aec_request_.store(UiRuntime::next_request(aec_request_.load(), mode)); }
  void request_aec_profile(unsigned profile) { if(profile<=6) profile_request_.store(UiRuntime::next_request(profile_request_.load(), profile)); }
  void request_aec_product_mode(bool external, bool internal=false) {
    const unsigned route=aec_route_key(external,internal);
    const unsigned wanted=aec_route_profile(route);
    const bool route_changed=aec_product_route_.exchange(route)!=route;
    // Clear temporary diagnostics on route changes even when both defaults are 4.
    if(aec_default_.exchange(wanted)!=wanted || route_changed) profile_request_.store(0);
  }
  void request_xmos_ui_state(unsigned state);
  void request_xmos_ui_progress(unsigned progress);
  void request_xmos_led_style(uint8_t brightness, uint8_t red, uint8_t green, uint8_t blue);
  UiSnapshot xmos_ui_snapshot() const;
  std::string xmos_ui_diagnostics() const;

  std::string startup_diagnostics() const;
  std::string runtime_diagnostics() const;
  // Synchronous OTA pre-write barrier. Only call from the main task.
  bool prepare_update(unsigned timeout_ms = 2000);
  bool update_requested() const { return update_requested_; }
  void request(unsigned route, unsigned attenuation = 60);
  void request_external_level(int db);
  void request_internal_level(int db);
  HostSnapshot snapshot() const { return snapshot_; }
  void set_gesture_enabled() { gesture_enabled_ = true; }

 protected:
  static void task_(void *);
  void run_();
  bool timing_ui_seen_{false}, timing_led_seen_{false}; // worker-only observations
  bool boot_spi_();
  bool load_spi_(const uint8_t *, size_t, unsigned);
  bool program_qspi_();
  bool qspi_requested_{false}, existing_delay_done_{false};
  const uint8_t *qspi_image_{nullptr}, *writer_image_{nullptr};
  size_t qspi_size_{}, writer_size_{};
  std::atomic<int> qspi_status_{};
  std::atomic<unsigned> qspi_progress_{};
  std::atomic<bool> qspi_running_{false};
  bool existing_boot_{false};
  int start_existing_(const vdsp_v2_host_ops &, const vdsp_v2_policy_limits &);
  const uint8_t *spi_image_{nullptr};
  size_t spi_size_{};
  std::atomic<int> spi_status_{};
  std::atomic<unsigned> spi_progress_{};
  bool open_bus_();
  void close_bus_();
  void publish_();
  static int transfer_(void *, uint8_t, const uint8_t *, size_t, uint8_t *, size_t);
  static uint32_t now_(void *);
  static void wait_(void *, unsigned);
  static int clocks_(void *, unsigned);
  static bool cancelled_(void *);
  static void low_(void *, unsigned);
  static void release_(void *, unsigned);
  static bool level_(void *, unsigned);
  static void half_period_(void *);
  i2s_audio::I2SAudioSpeaker *speaker_{nullptr};
  QueueHandle_t commands_{nullptr}, snapshots_{nullptr};
  TaskHandle_t task_handle_{nullptr};
  vdsp_v2_i2c bus_{};
  uint32_t transaction_start_{0};
  int bus_fault_{0};
  unsigned bus_fault_addr_{0}, bus_fault_tx0_{0}, bus_fault_tx1_{0};
  unsigned bus_fault_nt_{0}, bus_fault_nr_{0};
  uint32_t bus_fault_ms_{0};
  std::atomic<uint32_t> main_loop_ms_{0};
  uint32_t heartbeat_ms_{0}, logged_runtime_failures_{0};
  unsigned logged_route_{~0u}, logged_reason_{~0u};
  int logged_error_{0}, logged_power_error_{0};
  bool pins_configured_{false};
  vdsp_v2_host host_{};
  std::atomic<uint32_t> crossover_requested_{1u | (10u<<3) | (6u<<8) | (12u<<13)};
  std::atomic<uint32_t> crossover_applied_{~0u};
  std::atomic<unsigned> crossover_attenuation_{60};
  std::atomic<int> crossover_error_{};
  HostCommand crossover_inflight_{};
  static int prepare_crossover_(void *, vdsp_v2_audio *, unsigned, unsigned);
  void request_crossover_output_(const HostCommand &);
  UiRuntime ui_runtime_{};
  std::atomic<uint32_t> aec_request_{}, profile_request_{}, aec_default_{};
  std::atomic<unsigned> aec_product_route_{};
  std::atomic<uint32_t> ui_requests_[7]{}; // main-loop intent, worker consumption
  void poll_xmos_ui_();

  vdsp_v2_gesture gesture_{};
  bool gesture_enabled_{false};
  static int gesture_transfer_(void *, uint8_t, const uint8_t *, size_t, uint8_t *, size_t);
  void poll_gesture_();
  HostSnapshot snapshot_{};
  StartupTrace startup_{}; // worker-owned; published via the snapshot queue
  std::atomic<bool> shutdown_{false}, stopped_{false}, need_clocks_{false}, clocks_ready_{false};
  std::atomic<uint32_t> requested_epoch_{0};
  uint32_t executing_epoch_{0}; // worker-only after startup
  bool booting_{true}; // worker-only
  bool clock_start_sent_{false}, launched_{false}; // main-loop only
  bool update_requested_{false}; // main-loop OTA callbacks only
};
}  // namespace esphome::voicedsp_v2
