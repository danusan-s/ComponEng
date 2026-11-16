#pragma once

#include "componeng/ecs/system.hpp"

namespace componeng::net {

// Binds Network/Session to the world for ECS events and updates them (and the
// LanBrowser) once a frame, ahead of the systems that read the results. Skips
// resources that are absent.
class NetworkSystem final : public ecs::ISystem {
public:
  void onCreate(const ecs::SystemState &state) override;
  void onUpdate(const ecs::SystemState &state) override;
  void onDestroy(const ecs::SystemState &state) override;
};

} // namespace componeng::net
