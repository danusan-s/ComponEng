#pragma once
#include "ecs/archetype.hpp"
#include "ecs/component_registry.hpp"
#include "ecs/entity.hpp"

template <typename... Ts> class Query {
public:
  Query(std::array<Archetype, MAX_ARCHETYPES> &archetypes,
        ComponentRegistry &registry)
      : archetypes(archetypes), registry(registry) {
    required = registry.MakeSignature<Ts...>();
  }

  std::array<Archetype, MAX_ARCHETYPES> &archetypes;
  ComponentRegistry &registry;
  Signature required;

  template <typename Fn> void each(Fn fn) {
    for (Archetype &archetype : archetypes) {
      if ((archetype.signature & required) != required)
        continue;

      size_t n = archetype.GetEntityCount();
      for (size_t i = 0; i < n; ++i)
        fn(archetype.GetColumn(registry.GetComponentID<Ts>())
               .template Get<Ts>(i)...);
    }
  }
};
