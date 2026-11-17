#include "buttons.h"
#include "esphome/core/hal.h"

namespace esphome::voicedsp_v2_buttons {
void Buttons::loop() {
  const auto snapshot = controller_->snapshot();
  const auto result = vdsp_v2_buttons_update(&filter_, static_cast<uint32_t>(millis_64()), snapshot.inputs_ms,
      controller_->ready() && snapshot.error == 0, static_cast<uint8_t>(snapshot.buttons));
  available_->publish_state(result.available);
  const uint8_t masks[] = {result.pressed, result.released, result.short_press,
                           result.long_press, result.cancelled};
  const char *names[] = {"pressed", "released", "short_press", "long_press", "cancelled"};
  for (unsigned i = 0; i < 4; ++i)
    for (unsigned j = 0; j < 5; ++j)
      if (masks[j] & (1u << i)) events_[i]->trigger(names[j]);
}
}
