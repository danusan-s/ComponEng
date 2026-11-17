#pragma once

#include "componeng/ecs/system.hpp"

namespace componeng {
namespace ecs {
class World;
}

namespace net {

// Updates the world's Network, Session and LanBrowser once a frame, ahead of
// the systems that read the results, and binds the EcsBridge to the world. On
// destruction it leaves the session and closes the network. Skips resources
// that are absent.
class NetworkSystem final : public ecs::ISystem {
public:
  void onCreate(const ecs::SystemState &state) override;
  void onUpdate(const ecs::SystemState &state) override;
  void onDestroy(const ecs::SystemState &state) override;
};

// Opts a world in to networking: creates the Network, Session, LanBrowser and
// EcsBridge resources and registers NetworkSystem. Call once from IGame::init;
// nothing is opened until the game calls Network::open(). The engine does not
// do this itself, so a game that never calls it pays nothing.
void installNetworking(ecs::World &world);

} // namespace net
} // namespace componeng
