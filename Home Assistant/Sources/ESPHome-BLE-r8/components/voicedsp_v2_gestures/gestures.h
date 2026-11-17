#pragma once
#include "esphome/core/component.h"
#include "esphome/components/voicedsp_v2/v2_component.h"
#include "esphome/components/event/event.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
namespace esphome::voicedsp_v2_gestures {
class Gestures : public Component {
 public:
  void loop() override;
  void set_controller(voicedsp_v2::VoiceDSPV2 *p) { controller_ = p; }
  void set_event(event::Event *p) { event_ = p; }
  void set_available(binary_sensor::BinarySensor *p) { available_ = p; }
  void set_diagnostic(text_sensor::TextSensor *p) { diagnostic_ = p; }
 protected:
  voicedsp_v2::VoiceDSPV2 *controller_{nullptr};
  event::Event *event_{nullptr};
  binary_sensor::BinarySensor *available_{nullptr};
  text_sensor::TextSensor *diagnostic_{nullptr};
  uint32_t sequence_{0}, missed_{0}, rejected_{0}, diagnostic_ms_{0};
  bool diagnostic_sent_{false};
};
}
