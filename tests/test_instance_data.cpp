#include "componeng/core/transform_component.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset/diffuse.hpp"
#include "componeng/renderer/asset/material.hpp"
#include "componeng/renderer/batching/instance_data.hpp"
#include "componeng/renderer/batching/render_queue.hpp"
#include "componeng/renderer/component/material_component.hpp"
#include "gtest/gtest.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace componeng::core;
using namespace componeng::ecs;
using namespace componeng::renderer;

namespace {

constexpr float kEps = 1e-5f;

// The composition the engine used before TransformComponent::toMatrix().
Mat4 referenceModelMatrix(const TransformComponent &t) {
  Mat4 m(1.0f);
  m = translate(m, t.position);
  m = rotate(m, radians(t.rotation.x), Vec3(1.0f, 0.0f, 0.0f));
  m = rotate(m, radians(t.rotation.y), Vec3(0.0f, 1.0f, 0.0f));
  m = rotate(m, radians(t.rotation.z), Vec3(0.0f, 0.0f, 1.0f));
  m = scale(m, t.scale);
  return m;
}

::testing::AssertionResult SameMat4(const Mat4 &a, const Mat4 &b,
                                    float eps = kEps) {
  float worst = 0.0f;
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r)
      worst = std::max(worst, std::fabs(a[c][r] - b[c][r]));
  if (worst <= eps)
    return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure() << "worst |a-b| = " << worst;
}

// instanceModel (mat4, 4 rows) + instanceColor (vec4) + wind (float).
api::VertexLayout modelColorWindLayout() {
  api::VertexLayout layout;
  for (int row = 0; row < 4; ++row) {
    layout.attributes.push_back(
        {Name("instanceModel"), uint32_t(row * 16), 4, false, row});
  }
  layout.attributes.push_back({Name("instanceColor"), 64, 4, false, -1});
  layout.attributes.push_back({Name("wind"), 80, 1, false, -1});
  layout.stride = 84;
  return layout;
}

} // namespace

// --- TransformComponent::toMatrix -------------------------------------------

TEST(TransformToMatrix, MatchesComposedTranslateRotateScale) {
  const TransformComponent cases[] = {
      {{0, 0, 0}, {0, 0, 0}, {1, 1, 1}},
      {{1, 2, 3}, {0, 0, 0}, {2, 2, 2}},
      {{0, 0, 0}, {90, 0, 0}, {1, 1, 1}},
      {{0, 0, 0}, {0, 90, 0}, {1, 1, 1}},
      {{0, 0, 0}, {0, 0, 90}, {1, 1, 1}},
      {{1, 2, 3}, {90, 45, 30}, {2, 0.5f, 3}},
      {{-5, 0.5f, 10}, {17, -69, 137}, {1, 1, 1}},
      {{7, -3, 2}, {180, 0, 45}, {0.1f, 10, 1}},
      {{-2, -2, -2}, {-40, 109, -155}, {-1, 3, 0.5f}},
      {{0, 0, 0}, {720, -360, 405}, {1, 1, 1}},
  };
  int i = 0;
  for (const auto &t : cases) {
    EXPECT_TRUE(SameMat4(t.toMatrix(), referenceModelMatrix(t))) << "case " << i++;
  }
}

TEST(TransformToMatrix, UnrotatedIsExactlyTranslationTimesScale) {
  const TransformComponent t{{4, 5, 6}, {0, 0, 0}, {2, 3, 4}};
  const Mat4 m = t.toMatrix();
  EXPECT_EQ(m[0][0], 2.0f);
  EXPECT_EQ(m[1][1], 3.0f);
  EXPECT_EQ(m[2][2], 4.0f);
  EXPECT_EQ(m[3][0], 4.0f);
  EXPECT_EQ(m[3][1], 5.0f);
  EXPECT_EQ(m[3][2], 6.0f);
  EXPECT_EQ(m[3][3], 1.0f);
  EXPECT_EQ(m[0][1], 0.0f);
  EXPECT_EQ(m[1][0], 0.0f);
}

// --- TransformMatrixCache ---------------------------------------------------

