#include "componeng/net/network_system.hpp"

#include "componeng/ecs/world.hpp"
#include "componeng/net/ecs_bridge.hpp"
#include "componeng/net/lan_discovery.hpp"
#include "componeng/net/network.hpp"
#include "componeng/net/session.hpp"

namespace componeng::net {

void NetworkSystem::onCreate(const ecs::SystemState &state) {
  ecs::World &world = *state.world;
  if (!world.hasResource<EcsBridge>()) {
    return;
  }
  EcsBridge &bridge = world.getResource<EcsBridge>();
  bridge.bindWorld(&world);
  if (world.hasResource<Network>()) {
    bridge.attach(world.getResource<Network>());
  }
  if (world.hasResource<Session>()) {
    bridge.attach(world.getResource<Session>());
  }
}

void NetworkSystem::onUpdate(const ecs::SystemState &state) {
  ecs::World &world = *state.world;
  const double now = world.time;
  if (world.hasResource<Network>()) {
    world.getResource<Network>().update(now);
  }
  // After the network: the session reacts to what it just delivered.
  if (world.hasResource<Session>()) {
    world.getResource<Session>().update(now);
  }
  if (world.hasResource<LanBrowser>()) {
    world.getResource<LanBrowser>().update(now);
  }
}

void NetworkSystem::onDestroy(const ecs::SystemState &state) {
  ecs::World &world = *state.world;
  // The session says goodbye through the network, so it leaves first.
  if (world.hasResource<Session>()) {
    world.getResource<Session>().leave();
  }
  if (world.hasResource<LanBrowser>()) {
    world.getResource<LanBrowser>().stop();
  }
  if (world.hasResource<Network>()) {
    world.getResource<Network>().close();
  }
  // The world is going away; the bridge may not outlive it.
  if (world.hasResource<EcsBridge>()) {
    world.getResource<EcsBridge>().bindWorld(nullptr);
  }
}

void installNetworking(ecs::World &world) {
  if (world.hasResource<Network>()) {
    return;
  }
  world.setResource<Network>(Network());
  world.setResource<Session>(Session(world.getResource<Network>()));
  world.setResource<LanBrowser>(LanBrowser());
  world.setResource<EcsBridge>(EcsBridge());
  world.registerSystem<NetworkSystem>(ecs::SystemGroup::Initialization);
}

} // namespace componeng::net
