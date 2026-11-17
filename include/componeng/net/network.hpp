#pragma once

#include "componeng/ecs/world.hpp"
#include "componeng/events/network_event.hpp"
#include "componeng/net/address.hpp"
#include "componeng/net/idatagram_socket.hpp"
#include "componeng/net/message_registry.hpp"
#include "componeng/net/nat_discovery.hpp"
#include "componeng/net/stun_socket.hpp"
#include "componeng/net/transport.hpp"
#include "componeng/utils/logger.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_set>
#include <vector>

namespace componeng::net {

// What a game uses: open a port, connect to peers, send typed messages and
// receive them typed. Wraps the socket, Transport and MessageRegistry.
//
// Peers are equal: whoever connect()s and whoever accepts get the same kind of
// connection. For more than two players connect each to each (a mesh).
//
// Arrivals come as callbacks (run inside update()) and/or ECS events (next
// frame; see events/network_event.hpp). NetworkSystem calls update() in the
// engine; elsewhere call update(now) yourself.
class Network {
public:
  Network();
  ~Network();

  Network(Network &&) noexcept;
  Network &operator=(Network &&) noexcept;
  Network(const Network &) = delete;
  Network &operator=(const Network &) = delete;

  // Opens a UDP socket on `port` (0 = any free) on every interface; closes any
  // previous one.
  bool open(uint16_t port = 0, Address::Family family = Address::Family::IPv4,
            const TransportConfig &config = TransportConfig());
  // Uses a socket you made (e.g. a LoopbackSocket in tests).
  bool open(std::unique_ptr<IDatagramSocket> socket,
            const TransportConfig &config = TransportConfig());
  void close();
  bool isOpen() const {
    return m_transport != nullptr;
  }
  const TransportConfig &config() const {
    return m_config;
  }
  // The IP is the wildcard: peers need your real one (see publicAddress()).
  Address localAddress() const;

  void setAcceptIncoming(bool accept);
  // Starts connecting; the outcome arrives as onPeerConnected /
  // onConnectFailed. False if not open, or already connected or connecting. The
  // string overload takes a numeric "ip:port".
  bool connect(const Address &peer);
  bool connect(const std::string &text);
  void disconnect(const Address &peer);

  // ---- Reaching peers across the internet ----
  // STUN tells a socket its public address; it does not introduce peers. Peers
  // swap addresses themselves (inviteCode()), then both send to each other at
  // about the same time so the routers let the packets in (hole punching):
  // discoverPublicAddress(), show inviteCode(), the host punch()es the joiner's
  // address and the joiner connectToInvite()s the host's code. Fails through
  // symmetric NATs (see natType()).

  // Asks STUN servers (host:port, resolved in the background; defaults are
  // Google's and Cloudflare's) for this socket's public address.
  // onNatDiscovery() fires when done; two answering servers also give
  // natType().
  void discoverPublicAddress(std::vector<std::string> servers = {});
  NatDiscovery::State natDiscoveryState() const;
  Address publicAddress() const;
  NatType natType() const;
  // Fires once per discoverPublicAddress(); the bool says whether any server
  // answered.
  void onNatDiscovery(std::function<void(bool, const NatInfo &)> handler);
  // Empty until the public address is known.
  std::string inviteCode() const;
  // False if `code` is not a valid invite code.
  bool connectToInvite(const std::string &code);
  void punch(const Address &peerPublicAddress, double duration = 10.0);
  static std::vector<std::string> defaultStunServers();

  bool isConnected(const Address &peer) const;
  std::vector<Address> peers() const;
  ConnectionStats stats(const Address &peer) const;

