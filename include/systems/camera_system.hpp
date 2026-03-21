#pragma once
#include "system.hpp"
#include "types.hpp"

class CameraSystem : public System {
public:
  void Update(float deltaTime) override;

private:
  Vec2 smoothedMouseDelta = Vec2(0.0f);
  static constexpr float SMOOTHING = 0.7f;
};