TEST(TransformMatrixCache, ReturnsTheSameMatrixAsToMatrix) {
  TransformMatrixCache cache;
  const TransformComponent t{{1, 2, 3}, {10, 20, 30}, {1, 2, 3}};
  EXPECT_TRUE(SameMat4(cache.get(5, t), t.toMatrix(), 0.0f));
}

TEST(TransformMatrixCache, ReusesEntryWhileTransformIsUnchanged) {
  TransformMatrixCache cache;
  const TransformComponent t{{1, 2, 3}, {10, 20, 30}, {1, 2, 3}};
  const Mat4 *first = &cache.get(7, t);
  const Mat4 *second = &cache.get(7, t);
  EXPECT_EQ(first, second);
}

TEST(TransformMatrixCache, RebuildsWhenAnyComponentChanges) {
  TransformMatrixCache cache;
  TransformComponent t{{1, 2, 3}, {10, 20, 30}, {1, 2, 3}};
  cache.get(3, t);

  // Each of the nine floats must invalidate the entry on its own.
  float *fields[] = {&t.position.x, &t.position.y, &t.position.z,
                     &t.rotation.x, &t.rotation.y, &t.rotation.z,
                     &t.scale.x,    &t.scale.y,    &t.scale.z};
  for (float *field : fields) {
    *field += 1.0f;
    EXPECT_TRUE(SameMat4(cache.get(3, t), t.toMatrix(), 0.0f));
  }
}

TEST(TransformMatrixCache, EntitiesDoNotShareEntries) {
  TransformMatrixCache cache;
  const TransformComponent a{{1, 0, 0}, {0, 0, 0}, {1, 1, 1}};
  const TransformComponent b{{2, 0, 0}, {0, 0, 0}, {1, 1, 1}};
  EXPECT_EQ(cache.get(1, a)[3][0], 1.0f);
  EXPECT_EQ(cache.get(2, b)[3][0], 2.0f);
  EXPECT_EQ(cache.get(1, a)[3][0], 1.0f);
}

// --- InstanceWriter ---------------------------------------------------------

TEST(InstanceWriter, WritesEachAttributeAtItsOffset) {
  const api::VertexLayout layout = modelColorWindLayout();
  std::vector<float> data(layout.stride / sizeof(float), -1.0f);
  InstanceWriter w(layout, data.data());

  Mat4 model(1.0f);
  model[3][0] = 9.0f;
  model[0][0] = 2.0f;
  w.set(Name("instanceModel"), model);
  w.set(Name("instanceColor"), Vec4(0.1f, 0.2f, 0.3f, 0.4f));
  w.set(Name("wind"), 0.75f);

  // Slot r holds column r of the matrix, i.e. 16 floats == value_ptr order.
  for (int i = 0; i < 16; ++i) {
    EXPECT_EQ(data[i], value_ptr(model)[i]) << i;
  }
  EXPECT_EQ(data[16], 0.1f);
  EXPECT_EQ(data[17], 0.2f);
  EXPECT_EQ(data[18], 0.3f);
  EXPECT_EQ(data[19], 0.4f);
  EXPECT_EQ(data[20], 0.75f);
}

TEST(InstanceWriter, IgnoresNamesTheShaderDoesNotDeclare) {
  const api::VertexLayout layout = modelColorWindLayout();
  std::vector<float> data(layout.stride / sizeof(float), 0.0f);
  InstanceWriter w(layout, data.data());

  w.set(Name("notAnAttribute"), Vec3(1, 2, 3));
  w.set(Name("notAnAttribute"), Mat4(1.0f));

  EXPECT_TRUE(std::all_of(data.begin(), data.end(),
                          [](float f) { return f == 0.0f; }));
}

