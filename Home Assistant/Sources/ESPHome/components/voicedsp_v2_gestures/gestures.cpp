#include "gestures.h"
#include "esphome/core/hal.h"
#include <cstdio>
namespace esphome::voicedsp_v2_gestures {
void Gestures::loop() {
  const auto host = controller_->snapshot();
  const auto &s = host.gesture;
  // ESPHome millis() may use RTOS ticks; snapshots use esp_timer instead.
  const uint32_t now = static_cast<uint32_t>(millis_64());
  const bool available = controller_->ready() && host.error == 0 && s.ready && s.health_valid &&
      static_cast<uint32_t>(now - s.health_sampled_ms) <= VDSP_GESTURE_HEALTH_STALE_MS;
  const bool event_fresh = available && s.valid &&
      static_cast<uint32_t>(now - s.sampled_ms) <= VDSP_GESTURE_STALE_MS &&
      static_cast<uint32_t>(now - s.event_ms) <= VDSP_GESTURE_STALE_MS;
  available_->publish_state(available);
  if (s.sequence != sequence_) {
    const uint32_t delta = s.sequence - sequence_;
    if (delta > 1) missed_ += delta - 1;
    sequence_ = s.sequence;
    if (event_fresh && s.event_raw)
      event_->trigger(vdsp_v2_gesture_name(s.event_raw));
    else
      ++rejected_;
  }
  if (!diagnostic_sent_ || static_cast<uint32_t>(now - diagnostic_ms_) >= 1000) {
    diagnostic_sent_ = true;
    diagnostic_ms_ = now;
    char message[256];
    std::snprintf(message, sizeof(message),
        "sensor_axes=PixArt-v1.5 unqualified; ready=%d ef=%d stage=%u ID=%04x raw=%03x rd=%lu err=%lu try=%u sup=%lu missed=%lu stale=%lu io=%d@%02x/%u fail=%u,%u:%02x/%u:%d",
        available, event_fresh, static_cast<unsigned>(s.stage), static_cast<unsigned>(s.part_id),
        static_cast<unsigned>(s.raw), static_cast<unsigned long>(s.reads),
        static_cast<unsigned long>(s.errors), static_cast<unsigned>(s.attempts),
        static_cast<unsigned long>(s.suppressed), static_cast<unsigned long>(missed_),
        static_cast<unsigned long>(rejected_), s.last_transport_result,
        static_cast<unsigned>(s.last_register), static_cast<unsigned>(s.last_operation),
        static_cast<unsigned>(s.failure_kind), static_cast<unsigned>(s.failure_stage),
        static_cast<unsigned>(s.failure_register), static_cast<unsigned>(s.failure_operation),
        s.failure_transport_result);
    diagnostic_->publish_state(message);
  }
}
}
