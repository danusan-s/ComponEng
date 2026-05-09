#pragma once

#include "componeng/core/raw_input_state.hpp"

namespace componeng::resources {

struct InputState {
  core::RawInputState current_state;
  core::RawInputState previous_state;

  bool isKeyPressed(int key) const {
    return current_state.keys[key] && !previous_state.keys[key];
  }

  bool isKeyReleased(int key) const {
    return !current_state.keys[key] && previous_state.keys[key];
  }

  bool isMouseButtonPressed(int button) const {
    return current_state.mouseButtons[button] &&
           !previous_state.mouseButtons[button];
  }

  bool isMouseButtonReleased(int button) const {
    return !current_state.mouseButtons[button] &&
           previous_state.mouseButtons[button];
  }

  float getMouseDeltaX() const {
    return current_state.mouseX - previous_state.mouseX;
  }

  float getMouseDeltaY() const {
    return current_state.mouseY - previous_state.mouseY;
  }
};

} // namespace componeng::resources