TEST(InstanceWriter, TruncatesValuesWiderThanTheAttribute) {
  // "a" is a vec3 followed directly by "b"; writing a Vec4 into "a" must not
  // clobber "b".
  api::VertexLayout layout;
  layout.attributes.push_back({Name("a"), 0, 3, false, -1});
  layout.attributes.push_back({Name("b"), 12, 1, false, -1});
  layout.stride = 16;

  std::vector<float> data(4, 0.0f);
  InstanceWriter w(layout, data.data());
  w.set(Name("b"), 5.0f);
  w.set(Name("a"), Vec4(1, 2, 3, 99));

  EXPECT_EQ(data[0], 1.0f);
  EXPECT_EQ(data[1], 2.0f);
  EXPECT_EQ(data[2], 3.0f);
  EXPECT_EQ(data[3], 5.0f);
}

TEST(InstanceWriter, LeavesNarrowerValuesTailUntouched) {
  api::VertexLayout layout;
  layout.attributes.push_back({Name("c"), 0, 4, false, -1});
  layout.stride = 16;

  std::vector<float> data(4, 7.0f);
  InstanceWriter w(layout, data.data());
  w.set(Name("c"), Vec2(1, 2));

  EXPECT_EQ(data[0], 1.0f);
  EXPECT_EQ(data[1], 2.0f);
  EXPECT_EQ(data[2], 7.0f);
  EXPECT_EQ(data[3], 7.0f);
}

TEST(InstanceWriter, VariantOverloadDispatchesByType) {
  const api::VertexLayout layout = modelColorWindLayout();
  std::vector<float> data(layout.stride / sizeof(float), 0.0f);
  InstanceWriter w(layout, data.data());

  w.set(Name("wind"), UniformMap::mapped_type(0.5f));
  w.set(Name("instanceColor"), UniformMap::mapped_type(Vec4(1, 2, 3, 4)));

  EXPECT_EQ(data[20], 0.5f);
  EXPECT_EQ(data[16], 1.0f);
  EXPECT_EQ(data[19], 4.0f);
}

// --- Materials --------------------------------------------------------------

namespace {

std::unique_ptr<World> makeWorld() {
  auto world = std::make_unique<World>();
  world->init();
  world->registerComponents<TransformComponent, ColorComponent>();
  return world;
}

// Written against the old map-based API only.
class LegacyMaterial : public IMaterial {
public:
  using IMaterial::IMaterial;
  void buildInstanceData(World &, EntityID,
                         UniformMap &out) const override {
    out["instanceColor"] = Vec4(0.5f, 0.25f, 0.125f, 1.0f);
    out["wind"] = 3.0f;
  }
};

} // namespace

TEST(MaterialInstanceData, LegacyBuildInstanceDataStillWorks) {
  auto world = makeWorld();
  EntityID e = world->createEntity();
  world->addComponents(e, TransformComponent{});

  LegacyMaterial material(1, 1);
  const api::VertexLayout layout = modelColorWindLayout();
  material.setVertexLayout(layout);

  std::vector<float> data(layout.stride / sizeof(float), 0.0f);
  InstanceWriter writer(layout, data.data());
  TransformMatrixCache cache;
  InstanceContext ctx(*world, e, world->getComponent<TransformComponent>(e),
                      cache);
  material.writeInstanceData(ctx, writer);

  EXPECT_EQ(data[16], 0.5f);
  EXPECT_EQ(data[17], 0.25f);
  EXPECT_EQ(data[18], 0.125f);
  EXPECT_EQ(data[20], 3.0f);
}

TEST(MaterialInstanceData, DiffuseWritesModelAndColor) {
  auto world = makeWorld();
  EntityID e = world->createEntity();
  TransformComponent t{{1, 2, 3}, {0, 30, 0}, {2, 2, 2}};
  world->addComponents(e, TransformComponent(t), ColorComponent{.color = Vec4(0.1f, 0.2f, 0.3f, 1.0f)});

  DiffuseMaterial material(1, 1);
  const api::VertexLayout layout = modelColorWindLayout();
  material.setVertexLayout(layout);

  std::vector<float> data(layout.stride / sizeof(float), 0.0f);
  InstanceWriter writer(layout, data.data());
  TransformMatrixCache cache;
  InstanceContext ctx(*world, e, world->getComponent<TransformComponent>(e),
                      cache);
  material.writeInstanceData(ctx, writer);

  const Mat4 expected = referenceModelMatrix(t);
  for (int i = 0; i < 16; ++i) {
    EXPECT_NEAR(data[i], value_ptr(expected)[i], kEps) << i;
  }
  EXPECT_EQ(data[16], 0.1f);
  EXPECT_EQ(data[17], 0.2f);
  EXPECT_EQ(data[18], 0.3f);
}

