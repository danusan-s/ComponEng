#pragma once

#include "componeng/ecs/system.hpp"

namespace componeng::audio {

class AudioSystem : public ecs::ISystem {
public:
  void onUpdate(const ecs::SystemState &state) override;
};

} // namespace componeng::audio
