#include "componeng/net/network_system.hpp"

#include "componeng/ecs/world.hpp"
#include "componeng/net/network.hpp"

namespace componeng::net {

void NetworkSystem::onCreate(const ecs::SystemState &state) {
  if (state.world->hasResource<Network>()) {
    state.world->getResource<Network>().bindWorld(state.world);
  }
}

void NetworkSystem::onUpdate(const ecs::SystemState &state) {
  if (state.world->hasResource<Network>()) {
    state.world->getResource<Network>().update(state.world->time);
  }
}

void NetworkSystem::onDestroy(const ecs::SystemState &state) {
  // The world is going away; the Network may outlive it.
  if (state.world->hasResource<Network>()) {
    state.world->getResource<Network>().bindWorld(nullptr);
  }
}

} // namespace componeng::net
