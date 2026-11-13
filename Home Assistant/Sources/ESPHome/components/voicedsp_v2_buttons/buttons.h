#pragma once
#include "esphome/core/component.h"
#include "esphome/components/voicedsp_v2/v2_component.h"
#include "esphome/components/event/event.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "voicedsp_v2_buttons.h"

namespace esphome::voicedsp_v2_buttons {
class Buttons : public Component {
 public:
  void loop() override;
  void set_controller(voicedsp_v2::VoiceDSPV2 *value) { controller_ = value; }
  void set_event(unsigned index, event::Event *value) { if (index < 4) events_[index] = value; }
  void set_available(binary_sensor::BinarySensor *value) { available_ = value; }
 protected:
  voicedsp_v2::VoiceDSPV2 *controller_{nullptr};
  event::Event *events_[4]{};
  binary_sensor::BinarySensor *available_{nullptr};
  vdsp_v2_buttons filter_{};
};
}
