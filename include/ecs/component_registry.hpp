#pragma once

#include "entity.hpp"
#include <array>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

struct ComponentInfo {
  const char* name;
  size_t size;
};

class ComponentRegistry {
private:
  std::unordered_map<std::type_index, ComponentID> m_typeToID;
  std::array<ComponentInfo, MAX_COMPONENTS> m_componentInfos;
  ComponentID m_nextComponentID = 0;

public:
  template <typename T> ComponentID registerComponent() {
    static_assert(!std::is_pointer_v<T>,
                  "Component type must not be a pointer");
    static_assert(!std::is_reference_v<T>,
                  "Component type must not be a reference");
    static_assert(std::is_move_constructible_v<T>,
                  "Component must be move-constructible");

    const std::type_index key = typeid(T);
    auto it = m_typeToID.find(key);
    if (it != m_typeToID.end())
      return it->second;

    if (m_nextComponentID >= MAX_COMPONENTS)
      throw std::runtime_error("ComponentRegistry: MAX_COMPONENTS exceeded");

    ComponentID id = m_nextComponentID++;
    m_typeToID[key] = id;
    m_componentInfos[id].name = key.name();
    m_componentInfos[id].size = sizeof(T);
    return id;
  }

  template <typename T> ComponentID getComponentID() const {
    auto it = m_typeToID.find(typeid(T));
    if (it == m_typeToID.end())
      throw std::runtime_error(
          "ComponentRegistry: unregistered component type");
    return it->second;
  }

  ComponentID getComponentCount() const {
    return m_nextComponentID;
  }

  ComponentInfo& getComponentInfo(ComponentID id) {
    if (id >= m_nextComponentID)
      throw std::runtime_error("ComponentRegistry: invalid ComponentID");
    return m_componentInfos[id];
  }

  template <typename... Ts> Signature makeSignature() const {
    Signature sig;
    (sig.set(getComponentID<Ts>()), ...);
    return sig;
  }
};
