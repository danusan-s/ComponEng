#pragma once

#include "componeng/ecs/system.hpp"

namespace componeng::net {

// Binds the Network to the world for ECS events and updates it once a frame,
// ahead of the systems that read the results. Does nothing while no Network is
// open.
class NetworkSystem final : public ecs::ISystem {
public:
  void onCreate(const ecs::SystemState &state) override;
  void onUpdate(const ecs::SystemState &state) override;
  void onDestroy(const ecs::SystemState &state) override;
};

} // namespace componeng::net
