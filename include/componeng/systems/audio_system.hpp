#pragma once

#include "componeng/ecs/system.hpp"
#include <memory>
#include <miniaudio.h>
#include <vector>

namespace componeng::systems {

class AudioSystem : public ecs::ISystem {
public:
  void onUpdate(const ecs::SystemState &state) override;

private:
  std::vector<std::unique_ptr<ma_sound>> m_activeSounds;
};

} // namespace componeng::systems