  // Registers a message type on its channel (Reliable for what must arrive,
  // Unreliable for what goes stale). Every peer must register the same types;
  // unknown ones are counted in rejectedMessages(). Also emits
  // MessageReceivedEvent<T> unless `emitEvents` is false (engine-internal
  // messages).
  template <typename T>
  bool registerMessage(Channel channel = Channel::Reliable,
                       bool emitEvents = true) {
    if (!m_registry.add<T>(channel)) {
      return false;
    }
    if (emitEvents && m_registry.contains<T>() &&
        !m_forwarded.count(std::type_index(typeid(T)))) {
      m_forwarded.insert(std::type_index(typeid(T)));
      std::shared_ptr<WorldBinding> binding = m_binding;
      m_registry.onReceive<T>([binding](const Address &from, const T &message) {
        if (binding->world) {
          binding->world->emitEvent(
              events::MessageReceivedEvent<T>{from, message});
        }
      });
    }
    return true;
  }
  template <typename T> bool onMessage(MessageRegistry::Handler<T> handler) {
    return m_registry.onReceive<T>(std::move(handler));
  }

  void onPeerConnected(std::function<void(const Address &)> handler);
  void onPeerDisconnected(
      std::function<void(const Address &, DisconnectReason)> handler);
  void onConnectFailed(
      std::function<void(const Address &, DisconnectReason)> handler);

  // Sends on the channel T was registered with. False if unregistered, not
  // open, not connected, or the peer is not keeping up.
  template <typename T> bool send(const Address &peer, const T &message) {
    if (!m_transport) {
      return false;
    }
    ByteWriter writer;
    Channel channel = Channel::Reliable;
    if (!m_registry.encode(message, writer, channel)) {
      LOG_ERROR("Cannot send '%s': it was never registered", T::kName);
      return false;
    }
    return m_transport->send(peer, channel, writer.data().data(),
                             writer.size());
  }

  // Returns how many peers it was queued for.
  template <typename T> size_t broadcast(const T &message) {
    if (!m_transport) {
      return 0;
    }
    ByteWriter writer;
    Channel channel = Channel::Reliable;
    if (!m_registry.encode(message, writer, channel)) {
      LOG_ERROR("Cannot send '%s': it was never registered", T::kName);
      return 0;
    }
    size_t queued = 0;
    for (const Address &peer : m_transport->connectedPeers()) {
      queued +=
          m_transport->send(peer, channel, writer.data().data(), writer.size());
    }
    return queued;
  }

  // `now` is seconds on one steady clock.
  void update(double now);

  void bindWorld(ecs::World *world) {
    m_binding->world = world;
  }

  // Dropped arrivals: unknown type, wrong channel or unparsable.
  uint64_t rejectedMessages() const {
    return m_rejectedMessages;
  }

private:
  // Shared with the handlers registerMessage() installs, so moving a Network
  // (an ECS resource) leaves them valid.
  struct WorldBinding {
    ecs::World *world = nullptr;
  };

  void dispatch(const Event &event);
  void updateNatDiscovery(double now);

  // Where the lookup thread leaves its answer (lookups can take seconds).
  struct ResolveJob {
    std::mutex mutex;
    bool done = false;
    std::vector<Address> servers;
  };

  std::shared_ptr<WorldBinding> m_binding;
  MessageRegistry m_registry;
  std::unordered_set<std::type_index> m_forwarded;
  // The transport refers to the socket, so both live on the heap and survive a
  // move.
  std::unique_ptr<IDatagramSocket> m_socket;
  // m_socket seen as a StunSocket; null when closed.
  StunSocket *m_stun = nullptr;
  std::shared_ptr<ResolveJob> m_resolve;
  bool m_discoveryPending = false;
  std::vector<std::function<void(bool, const NatInfo &)>> m_onNatDiscovery;
  std::unique_ptr<Transport> m_transport;
  std::vector<std::function<void(const Address &)>> m_onConnected;
  std::vector<std::function<void(const Address &, DisconnectReason)>>
      m_onDisconnected;
  std::vector<std::function<void(const Address &, DisconnectReason)>>
      m_onConnectFailed;
  TransportConfig m_config;
  uint64_t m_rejectedMessages = 0;
};

} // namespace componeng::net
