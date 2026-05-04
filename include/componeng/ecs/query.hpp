#pragma once

#include "componeng/ecs/archetype.hpp"
#include "componeng/ecs/component_registry.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/ecs/thread_pool.hpp"
#include <algorithm>
#include <future>
#include <vector>

struct QueryDesc {
  Signature required;
  Signature excluded;
};

/**
 * @brief Typed query over entities matching a set of required component types.
 *
 * Iterates all archetypes whose Signature contains every requested component
 * and invokes a user-provided callback for each matching entity.
 * Also supports exclude() to filter out entities with specific components,
 * and eachParallel() to distribute work across the engine's ThreadPool.
 *
 * @tparam Req... Component types that an entity must have to match.
 */
template <typename... Req> class Query {
public:
  Query(std::array<Archetype, MAX_ARCHETYPES> &archetypes,
        ComponentRegistry &registry)
      : archetypes(archetypes), registry(registry) {
    desc.required = registry.makeSignature<Req...>();
  }

  std::array<Archetype, MAX_ARCHETYPES> &archetypes;
  ComponentRegistry &registry;
  QueryDesc desc;

  bool matches(const Archetype &archetype) {
    return (archetype.m_signature & desc.required) == desc.required &&
           (archetype.m_signature & desc.excluded).none();
  }

  template <typename... Excl> Query<Req...> &exclude() {
    desc.excluded = registry.makeSignature<Excl...>();
    return *this;
  }

  template <typename Fn, size_t... I>
  void invokeCallback(Fn &fn, ComponentColumn **reqCols, size_t i,
                      std::index_sequence<I...>) {
    fn(reqCols[I]->template get<Req>(i)...);
  }

  template <typename Fn> void each(Fn fn) {
    for (Archetype &archetype : archetypes) {
      if (!matches(archetype))
        continue;

      ComponentColumn *reqCols[] = {
          &archetype.getColumn(registry.getComponentID<Req>())...};

      size_t n = archetype.getEntityCount();
      for (size_t i = 0; i < n; ++i)
        invokeCallback(fn, reqCols, i, std::index_sequence_for<Req...>{});
    }
  }

  template <typename Fn, size_t... I>
  void invokeCallbackWithEntity(Fn &fn, EntityID entity,
                                ComponentColumn **reqCols, size_t i,
                                std::index_sequence<I...>) {
    fn(entity, reqCols[I]->template get<Req>(i)...);
  }

  template <typename Fn> void eachWithEntity(Fn fn) {
    for (Archetype &archetype : archetypes) {
      if (!matches(archetype))
        continue;

      ComponentColumn *reqCols[] = {
          &archetype.getColumn(registry.getComponentID<Req>())...};

      size_t n = archetype.getEntityCount();
      for (size_t i = 0; i < n; ++i)
        invokeCallbackWithEntity(fn, archetype.getEntityForRow(i), reqCols, i,
                                 std::index_sequence_for<Req...>{});
    }
  }

  template <typename Fn> void eachParallel(ThreadPool &pool, Fn fn) {
    std::vector<std::future<void>> futures;

    for (size_t a = 0; a < archetypes.size(); ++a) {
      Archetype &archetype = archetypes[a];
      if (!matches(archetype))
        continue;

      size_t n = archetype.getEntityCount();
      if (n == 0)
        continue;

      // If less than 50 running non parallel is faster as there is a overhead
      // when we create task and then the pool unlocks and runs it. For small
      // number of entities, that overhead is more than the time it takes to
      // just run the loop in the current thread.
      if (n < 50) {
        ComponentColumn *reqCols[] = {
            &archetype.getColumn(registry.getComponentID<Req>())...};
        for (size_t i = 0; i < n; ++i) {
          invokeCallback(fn, reqCols, i, std::index_sequence_for<Req...>{});
        }
        continue;
      }

      size_t numChunks = std::min(n, pool.threadCount());
      size_t chunkSize = std::max(size_t(1), n / numChunks);

      for (size_t c = 0; c < numChunks; ++c) {
        size_t start = c * chunkSize;
        size_t end = (c == numChunks - 1) ? n : start + chunkSize;

        futures.push_back(pool.submit([this, a, start, end, fn]() {
          Archetype &archetype = archetypes[a];
          ComponentColumn *reqCols[] = {
              &archetype.getColumn(registry.getComponentID<Req>())...};

          for (size_t i = start; i < end; ++i) {
            invokeCallback(fn, reqCols, i, std::index_sequence_for<Req...>{});
          }
        }));
      }
    }

    for (auto &f : futures)
      f.wait();
  }
};

// Optional components make it so system implementation has to check if
// component is nullptr or not which means per entity branching. This is a
// tradeoff that allows more flexible systems at the cost of some performance.
// We can just split queries into multiple queries if we want to avoid that
// branching in hot code paths.
//
//   template <typename T> ComponentColumn *TryGetColumn(Archetype &archetype)
//   {
//     auto id = registry.GetComponentID<T>();
//     if (archetype.signature.test(id))
//       return &archetype.GetColumn(id);
//     return nullptr;
//   }
//
//   template <typename... Opts, typename Fn, size_t... I, size_t... J>
//   void callEachOptional(Fn &fn, ComponentColumn **reqCols,
//                         ComponentColumn **optCols, size_t i,
//                         std::index_sequence<I...>,
//                         std::index_sequence<J...>)
//                         {
//     fn(reqCols[I]->template Get<Req>(i)...,
//        (optCols[J] ? &optCols[J]->template Get<Opts>(i) : nullptr)...);
//   }
//
//   template <typename... Opts, typename Fn> void eachOptional(Fn fn) {
//     for (Archetype &archetype : archetypes) {
//       if (!Matches(archetype))
//         continue;
//
//       ComponentColumn *reqCols[] = {
//           &archetype.GetColumn(registry.GetComponentID<Req>())...};
//
//       ComponentColumn *optCols[] = {TryGetColumn<Opts>(archetype)...};
//
//       size_t n = archetype.GetEntityCount();
//       for (size_t i = 0; i < n; ++i)
//         callEachOptional<Opts...>(fn, reqCols, optCols, i,
//                                   std::index_sequence_for<Req...>{},
//                                   std::index_sequence_for<Opts...>{});
//     }
//   }
//
//   template <typename... Opts, typename Fn, size_t... I, size_t... J>
//   void callEachWithEntityOptional(Fn &fn, EntityID entity,
//                                   ComponentColumn **reqCols,
//                                   ComponentColumn **optCols, size_t i,
//                                   std::index_sequence<I...>,
//                                   std::index_sequence<J...>) {
//     fn(entity, reqCols[I]->template Get<Req>(i)...,
//        (optCols[J] ? &optCols[J]->template Get<Opts>(i) : nullptr)...);
//   }
//
//   template <typename... Opts, typename Fn> void eachWithEntityOptional(Fn
//   fn)
//   {
//     for (Archetype &archetype : archetypes) {
//       if (!Matches(archetype))
//         continue;
//
//       ComponentColumn *reqCols[] = {
//           &archetype.GetColumn(registry.GetComponentID<Req>())...};
//
//       ComponentColumn *optCols[] = {TryGetColumn<Opts>(archetype)...};
//
//       size_t n = archetype.GetEntityCount();
//       for (size_t i = 0; i < n; ++i)
//         callEachWithEntityOptional(fn, archetype.GetEntityForRow(i),
//         reqCols,
//                                    optCols, i,
//                                    std::index_sequence_for<Req...>{},
//                                    std::index_sequence_for<Opts...>{});
//     }
//   }
