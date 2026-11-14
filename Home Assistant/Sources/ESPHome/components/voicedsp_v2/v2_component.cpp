#include "v2_component.h"
#include "v2_aec_bus_clear.h"
#include <cstdio>
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esphome/core/log.h"

namespace esphome::voicedsp_v2 {
static const char *const TAG = "voicedsp_v2";
static constexpr gpio_num_t BUS_PINS[] = {GPIO_NUM_6, GPIO_NUM_5};

void VoiceDSPV2::setup() {
  commands_ = xQueueCreate(1, sizeof(HostCommand));
  snapshots_ = xQueueCreate(1, sizeof(HostSnapshot));
  if (!commands_ || !snapshots_ || !speaker_) {
    stopped_.store(true);
    mark_failed();
  }
}

void VoiceDSPV2::loop() {
  main_loop_ms_.store(now_(this));
  if (is_failed()) return;
  if (!launched_ && !shutdown_.load()) {
    launched_ = true;
    // Bit-banged I2C must not be starved by wake inference (priority 3).
    // Remain below the DMA audio task (19); the worker yields between transactions.
    if (xTaskCreate(task_, "v2_control", 8192, this, 4, &task_handle_) != pdPASS) {
      stopped_.store(true);
      mark_failed();
      return;
    }
  }
  if (need_clocks_.load() && !shutdown_.load()) {
    if (!clock_start_sent_) {
      // Stream payload is 16-bit; the fixed duplex bus has two 32-bit slots.
      speaker_->set_output_channel_mode(i2s_audio::I2SAudioSpeaker::OUTPUT_STEREO);
      speaker_->set_audio_stream_info(audio::AudioStreamInfo(16, 2, 48000));
      speaker_->start();
      clock_start_sent_ = true;
    }
    clocks_ready_.store(speaker_->is_running() && !speaker_->is_failed());
  }
  HostSnapshot next;
  if (xQueueReceive(snapshots_, &next, 0) == pdTRUE) {
    snapshot_ = next;
    if (next.error) status_set_warning(); else status_clear_warning();
  }
}

void VoiceDSPV2::dump_config() {
  ESP_LOGCONFIG(TAG, "V2.02: embedded XMOS I2S 48k SPI RAM boot; QSPI unchanged");
  ESP_LOGCONFIG(TAG, "Sole software I2C owner, SDA5/SCL6 <=100kHz; no scan/recovery");
  ESP_LOGCONFIG(TAG, "Controller starts OFF; product YAML owns startup output policy");
}

bool VoiceDSPV2::teardown() {
  shutdown_.store(true);
  return !launched_ || stopped_.load();
}

bool VoiceDSPV2::prepare_update(unsigned timeout_ms) {
  update_requested_ = true;
  shutdown_.store(true);
  const uint32_t started = now_(this);
  while (launched_ && !stopped_.load()) {
    if (static_cast<uint32_t>(now_(this) - started) >= timeout_ms) {
      ESP_LOGI(TAG, "UPDATE_QUIESCE timeout; worker still active");
      return false;
    }
    // Yield to the sole I2C worker until its final STOP and pin release finish.
    vTaskDelay(pdMS_TO_TICKS(1) > 0 ? pdMS_TO_TICKS(1) : 1);
  }
  ESP_LOGI(TAG, "UPDATE_QUIESCE complete; worker stopped, I2C pins released");
  return true;
}

void VoiceDSPV2::request(unsigned route, unsigned attenuation) {
  if (!commands_ || shutdown_.load() || route < VDSP_AUDIO_OFF ||
      route > VDSP_AUDIO_DAC_SPEAKERS || attenuation > 127) return;
  HostCommand command{requested_epoch_.fetch_add(1) + 1,
                      static_cast<vdsp_v2_audio_route>(route), attenuation};
  command.crossover = crossover_requested_.load();
  xQueueOverwrite(commands_, &command);
}

void VoiceDSPV2::request_external_level(int db) {
  if (db < -60 || db > 9 || !commands_ || shutdown_.load()) return;
  if (db > 0) db = 0;  // Clamp stale positive calls as well as the HA slider.
  HostCommand command{requested_epoch_.fetch_add(1) + 1, VDSP_AUDIO_HT326,
                      db < 0 ? static_cast<unsigned>(-2 * db) : 0,
                      db > 0 ? static_cast<unsigned>(db) : 0};
  command.crossover = crossover_requested_.load();
  xQueueOverwrite(commands_, &command);
}

static int prepare_external_gain(void *context, vdsp_v2_audio *audio, unsigned, unsigned attenuation) {
  const auto gain = *static_cast<const unsigned *>(context);
  if (gain > 9 || attenuation != 0 || !audio->initialized || !audio->muted ||
      audio->route != VDSP_AUDIO_OFF || audio->control->amp_enabled) return -2;
  const int result = vdsp_v2_audio_volume(audio, 0);
  if (!result) audio->external_gain_db = gain;
  return result;
}

void VoiceDSPV2::request_internal_level(int db) {
  if (db < -60 || db > 0 || !commands_ || shutdown_.load()) return;
  HostCommand command{requested_epoch_.fetch_add(1) + 1, VDSP_AUDIO_DAC_SPEAKERS,
                      db < 0 ? static_cast<unsigned>(-2 * db) : 0, 0, 0};
  command.crossover = crossover_requested_.load();
  xQueueOverwrite(commands_, &command);
}

static int prepare_internal_gain(void *context, vdsp_v2_audio *audio, unsigned, unsigned attenuation) {
  const auto gain = *static_cast<const unsigned *>(context);
  if (gain > 6 || attenuation != 0 || !audio->initialized || !audio->muted ||
      audio->route != VDSP_AUDIO_OFF || audio->control->amp_enabled) return -2;
  const int result = vdsp_v2_audio_volume(audio, 0);
  if (!result) audio->internal_gain_db = gain;
  return result;
}

void VoiceDSPV2::request_xmos_mute(bool muted) {
  if (shutdown_.load()) return;
  ui_requests_[0].store(UiRuntime::next_request(ui_requests_[0].load(), muted ? 1 : 0));
}

void VoiceDSPV2::request_xmos_ui_state(unsigned state) {
  if (shutdown_.load() || state > 15) return;
  ui_requests_[1].store(UiRuntime::next_request(ui_requests_[1].load(), static_cast<uint8_t>(state)));
}

void VoiceDSPV2::request_xmos_ui_progress(unsigned progress) {
  if (shutdown_.load() || progress > 100) return;
  ui_requests_[2].store(UiRuntime::next_request(ui_requests_[2].load(), static_cast<uint8_t>(progress)));
}

void VoiceDSPV2::request_xmos_led_style(uint8_t brightness, uint8_t red, uint8_t green, uint8_t blue) {
  if (shutdown_.load()) return;
  const uint8_t values[] = {brightness, red, green, blue};
  for (unsigned i = 0; i < 4; ++i)
    ui_requests_[i + 3].store(UiRuntime::next_request(ui_requests_[i + 3].load(), values[i]));
}

UiSnapshot VoiceDSPV2::xmos_ui_snapshot() const {
  if (!ready()) return {};
  auto result = snapshot_.ui;
  if (result.acknowledged[0] != ui_requests_[0].load()) result.mute_valid = false;
  if (result.acknowledged[1] != ui_requests_[1].load()) result.state_valid = false;
  if (result.acknowledged[2] != ui_requests_[2].load()) result.progress_valid = false;
  if (static_cast<uint32_t>(now_(nullptr) - result.levels_ms) > 2500) result.levels_valid = false;
  return result;
}

std::string VoiceDSPV2::xmos_ui_diagnostics() const {
  const auto value = xmos_ui_snapshot();
  char message[250];
  std::snprintf(message, sizeof(message), "cap=%d cookie=%lu mute_ack=%d muted=%d ui_ack=%d ui=%u progress_ack=%d progress=%u rms=%d err=%d style=%d led=%u,%u,%u,%u",
      value.available, static_cast<unsigned long>(value.session_cookie), value.mute_valid, value.muted,
      value.state_valid, static_cast<unsigned>(value.state), value.progress_valid,
      static_cast<unsigned>(value.progress), value.levels_valid, value.error, value.style_available,
      unsigned(value.style[0]), unsigned(value.style[1]), unsigned(value.style[2]), unsigned(value.style[3]));
  return message;
}

void VoiceDSPV2::poll_xmos_ui_() {
  uint32_t requests[7];
  for (unsigned i = 0; i < 7; ++i) requests[i] = ui_requests_[i].load();
  ui_runtime_.step(host_, requests, aec_request_.load(), profile_request_.load(), aec_default_.load());
}

bool VoiceDSPV2::configure_crossover(int mode, int hz, int left, int right) {
  if (mode < 0 || mode > 5 || hz < 6000 || hz > 10000 || (hz-6000)%250 ||
      left < -24 || left > 0 || right < -24 || right > 0) return false;
  const uint32_t word = mode | (((hz-6000)/250) << 3) | ((-left) << 8) | ((-right) << 13);
  crossover_requested_.store(word);
  return true;
}

std::string VoiceDSPV2::crossover_diagnostics() const {
  const auto word = crossover_applied_.load();
  char text[160];
  snprintf(text, sizeof(text), "applied=%u mode=%u fc=%u L=%d R=%d base=%.1fdB rc=%d; mono ref; cap4.7uF",
    word != ~0u, word & 7u, 6000u + 250u*((word >> 3)&31u),
    -int((word >> 8)&31u), -int((word >> 13)&31u),
    -.5f*crossover_attenuation_.load(), crossover_error_.load());
  return std::string(text);
}

int VoiceDSPV2::prepare_crossover_(void *context, vdsp_v2_audio *audio,
                                  unsigned rate, unsigned attenuation) {
  auto *self = static_cast<VoiceDSPV2 *>(context);
  const auto &cmd = self->crossover_inflight_;
  const auto word = cmd.crossover;
  const unsigned mode = word & 7u;
  vdsp_v2_equalizer_pair pair{};
  int error;
  const auto effective = self->host_.decision.route;
  if (effective == VDSP_AUDIO_HT326 && mode >= 4) {
    // Identification only: capacitor retained, strict low-level full-band isolation.
    if (attenuation < 72) attenuation = 72;
    error = vdsp_v2_measure_flat_apply_off(audio, rate, mode == 4 ? 2u : 1u,
                                         -int(attenuation), &pair);
  } else if (effective == VDSP_AUDIO_HT326 && mode != 0) {
    // Product two-way mode follows the slider; isolated measurement modes stay limited.
    if (mode != 1 && attenuation < 36) attenuation = 36;
    vdsp_v2_crossover_config config{
      float(6000u+250u*((word>>3)&31u)), -float((word>>8)&31u), -float((word>>13)&31u),
      mode == 2 ? 2u : mode == 3 ? 1u : 0u};
    error = vdsp_v2_crossover_apply_off(audio, rate, &config, -int(attenuation), &pair);
  } else {
    // Every non-crossover activation explicitly restores flat coefficients.
    error = vdsp_v2_equalizer_apply_pair_off(audio, rate, 0, 0, 0, 0, -int(attenuation), &pair);
    if (!error && cmd.internal_gain_db && effective == VDSP_AUDIO_DAC_SPEAKERS)
      error = prepare_internal_gain(&self->crossover_inflight_.internal_gain_db, audio, rate, attenuation);
  }
  self->crossover_attenuation_.store(attenuation);
  return error;
}

void VoiceDSPV2::request_crossover_output_(const HostCommand &command) {
  crossover_inflight_ = command;
  const int error = vdsp_v2_host_request_prepared(&host_, command.route, command.attenuation,
                                                prepare_crossover_, this);
  crossover_error_.store(error);
  crossover_applied_.store(error || host_.audio.route != VDSP_AUDIO_HT326 ? ~0u : command.crossover);
  ESP_LOGI("v2_crossover", "APPLY route=%u word=%lu attenuation=%u rc=%d",
    unsigned(command.route), (unsigned long)command.crossover, crossover_attenuation_.load(), error);
}

std::string VoiceDSPV2::diagnostics() const {
  char value[200];
  std::snprintf(value, sizeof(value), "ready=%d session=%d io=%d route=%u reason=%u err=%d mic=%u PD=%uV/%umA valid=%d PG=%d",
                ready(), snapshot_.session, snapshot_.valid, snapshot_.route,
                snapshot_.reason, snapshot_.error, snapshot_.microphone,
                snapshot_.millivolts / 1000, snapshot_.milliamps,
                snapshot_.power_valid, snapshot_.pg);
  return value;
}

std::string VoiceDSPV2::startup_diagnostics() const {
  const auto &s = snapshot_.startup;
  if (!s.done) return "Demarrage en cours";
  char value[200];
  std::snprintf(value, sizeof(value),
      "boot=%d claimed=%d resume=%s rc=%d stage=%u last_bus=%d addr=%02x tx=%02x/%02x nt=%u nr=%u",
      s.bootstrap_result, s.bootstrap_claimed, s.resume_attempted ? "yes" : "no",
      s.resume_result, s.stage, s.bus_error, s.address, s.tx0, s.tx1, s.nt, s.nr);
  return value;
}

std::string VoiceDSPV2::runtime_diagnostics() const {
  const auto &t = snapshot_.runtime_failure;
  char value[240];
  std::snprintf(value, sizeof(value),
      "failures=%lu stage=%u rc=%d cmd=%u status=%u tries=%u ms=%lu frames=%lu/%lu ticks=%lu/%lu",
      static_cast<unsigned long>(snapshot_.runtime_failures), t.stage, t.error,
      t.command, t.status, t.attempts, static_cast<unsigned long>(t.elapsed_ms),
      static_cast<unsigned long>(t.frames_before), static_cast<unsigned long>(t.frames_after),
      static_cast<unsigned long>(t.ticks_before), static_cast<unsigned long>(t.ticks_after));
  return value;
}

uint32_t VoiceDSPV2::now_(void *) { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
void VoiceDSPV2::wait_(void *, unsigned ms) {
  vTaskDelay(pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1);
}
bool VoiceDSPV2::cancelled_(void *context) {
  auto *self = static_cast<VoiceDSPV2 *>(context);
  return self->shutdown_.load() || (!self->booting_ &&
         self->executing_epoch_ != self->requested_epoch_.load());
}

int VoiceDSPV2::clocks_(void *context, unsigned rate) {
  auto *self = static_cast<VoiceDSPV2 *>(context);
  if (rate != 48000) return -2;
  self->need_clocks_.store(true);
  uint32_t start = now_(context);
  while (!self->clocks_ready_.load()) {
    if (self->shutdown_.load() || static_cast<uint32_t>(now_(context) - start) > 3000) return -3;
    wait_(context, 10);
  }
  // The portable service independently checks XMOS frame progression afterwards.
  return 0;
}

bool VoiceDSPV2::open_bus_() {
  // Load released output latches before enabling open-drain outputs. No SCL pulses.
  for (auto pin : BUS_PINS) if (gpio_set_level(pin, 1) != ESP_OK) return false;
  gpio_config_t config{};
  config.pin_bit_mask = (1ULL << GPIO_NUM_5) | (1ULL << GPIO_NUM_6);
  config.mode = GPIO_MODE_INPUT_OUTPUT_OD;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  if (gpio_config(&config) != ESP_OK) return false;
  pins_configured_ = true;
  const int cleared=v2_aec_bus_clear(
      [](unsigned p){return gpio_get_level(BUS_PINS[1-p])!=0;},
      [](unsigned p,bool release){gpio_set_level(BUS_PINS[1-p],release?1:0);},
      [](unsigned us){esp_rom_delay_us(us);});
  ESP_LOGI(TAG,"AEC_BOOT_BUS_CLEAR rc=%d scl=%d sda=%d",cleared,
      gpio_get_level(GPIO_NUM_6),gpio_get_level(GPIO_NUM_5));
  bus_ = {this, low_, release_, level_, half_period_, 2600};
  return true;
}

void VoiceDSPV2::close_bus_() {
  if (pins_configured_) {
    for (auto pin : BUS_PINS) {
      gpio_set_level(pin, 1);
      gpio_set_direction(pin, GPIO_MODE_INPUT);
    }
    pins_configured_ = false;
  }
}

void VoiceDSPV2::low_(void *, unsigned pin) { gpio_set_level(BUS_PINS[pin], 0); }
void VoiceDSPV2::release_(void *, unsigned pin) { gpio_set_level(BUS_PINS[pin], 1); }
void VoiceDSPV2::half_period_(void *) { esp_rom_delay_us(5); }
bool VoiceDSPV2::level_(void *context, unsigned pin) {
  auto *self = static_cast<VoiceDSPV2 *>(context);
  // A timed-out SCL check makes the portable driver abort without a recovery STOP.
  if (pin == VDSP_SCL && static_cast<uint32_t>(now_(context) - self->transaction_start_) >= 50) return false;
  return gpio_get_level(BUS_PINS[pin]) != 0;
}

int VoiceDSPV2::transfer_(void *context, uint8_t address, const uint8_t *tx,
                          size_t nt, uint8_t *rx, size_t nr) {
  auto *self = static_cast<VoiceDSPV2 *>(context);
  if (!self->pins_configured_ || (address != 0x2c && address != 0x20 && address != 0x18 && address != 0x08 && address != 0x73)) return -1;
  self->transaction_start_ = now_(context);
  const int result = vdsp_v2_host_bus_transfer(&self->bus_, address, tx, nt, rx, nr);
  if (result) {
    ESP_LOGW(TAG, "I2C_FAILURE rc=%d addr=%02x elapsed_ms=%lu scl=%d sda=%d",
        result, address, static_cast<unsigned long>(now_(context) - self->transaction_start_),
        gpio_get_level(GPIO_NUM_6), gpio_get_level(GPIO_NUM_5));
    self->bus_fault_ = result;
    self->bus_fault_addr_ = address;
    self->bus_fault_tx0_ = nt ? tx[0] : 0;
    self->bus_fault_tx1_ = nt > 1 ? tx[1] : 0;
    self->bus_fault_nt_ = static_cast<unsigned>(nt);
    self->bus_fault_nr_ = static_cast<unsigned>(nr);
    self->bus_fault_ms_ = now_(context);
  }
  return result;
}

int VoiceDSPV2::gesture_transfer_(void *context, uint8_t address, const uint8_t *tx, size_t nt, uint8_t *rx, size_t nr) {
  if (address != 0x73) return -1;
  if (cancelled_(context)) return VDSP_GESTURE_CANCELLED;
  return transfer_(context, address, tx, nt, rx, nr);
}

void VoiceDSPV2::poll_gesture_() {
  if (!gesture_enabled_) return;
  const bool allowed = host_.stage == VDSP_HOST_READY && host_.session_valid && host_.io.valid &&
      host_.last_error == 0 && !cancelled_(this) && static_cast<uint32_t>(now_(this) - host_.inputs_ms) <= 250;
  vdsp_v2_gesture_step(&gesture_, allowed, host_.io.gesture_irq);
}

void VoiceDSPV2::publish_() {
  // Worker-side heartbeat remains observable over USB if the main loop stalls.
  const uint32_t now = now_(this);
  if (now - heartbeat_ms_ >= 5000U) {
    ESP_LOGI(TAG, "WORKER_ALIVE main_age_ms=%lu runtime_failures=%lu",
        static_cast<unsigned long>(now - main_loop_ms_.load()),
        static_cast<unsigned long>(host_.runtime_failures));
    heartbeat_ms_ = now;
  }
  if (host_.runtime_failures != logged_runtime_failures_) {
    const auto &t = host_.runtime_failure;
    ESP_LOGI(TAG, "RUNTIME_FAILURE count=%lu stage=%u rc=%d cmd=%u status=%u tries=%u ms=%lu frames=%lu/%lu ticks=%lu/%lu",
        static_cast<unsigned long>(host_.runtime_failures), t.stage, t.error,
        t.command, t.status, t.attempts, static_cast<unsigned long>(t.elapsed_ms),
        static_cast<unsigned long>(t.frames_before), static_cast<unsigned long>(t.frames_after),
        static_cast<unsigned long>(t.ticks_before), static_cast<unsigned long>(t.ticks_after));
    (void) t;  // Native test logging may be compiled out.
    logged_runtime_failures_ = host_.runtime_failures;
  }
  const unsigned route = static_cast<unsigned>(host_.audio.route);
  const unsigned reason = static_cast<unsigned>(host_.decision.reason);
  // Log transitions after the transaction, not during timing-sensitive I2C.
  if (route != logged_route_ || reason != logged_reason_ ||
      host_.last_error != logged_error_ || host_.power.last_error != logged_power_error_) {
    ESP_LOGI(TAG, "AUDIO_TRACE route=%u reason=%u host=%d codec=%d inhibited=%d pg=%d power=%d kind=%u age=%lu epoch=%lu/%lu",
        route, reason, host_.last_error, host_.audio.last_error, host_.policy.inhibited,
        host_.io.buck_pg, host_.power.last_error, static_cast<unsigned>(host_.power.current.kind),
        static_cast<unsigned long>(now_(this) - host_.power.current.captured_ms),
        static_cast<unsigned long>(executing_epoch_), static_cast<unsigned long>(requested_epoch_.load()));
    if (bus_fault_) {
      ESP_LOGI(TAG, "BUS_LAST_FAULT rc=%d addr=%02x tx=%02x/%02x nt=%u nr=%u age=%lu",
          bus_fault_, bus_fault_addr_, bus_fault_tx0_, bus_fault_tx1_, bus_fault_nt_, bus_fault_nr_,
          static_cast<unsigned long>(now_(this) - bus_fault_ms_));
    }
    if (host_.audio.last_error == -4) {
      ESP_LOGW(TAG, "CODEC_STATUS_FAULT reg=%u value=%02x expected=%02x",
          host_.audio.fault_reg, host_.audio.fault_value, host_.audio.fault_expected);
    }
    logged_route_ = route;
    logged_reason_ = reason;
    logged_error_ = host_.last_error;
    logged_power_error_ = host_.power.last_error;
  }
  HostSnapshot next{now_(this), host_.stage == VDSP_HOST_READY, host_.io.valid, host_.session_valid,
    host_.power.current.valid, host_.last_error, static_cast<unsigned>(host_.audio.route),
    static_cast<unsigned>(host_.decision.reason), static_cast<unsigned>(host_.io.microphone),
    host_.io.buttons_pressed, host_.power.current.contract_mv, host_.power.current.contract_ma,
    host_.io.buck_pg, !host_.io.amp_sd_n, host_.io.gesture_irq, host_.inputs_ms, gesture_.snapshot};
  next.port0 = host_.io.port0;
  // Copy existing worker-owned telemetry; no extra I2C transaction or PD command.
  next.power_sample = host_.power.current;
  next.power_errors = host_.power.errors;
  next.power_error = host_.power.last_error;
  next.io_sample_ms = host_.refreshed_ms;
  next.ui = ui_runtime_.snapshot();
  next.startup = startup_;
  next.runtime_failure = host_.runtime_failure;
  next.runtime_failures = host_.runtime_failures;
  next.completed_epoch = executing_epoch_;
  xQueueOverwrite(snapshots_, &next);
}

void VoiceDSPV2::task_(void *context) {
  auto *self = static_cast<VoiceDSPV2 *>(context);
  self->run_();
  self->stopped_.store(true);
  vTaskDelete(nullptr);
}

void VoiceDSPV2::run_() {
  if (!open_bus_()) {
    host_.last_error = -1;
    startup_.done = true;
    startup_.bootstrap_result = -1;
    publish_();
    close_bus_();
    return;
  }
  vdsp_v2_host_ops ops{this, transfer_, now_, wait_, clocks_, cancelled_};
  if (!boot_spi_()) {
    host_.last_error = -53;
    startup_.done = true;
    startup_.bootstrap_result = -53;
    publish_();
    close_bus_();
    return;
  }
  // Engineering gates, not a validated product-wide current/power limiter.
  vdsp_v2_policy_limits limits{500, 100, 65000, 15000, 9000, 5000};
  int error = vdsp_v2_host_init(&host_, &ops, 48000, &limits, true);
  if (!error) error = vdsp_v2_host_start(&host_, esp_random());
  startup_.bootstrap_result = error;
  startup_.bootstrap_claimed = host_.claimed_bus;
  if (error && !host_.claimed_bus && !shutdown_.load()) {
    // No commit was sent: the XMOS may already be running after ESP32 OTA/reboot.
    const int bootstrap_error = error;
    startup_.resume_attempted = true;
    error = vdsp_v2_host_init(&host_, &ops, 48000, &limits, true);
    if (!error) error = vdsp_v2_host_resume(&host_);
    startup_.resume_result = error;
    ESP_LOGI(TAG, "Runtime resume result=%d (bootstrap=%d)", error, bootstrap_error);
    (void)bootstrap_error;  // Logging may be compiled out in native adapters.
  }
  ESP_LOGI(TAG, "Bootstrap result=%d stage=%u session=%d clocks=%d geometry=%u",
           error, static_cast<unsigned>(host_.stage), host_.session_valid,
           host_.clocks, static_cast<unsigned>(host_.io.microphone));
  startup_.done = true;
  startup_.stage = static_cast<unsigned>(host_.stage);
  startup_.bus_error = bus_fault_;
  startup_.address = bus_fault_addr_;
  startup_.tx0 = bus_fault_tx0_;
  startup_.tx1 = bus_fault_tx1_;
  startup_.nt = bus_fault_nt_;
  startup_.nr = bus_fault_nr_;
  booting_ = false;
  vdsp_v2_gesture_init(&gesture_, this, gesture_transfer_, now_);
  publish_();
  if (!error) {
    while (!shutdown_.load() && host_.stage == VDSP_HOST_READY) {
      HostCommand command;
      if (xQueueReceive(commands_, &command, pdMS_TO_TICKS(50)) == pdTRUE) {
        executing_epoch_ = command.epoch;
        request_crossover_output_(command);
      } else {
        vdsp_v2_host_poll(&host_);
      }
      crossover_error_.store(host_.last_error);
      crossover_applied_.store(host_.last_error || !host_.output_prepared_valid ||
          host_.audio.route != VDSP_AUDIO_HT326 ? ~0u : crossover_inflight_.crossover);
      poll_gesture_();
      poll_xmos_ui_();
      publish_();
    }
    vdsp_v2_host_stop(&host_);
    host_.session_valid = false;
    ui_runtime_.invalidate();
    host_.io.valid = false;
    vdsp_v2_gesture_step(&gesture_, false, false);
    publish_();
  }
  close_bus_();
}
}  // namespace esphome::voicedsp_v2
