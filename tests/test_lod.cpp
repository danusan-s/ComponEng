#include "render_scene_fixture.hpp"

#include "componeng/renderer/lod/lod_component.hpp"
#include "componeng/renderer/lod/lod_system.hpp"
#include "gtest/gtest.h"

using namespace rscene;

namespace {

// The fixture's camera sits at (1, 2, 3); entities are placed along +X from it
// so their distance is exactly `d`.
constexpr float kCamX = 1.0f, kCamY = 2.0f, kCamZ = 3.0f;

struct LodScene {
  std::unique_ptr<Scene> scene = makeScene();
  HandleID tri = 0, quad = 0;

  LodScene() {
    scene->world->registerComponent<LodComponent>();
    tri = scene->assets().getMeshID("tri");
    quad = scene->assets().getMeshID("quad");
  }

  EntityID spawn(float d, LodComponent lod) {
    EntityID e = scene->world->createEntity();
    scene->world->addComponents(
        e,
        TransformComponent{.position = Vec3(kCamX + d, kCamY, kCamZ),
                           .rotation = Vec3(0.0f),
                           .scale = Vec3(1.0f)},
        MeshComponent{.meshName = "tri", .visible = true},
        MaterialComponent{.materialName = "matA"}, LodComponent(lod));
    return e;
  }

  void run() {
    LodSystem system;
    system.onUpdate(SystemState{scene->world.get(), 0.016f});
  }

  MeshComponent &mesh(EntityID e) {
    return scene->world->getComponent<MeshComponent>(e);
  }

  void moveCamera(float x) {
    scene->world->getComponent<TransformComponent>(scene->camera).position =
        Vec3(x, kCamY, kCamZ);
  }
};

LodComponent twoLevels(HandleID near, HandleID far) {
  LodComponent lod;
  lod.mesh[0] = near;
  lod.mesh[1] = far;
  lod.distance[0] = 10.0f;
  lod.distance[1] = 50.0f;
  return lod;
}

} // namespace

TEST(Lod, ChoosesTheLevelWhoseRangeCoversTheDistance) {
  LodScene s;
  EntityID close = s.spawn(5.0f, twoLevels(s.tri, s.quad));
  EntityID mid = s.spawn(30.0f, twoLevels(s.tri, s.quad));
  s.run();

  EXPECT_TRUE(s.mesh(close).visible);
  EXPECT_EQ(s.mesh(close).meshID, s.tri);
  EXPECT_TRUE(s.mesh(mid).visible);
  EXPECT_EQ(s.mesh(mid).meshID, s.quad);
}

TEST(Lod, HidesTheEntityBeyondTheLastLevel) {
  LodScene s;
  EntityID far = s.spawn(80.0f, twoLevels(s.tri, s.quad));
  s.run();
  EXPECT_FALSE(s.mesh(far).visible);
}

TEST(Lod, SkipsLevelsThatHaveNoMesh) {
  LodScene s;
  // Level 0 absent: a part that only exists at medium range.
  EntityID e = s.spawn(5.0f, twoLevels(0, s.quad));
  s.run();
  EXPECT_TRUE(s.mesh(e).visible);
  EXPECT_EQ(s.mesh(e).meshID, s.quad);
}

TEST(Lod, ShowsAgainWhenTheCameraComesBack) {
  LodScene s;
  EntityID e = s.spawn(80.0f, twoLevels(s.tri, s.quad));
  s.run();
  ASSERT_FALSE(s.mesh(e).visible);

  s.moveCamera(kCamX + 75.0f); // now 5 m away
  s.run();
  EXPECT_TRUE(s.mesh(e).visible);
  EXPECT_EQ(s.mesh(e).meshID, s.tri);
}

TEST(Lod, NeverTouchesEntitiesWithoutALodComponent) {
  LodScene s;
  EntityID plain = s.scene->spawn("quad", "matA", 500.0f);
  s.run();
  EXPECT_TRUE(s.mesh(plain).visible);
}
