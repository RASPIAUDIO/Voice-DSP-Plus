#pragma once
#include <cstdint>

namespace vdsp_auto {
enum class Task { NONE, MODE, FILTER, SAVE };
struct Filter { int profile{1}; int hz{8500}; int woofer{-6}; int tweeter{-12}; };

// Requests are separate from the in-flight snapshot. Boot restoration and
// programmatic preset publications must not create new user requests.
class Queue {
 public:
  bool armed{false}, internal_write{false}, blocked{false};
  Task task{Task::NONE};
  int active_mode{0};
  Filter active_filter{};
  Filter applied_filter{};
  void arm() { armed = true; }
  bool accepts() const { return armed && !internal_write; }
  void mode(int value, uint32_t now) {
    if (!accepts() || value < 0 || value > 2) return;
    wanted_mode_ = value; mode_ = true;
    filter_ = false;  // A later preset supersedes edits to the previous preset.
    changed(now);
  }
  void filter(Filter value, uint32_t now) {
    if (!accepts() || value.profile < 0 || value.profile > 5 ||
        value.hz < 6000 || value.hz > 10000 || (value.hz - 6000) % 250 ||
        value.woofer < -24 || value.woofer > 0 || value.tweeter < -24 || value.tweeter > 0) return;
    wanted_filter_ = value; filter_ = true; changed(now);
  }
  void save(uint32_t now) {
    if (!accepts()) return;
    save_ = true; changed(now);
  }
  bool pending() const { return mode_ || filter_ || save_; }
  bool busy() const { return task != Task::NONE; }
  Task poll(bool idle, uint32_t now) {
    if (!armed || blocked || busy() || !idle) { quiet_ = false; return Task::NONE; }
    if (!quiet_) { quiet_ = true; quiet_since_ = now; }
    if (!pending() || uint32_t(now - quiet_since_) < 1000 || uint32_t(now - edited_) < 1000)
      return Task::NONE;
    if (mode_) { mode_ = false; active_mode = wanted_mode_; task = Task::MODE; }
    else if (filter_) { filter_ = false; active_filter = wanted_filter_; task = Task::FILTER; }
    else { save_ = false; task = Task::SAVE; }
    return task;
  }
  void finish(bool success) {
    task = Task::NONE; quiet_ = false;
    blocked = !success;  // No endless retries or repeated output switching.
  }
 private:
  bool mode_{false}, filter_{false}, save_{false}, quiet_{false};
  int wanted_mode_{0};
  Filter wanted_filter_{};
  uint32_t edited_{0}, quiet_since_{0};
  void changed(uint32_t now) { edited_ = now; blocked = false; }
};
}
