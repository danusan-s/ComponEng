#include "componeng/net/network_system.hpp"

#include "componeng/ecs/world.hpp"
#include "componeng/net/lan_discovery.hpp"
#include "componeng/net/network.hpp"
#include "componeng/net/session.hpp"

namespace componeng::net {

void NetworkSystem::onCreate(const ecs::SystemState &state) {
  if (state.world->hasResource<Network>()) {
    state.world->getResource<Network>().bindWorld(state.world);
  }
  if (state.world->hasResource<Session>()) {
    state.world->getResource<Session>().bindWorld(state.world);
  }
}

void NetworkSystem::onUpdate(const ecs::SystemState &state) {
  const double now = state.world->time;
  if (state.world->hasResource<Network>()) {
    state.world->getResource<Network>().update(now);
  }
  // After the network: the session reacts to what it just delivered.
  if (state.world->hasResource<Session>()) {
    state.world->getResource<Session>().update(now);
  }
  if (state.world->hasResource<LanBrowser>()) {
    state.world->getResource<LanBrowser>().update(now);
  }
}

void NetworkSystem::onDestroy(const ecs::SystemState &state) {
  // The world is going away; the Network may outlive it.
  if (state.world->hasResource<Network>()) {
    state.world->getResource<Network>().bindWorld(nullptr);
  }
  if (state.world->hasResource<Session>()) {
    state.world->getResource<Session>().bindWorld(nullptr);
  }
}

} // namespace componeng::net
