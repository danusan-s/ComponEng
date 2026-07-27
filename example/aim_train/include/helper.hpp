#pragma once

#include "componeng/components/collider_component.hpp"
#include "componeng/components/material_component.hpp"
#include "componeng/components/mesh_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/ecs/world.hpp"
#include <random>

using namespace componeng;

constexpr float kSpawnInner = 10.0f;
constexpr float kSpawnOuter = 30.0f;
constexpr float kSpawnYMin = 0.5f;
constexpr float kSpawnYMax = 6.0f;
constexpr float kOrbScaleMin = 0.3f;
constexpr float kOrbScaleMax = 0.8f;

static inline void spawnOrb(ecs::World &world) {
  static std::mt19937 gen(std::random_device{}());
  static std::uniform_real_distribution<float> distScale(kOrbScaleMin,
                                                         kOrbScaleMax);
  static std::uniform_real_distribution<float> distY(kSpawnYMin, kSpawnYMax);
  static std::uniform_real_distribution<float> distCoord(-kSpawnOuter,
                                                         kSpawnOuter);

  float x, z;
  do {
    x = distCoord(gen);
  } while (std::abs(x) < kSpawnInner);

  do {
    z = distCoord(gen);
  } while (std::abs(z) < kSpawnInner);

  float s = distScale(gen);
  ecs::EntityID e = world.createEntity();
  world.addComponents(
      e, components::MeshComponent{.meshName = "sphere"},
      components::TransformComponent{.position = core::Vec3(x, distY(gen), z),
                                     .scale = core::Vec3(s)},
      components::MaterialComponent{.color = core::Vec3(0.0f, 1.0f, 1.0f),
                                    .textureName = "white",
                                    .shaderName = "default"},
      components::ColliderComponent{
          .type = components::ColliderType::Sphere,
          .transform = components::TransformComponent{.scale = core::Vec3(1)}});
}
