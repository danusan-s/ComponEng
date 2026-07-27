#pragma once

#include "componeng/ecs/system.hpp"

namespace aim_train {

class PlayerController : public componeng::ecs::ISystem {
  float m_sensitivity = 0.25f;

public:
  void onUpdate(const componeng::ecs::SystemState &state) override;
};

} // namespace aim_train
