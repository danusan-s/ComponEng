#pragma once
#include "entity.hpp"
#include <array>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

struct ComponentInfo {
  const char *name;
  size_t size;
};

// ---------------------------------------------------------------------------
// ComponentRegistry
//
// Central registry that assigns a stable ComponentId to every C++ type.
// IDs are assigned in registration order and never change for the lifetime of
// the registry (i.e. the lifetime of the World).
//
// Thread-safety: registration should happen once at startup before any systems
// run. Lookups after that are read-only and need no lock.
// ---------------------------------------------------------------------------

class ComponentRegistry {
private:
  std::unordered_map<std::type_index, ComponentID> typeToID;
  std::array<ComponentInfo, MAX_COMPONENTS> componentInfos;
  ComponentID nextComponentID = 0;

public:
  template <typename T> ComponentID RegisterComponent() {
    static_assert(!std::is_pointer_v<T>,
                  "Component type must not be a pointer");
    static_assert(!std::is_reference_v<T>,
                  "Component type must not be a reference");
    static_assert(std::is_move_constructible_v<T>,
                  "Component must be move-constructible");

    const std::type_index key = typeid(T);
    auto it = typeToID.find(key);
    if (it != typeToID.end())
      return it->second;

    if (nextComponentID >= MAX_COMPONENTS)
      throw std::runtime_error("ComponentRegistry: MAX_COMPONENTS exceeded");

    ComponentID id = nextComponentID++;
    typeToID[key] = id;
    componentInfos[id].name = key.name();
    componentInfos[id].size = sizeof(T);
    return id;
  }

  // Retrieve ComponentId for an already-registered type. Throws if not found.
  template <typename T> ComponentID GetComponentID() const {
    auto it = typeToID.find(typeid(T));
    if (it == typeToID.end())
      throw std::runtime_error(
          "ComponentRegistry: unregistered component type");
    return it->second;
  }

  ComponentID GetComponentCount() const {
    return nextComponentID;
  }

  ComponentInfo &GetComponentInfo(ComponentID id) {
    if (id >= nextComponentID)
      throw std::runtime_error("ComponentRegistry: invalid ComponentID");
    return componentInfos[id];
  }

  // Build a Signature from a list of component types.
  template <typename... Ts> Signature MakeSignature() const {
    Signature sig;
    (sig.set(GetComponentID<Ts>()), ...);
    return sig;
  }
};
