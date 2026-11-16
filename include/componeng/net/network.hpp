#pragma once

#include "componeng/ecs/world.hpp"
#include "componeng/events/network_event.hpp"
#include "componeng/net/address.hpp"
#include "componeng/net/idatagram_socket.hpp"
#include "componeng/net/message_registry.hpp"
#include "componeng/net/transport.hpp"
#include "componeng/utils/logger.hpp"

#include <cstdint>
#include <functional>
#include <memory>
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

  bool isConnected(const Address &peer) const;
  std::vector<Address> peers() const;
  ConnectionStats stats(const Address &peer) const;

  // Registers a message type on its channel (Reliable for what must arrive,
  // Unreliable for what goes stale). Every peer must register the same types;
  // unknown ones are counted in rejectedMessages(). Also emits
  // MessageReceivedEvent<T>.
  template <typename T>
  bool registerMessage(Channel channel = Channel::Reliable) {
    if (!m_registry.add<T>(channel)) {
      return false;
    }
    if (m_registry.contains<T>() &&
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

  std::shared_ptr<WorldBinding> m_binding;
  MessageRegistry m_registry;
  std::unordered_set<std::type_index> m_forwarded;
  // The transport refers to the socket, so both live on the heap and survive a
  // move.
  std::unique_ptr<IDatagramSocket> m_socket;
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