TEST(MaterialInstanceData, DiffuseDefaultsColorToWhiteWithoutComponent) {
  auto world = makeWorld();
  EntityID e = world->createEntity();
  world->addComponents(e, TransformComponent{});

  DiffuseMaterial material(1, 1);
  const api::VertexLayout layout = modelColorWindLayout();
  material.setVertexLayout(layout);

  std::vector<float> data(layout.stride / sizeof(float), 0.0f);
  InstanceWriter writer(layout, data.data());
  TransformMatrixCache cache;
  InstanceContext ctx(*world, e, world->getComponent<TransformComponent>(e),
                      cache);
  material.writeInstanceData(ctx, writer);

  for (int i = 16; i < 20; ++i) {
    EXPECT_EQ(data[i], 1.0f) << i;
  }
}

// --- World::tryGetComponent -------------------------------------------------

TEST(WorldTryGetComponent, ReturnsPointerOrNull) {
  auto world = makeWorld();
  EntityID e = world->createEntity();
  world->addComponents(e, TransformComponent{{1, 2, 3}, {}, {1, 1, 1}});

  auto *t = world->tryGetComponent<TransformComponent>(e);
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->position.y, 2.0f);
  EXPECT_EQ(world->tryGetComponent<ColorComponent>(e), nullptr);
}

// --- RenderQueue ------------------------------------------------------------

TEST(RenderQueue, SortsByShaderThenTextureThenMaterialThenMesh) {
  RenderQueue q;
  //            mesh material shader texture
  q.openBatch(2, 10, 2, 5);
  q.openBatch(1, 11, 1, 9);
  q.openBatch(3, 12, 1, 4);
  q.openBatch(1, 13, 1, 4);
  q.openBatch(2, 13, 1, 4);
  q.sort();

  ASSERT_EQ(q.size(), 5u);
  // shader 1 first; within it texture 4 before 9; within texture 4 material 12
  // before 13; within material 13 mesh 1 before 2. Shader 2 last.
  EXPECT_EQ(q.sorted(0).materialID, 12u);
  EXPECT_EQ(q.sorted(1).materialID, 13u);
  EXPECT_EQ(q.sorted(1).meshID, 1u);
  EXPECT_EQ(q.sorted(2).materialID, 13u);
  EXPECT_EQ(q.sorted(2).meshID, 2u);
  EXPECT_EQ(q.sorted(3).textureID, 9u);
  EXPECT_EQ(q.sorted(4).shaderID, 2u);
}

TEST(RenderQueue, OnlyOpenedBatchesExist) {
  RenderQueue q;
  q.openBatch(1, 1, 1, 1);
  q.sort();
  EXPECT_EQ(q.size(), 1u);

  q.clear();
  q.sort();
  EXPECT_EQ(q.size(), 0u) << "a cleared queue must not replay old batches";
}

TEST(RenderQueue, ClearKeepsStorageForReuse) {
  RenderQueue q;
  uint32_t first = q.openBatch(1, 1, 1, 1);
  RenderBatch &b = q.batch(first);
  b.instanceData.assign(1000, 1.0f);
  b.instanceCount = 10;
  const float *storage = b.instanceData.data();

  q.clear();
  uint32_t again = q.openBatch(2, 3, 4, 5);
  RenderBatch &reused = q.batch(again);

  EXPECT_EQ(reused.meshID, 2u);
  EXPECT_EQ(reused.instanceCount, 0u);
  EXPECT_TRUE(reused.instanceData.empty());
  EXPECT_GE(reused.instanceData.capacity(), 1000u);
  reused.instanceData.resize(1000);
  EXPECT_EQ(reused.instanceData.data(), storage)
      << "refilling to the previous size must not reallocate";
}
