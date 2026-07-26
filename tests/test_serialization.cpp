#include "componeng/components/audio_component.hpp"
#include "componeng/components/camera_component.hpp"
#include "componeng/components/collider_component.hpp"
#include "componeng/components/material_component.hpp"
#include "componeng/components/mesh_component.hpp"
#include "componeng/components/rigidbody_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/ecs/component_registry.hpp"
#include "componeng/ecs/archetype.hpp"
#include "componeng/ecs/entity.hpp"
#include "gtest/gtest.h"
#include <cstring>

using namespace componeng::components;
using namespace componeng::core;
using namespace componeng::ecs;

template <typename T>
ComponentID reg(ComponentRegistry &r) {
  return r.registerComponent<T>();
}

TEST(SerializationTest, RoundTripMeshComponent) {
  MeshComponent orig;
  orig.meshName = "test_cube";

  auto json = ComponentSerializer<MeshComponent>::serialize(orig);
  auto restored = ComponentSerializer<MeshComponent>::deserialize(json);

  EXPECT_EQ(orig.meshName, restored.meshName);
}

TEST(SerializationTest, RoundTripLongStringMesh) {
  MeshComponent orig;
  orig.meshName = "this_is_a_very_long_mesh_name_that_exceeds_SSO_limit";

  auto json = ComponentSerializer<MeshComponent>::serialize(orig);
  auto restored = ComponentSerializer<MeshComponent>::deserialize(json);

  EXPECT_EQ(orig.meshName, restored.meshName);
}

TEST(SerializationTest, RoundTripMaterialComponent) {
  MaterialComponent orig;
  orig.color = glm::vec3(0.5f, 0.7f, 0.3f);
  orig.textureName = "brick_wall_texture_1024x1024.png";
  orig.shaderName = "pbr";

  auto json = ComponentSerializer<MaterialComponent>::serialize(orig);
  auto restored = ComponentSerializer<MaterialComponent>::deserialize(json);

  EXPECT_EQ(orig.color, restored.color);
  EXPECT_EQ(orig.textureName, restored.textureName);
  EXPECT_EQ(orig.shaderName, restored.shaderName);
}

TEST(SerializationTest, RoundTripAudioComponent) {
  AudioComponent orig;
  orig.audioName = "explosion_sound_effect_high_quality";
  orig.playOnAwake = false;
  orig.loop = true;
  orig.volume = 0.75f;

  auto json = ComponentSerializer<AudioComponent>::serialize(orig);
  auto restored = ComponentSerializer<AudioComponent>::deserialize(json);

  EXPECT_EQ(orig.audioName, restored.audioName);
  EXPECT_EQ(orig.playOnAwake, restored.playOnAwake);
  EXPECT_EQ(orig.loop, restored.loop);
  EXPECT_EQ(orig.volume, restored.volume);
}

TEST(SerializationTest, ArchetypeMigrationPreservesLongStrings) {
  ComponentRegistry reg;
  ComponentID meshCID = reg.registerComponent<MeshComponent>();

  Signature sig1;
  sig1.set(meshCID);
  Archetype arch1;
  arch1.init(sig1, reg);

  arch1.addEntity(1);
  void *slot1 = arch1.getColumn(meshCID).at(0);
  new (slot1) MeshComponent();
  static_cast<MeshComponent *>(slot1)->meshName =
      "very_long_mesh_name_that_exceeds_sso_for_testing_purposes";

  // Migrate to same-signature archetype using memcpy (safe for trivially-copyable Name)
  Signature sig2 = sig1;
  Archetype arch2;
  arch2.init(sig2, reg);
  arch2.addEntity(1);
  std::memcpy(arch2.getColumn(meshCID).at(0), arch1.getColumn(meshCID).at(0),
              arch1.getColumn(meshCID).m_stride);
  arch1.removeEntity(1);

  auto &moved =
      static_cast<MeshComponent *>(arch2.getColumn(meshCID).at(0))->meshName;
  EXPECT_EQ(moved, "very_long_mesh_name_that_exceeds_sso_for_testing_purposes");

  auto json = ComponentSerializer<MeshComponent>::serialize(
      *static_cast<MeshComponent *>(arch2.getColumn(meshCID).at(0)));
  EXPECT_EQ(json["meshName"],
            "very_long_mesh_name_that_exceeds_sso_for_testing_purposes");
}

TEST(SerializationTest, MultipleEntityMigrations) {
  ComponentRegistry reg;
  ComponentID meshCID = reg.registerComponent<MeshComponent>();

  Signature sig1;
  sig1.set(meshCID);
  Archetype arch1;
  arch1.init(sig1, reg);

  for (int i = 0; i < 3; ++i) {
    arch1.addEntity(i + 10);
    void *slot = arch1.getColumn(meshCID).at(i);
    new (slot) MeshComponent();
    static_cast<MeshComponent *>(slot)->meshName =
        Name(("entity_" + std::to_string(i) + "_with_a_long_name_to_test").c_str());
  }

  // Migrate using memcpy + removeEntity (the archetype swap pattern)
  Signature sig2 = sig1;
  Archetype arch2;
  arch2.init(sig2, reg);
  // entity 10 is at row 0 in arch1; migrate it to arch2
  arch2.addEntity(10);
  std::memcpy(arch2.getColumn(meshCID).at(0), arch1.getColumn(meshCID).at(0),
              arch1.getColumn(meshCID).m_stride);
  arch1.removeEntity(10);

  // entity 11 is now at row 0 (entity 10 was at 0, removed, entity 12
  // was at row 2 and got swapped to row 0). entity 11 remains at row 1.
  arch2.addEntity(11);
  std::memcpy(arch2.getColumn(meshCID).at(1), arch1.getColumn(meshCID).at(1),
              arch1.getColumn(meshCID).m_stride);
  arch1.removeEntity(11);

  auto &moved =
      static_cast<MeshComponent *>(arch2.getColumn(meshCID).at(0))->meshName;
  EXPECT_EQ(moved, "entity_0_with_a_long_name_to_test");

  auto json = ComponentSerializer<MeshComponent>::serialize(
      *static_cast<MeshComponent *>(arch2.getColumn(meshCID).at(0)));
  EXPECT_EQ(json["meshName"], "entity_0_with_a_long_name_to_test");
}
