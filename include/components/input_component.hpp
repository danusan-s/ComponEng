#pragma once

struct InputComponent {
  bool forward, backward, left, right; // 4 bytes
  bool jump, crouch;                   // 2 bytes
};

struct MouseInputComponent {
  float deltaX, deltaY;         // 8 bytes
  bool leftButton, rightButton; // 2 bytes
};
