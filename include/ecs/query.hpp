#pragma once
#include "ecs/archetype.hpp"
#include "ecs/component_registry.hpp"
#include "ecs/entity.hpp"

struct QueryDesc {
  Signature required;
  Signature excluded;
};

template <typename... Req> class Query {
public:
  Query(std::array<Archetype, MAX_ARCHETYPES> &archetypes,
        ComponentRegistry &registry)
      : archetypes(archetypes), registry(registry) {
    desc.required = registry.MakeSignature<Req...>();
  }

  std::array<Archetype, MAX_ARCHETYPES> &archetypes;
  ComponentRegistry &registry;
  QueryDesc desc;

  bool Matches(Archetype &archetype) {
    return (archetype.signature & desc.required) == desc.required &&
           (archetype.signature & desc.excluded).none();
  }

  template <typename T> ComponentColumn *TryGetColumn(Archetype &archetype) {
    auto id = registry.GetComponentID<T>();
    if (archetype.signature.test(id))
      return &archetype.GetColumn(id);
    return nullptr;
  }

  template <typename... Excl> Query<Req...> Exclude() {
    desc.excluded = registry.MakeSignature<Excl...>();
    return *this;
  }

  template <typename... Opts, typename Fn, size_t... I, size_t... J>
  void callEachOptional(Fn &fn, ComponentColumn **reqCols,
                        ComponentColumn **optCols, size_t i,
                        std::index_sequence<I...>, std::index_sequence<J...>) {
    fn(reqCols[I]->template Get<Req>(i)...,
       (optCols[J] ? &optCols[J]->template Get<Opts>(i) : nullptr)...);
  }

  template <typename... Opts, typename Fn> void eachOptional(Fn fn) {
    for (Archetype &archetype : archetypes) {
      if (!Matches(archetype))
        continue;

      ComponentColumn *reqCols[] = {
          &archetype.GetColumn(registry.GetComponentID<Req>())...};

      ComponentColumn *optCols[] = {TryGetColumn<Opts>(archetype)...};

      size_t n = archetype.GetEntityCount();
      for (size_t i = 0; i < n; ++i)
        callEachOptional<Opts...>(fn, reqCols, optCols, i,
                                  std::index_sequence_for<Req...>{},
                                  std::index_sequence_for<Opts...>{});
    }
  }

  template <typename... Opts, typename Fn, size_t... I, size_t... J>
  void callEachWithEntityOptional(Fn &fn, EntityID entity,
                                  ComponentColumn **reqCols,
                                  ComponentColumn **optCols, size_t i,
                                  std::index_sequence<I...>,
                                  std::index_sequence<J...>) {
    fn(entity, reqCols[I]->template Get<Req>(i)...,
       (optCols[J] ? &optCols[J]->template Get<Opts>(i) : nullptr)...);
  }

  template <typename... Opts, typename Fn> void eachWithEntityOptional(Fn fn) {
    for (Archetype &archetype : archetypes) {
      if (!Matches(archetype))
        continue;

      ComponentColumn *reqCols[] = {
          &archetype.GetColumn(registry.GetComponentID<Req>())...};

      ComponentColumn *optCols[] = {TryGetColumn<Opts>(archetype)...};

      size_t n = archetype.GetEntityCount();
      for (size_t i = 0; i < n; ++i)
        callEachWithEntityOptional(fn, archetype.GetEntityForRow(i), reqCols,
                                   optCols, i,
                                   std::index_sequence_for<Req...>{},
                                   std::index_sequence_for<Opts...>{});
    }
  }
};
