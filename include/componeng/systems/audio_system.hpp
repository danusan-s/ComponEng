#pragma once

#include "componeng/ecs/system.hpp"

namespace componeng::systems {

class AudioSystem : public ecs::ISystem {
public:
  void onUpdate(const ecs::SystemState &state) override;
};

} // namespace componeng::systems
