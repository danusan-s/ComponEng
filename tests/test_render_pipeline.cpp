#include "render_scene_fixture.hpp"

#include "gtest/gtest.h"

#include <algorithm>
#include <cstring>

using namespace rscene;


// --- Decoupling from the GL backend -----------------------------------------

TEST(RenderPipeline, AssetsAreCreatedThroughTheDevice) {
  auto s = makeScene();
  EXPECT_EQ(s->rec().shadersCreated, 2);
  EXPECT_EQ(s->rec().texturesCreated, 2);
  EXPECT_EQ(s->rec().meshesCreated, 2);
}

// --- Batching and sorting ---------------------------------------------------

TEST(RenderPipeline, EntitiesMergeIntoOneSortedDrawPerMeshAndMaterial) {
  auto s = makeScene();
  spawnMixed(*s);
  s->frame();

  const auto &draws = s->rec().draws;
  ASSERT_EQ(draws.size(), 4u);

  // Order: shader, then texture, then material, then mesh.
  //   (shader a, t1, matA, tri)  x3
  //   (shader a, t1, matA, quad) x2
  //   (shader a, t2, matC, tri)  x1
  //   (shader b, t2, matB, tri)  x4
  const uint32_t expectedInstances[] = {3, 2, 1, 4};
  const size_t expectedIndices[] = {3, 6, 3, 3};
  const int expectedShader[] = {1, 1, 1, 2};
  const int expectedTexture[] = {1, 1, 2, 2};
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(draws[i].instanceCount, expectedInstances[i]) << "draw " << i;
    EXPECT_EQ(draws[i].indexCount, expectedIndices[i]) << "draw " << i;
    EXPECT_EQ(draws[i].shaderId, expectedShader[i]) << "draw " << i;
    EXPECT_EQ(draws[i].albedoTextureId, expectedTexture[i]) << "draw " << i;
  }
}

TEST(RenderPipeline, SkipsShaderAndTextureRebindsBetweenSharedDraws) {
  auto s = makeScene();
  spawnMixed(*s);
  s->frame();

  // Four draws, but only two distinct shaders and two distinct textures, and
  // sorting makes each one contiguous.
  EXPECT_EQ(s->rec().shaderUses, (std::vector<int>{1, 2}));
  EXPECT_EQ(s->rec().unit0TextureBinds, (std::vector<int>{1, 2}));
}

TEST(RenderPipeline, DrawOrderIsStableAcrossFramesAndSpawnOrder) {
  auto s = makeScene();
  spawnMixed(*s);
  s->frame();
  s->frame();

  const auto &draws = s->rec().draws;
  ASSERT_EQ(draws.size(), 8u);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(draws[i].instanceCount, draws[i + 4].instanceCount);
    EXPECT_EQ(draws[i].indexCount, draws[i + 4].indexCount);
    EXPECT_EQ(draws[i].shaderId, draws[i + 4].shaderId);
  }
}

TEST(RenderPipeline, BatchesWithNoEntitiesLeftProduceNoDraw) {
  auto s = makeScene();
  std::vector<EntityID> bEntities;
  s->spawn("tri", "matA");
  for (int i = 0; i < 3; ++i) {
    bEntities.push_back(s->spawn("tri", "matB", float(i)));
  }
  s->frame();
  EXPECT_EQ(s->rec().draws.size(), 2u);

  for (EntityID e : bEntities) {
    s->world->destroyEntity(e);
  }
  s->rec().draws.clear();
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 1u)
      << "the matB batch no longer has entities and must not be drawn";
  EXPECT_EQ(s->rec().draws[0].shaderId, 1);
}

TEST(RenderPipeline, InvisibleEntitiesAreNotDrawn) {
  auto s = makeScene();
  EntityID hidden = s->spawn("tri", "matA", 0);
  s->spawn("tri", "matA", 1);
  s->world->getComponent<MeshComponent>(hidden).visible = false;
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 1u);
  EXPECT_EQ(s->rec().draws[0].instanceCount, 1u);
}

// --- Per-frame uniform buffer -----------------------------------------------

TEST(RenderPipeline, FrameDataIsUploadedAndBoundOncePerFrame) {
  auto s = makeScene();
  s->world->time = 1.5;
  spawnMixed(*s);
  s->frame();

  const auto &binds = s->rec().uniformBufferBinds;
  ASSERT_EQ(binds.size(), 1u) << "once per frame, not once per draw";
  EXPECT_EQ(binds[0].binding, kFrameDataBinding);
  ASSERT_EQ(binds[0].bytes.size(), sizeof(FrameData));

  FrameData fd;
  std::memcpy(&fd, binds[0].bytes.data(), sizeof(fd));
  for (int i = 0; i < 16; ++i) {
    EXPECT_EQ(fd.viewProj[i], float(i) + 0.5f) << i;
  }
  EXPECT_EQ(fd.cameraPos[0], 1.0f);
  EXPECT_EQ(fd.cameraPos[1], 2.0f);
  EXPECT_EQ(fd.cameraPos[2], 3.0f);
  EXPECT_EQ(fd.time, 1.5f);
  EXPECT_EQ(fd.lightDir[1], 1.0f);
  EXPECT_EQ(fd.lightColor[0], 0.5f);
  EXPECT_EQ(fd.lightColor[1], 0.25f);
  EXPECT_EQ(fd.lightColor[2], 0.125f);

  const resources::Fog fog;
  EXPECT_EQ(fd.fogColor[2], fog.color.z);
  EXPECT_EQ(fd.fogDensity, fog.density);
  EXPECT_EQ(fd.fogStart, fog.start);
  EXPECT_EQ(fd.fogEnd, fog.end);
}

TEST(RenderPipeline, ShaderWithFrameBlockIsNotSentLooseFrameUniforms) {
  // Shader "a" (id 1) declares the FrameData block; shader "b" (id 2) is a
  // legacy shader with loose uniforms.
  auto s = makeScene([](fake::Recorder &rec) {
    rec.shaderDeclaresFrameBlock[1] = true;
    rec.shaderDeclaresFrameBlock[2] = false;
  });
  spawnMixed(*s);
  s->frame();

  auto has = [&](int shader, const char *name) {
    const auto &v = s->rec().looseUniforms[shader];
    return std::find(v.begin(), v.end(), name) != v.end();
  };

  for (const char *name :
       {"viewProj", "lightDir", "lightColor", "cameraPos", "time", "fogColor",
        "fogStart", "fogEnd", "ambientColor"}) {
    EXPECT_FALSE(has(1, name)) << name << " must come from the block";
    EXPECT_TRUE(has(2, name)) << name << " must still reach a legacy shader";
  }
}

TEST(RenderPipeline, LegacyShaderGetsTheSameValuesAsLooseUniforms) {
  // Neither shader declares the block (the default): everything is loose.
  auto s = makeScene();
  spawnMixed(*s);
  s->frame();

  for (int shader : {1, 2}) {
    const auto &v = s->rec().looseUniforms[shader];
    EXPECT_NE(std::find(v.begin(), v.end(), "viewProj"), v.end()) << shader;
    EXPECT_NE(std::find(v.begin(), v.end(), "lightDir"), v.end()) << shader;
  }
}

// --- Render state -----------------------------------------------------------

TEST(RenderPipeline, FrameStartsFromBaselineStateBeforeClearing) {
  auto s = makeScene();
  s->spawn("tri", "matA");
  s->frame();

  EXPECT_EQ(s->rec().clears, 1);
  ASSERT_FALSE(s->rec().stateRequests.empty());
  EXPECT_EQ(s->rec().stateRequests.front(), api::RenderState{});
}

TEST(RenderPipeline, EachMaterialsStateIsInEffectForItsDraw) {
  auto s = makeScene();
  api::RenderState decal;
  decal.cull = api::CullMode::None;
  decal.blend = api::BlendMode::None;
  decal.depthWrite = false;
  s->assets()
      .getMaterial(s->assets().getMaterialID("matB"))
      .setRenderState(decal);

  s->spawn("tri", "matA", 0);
  s->spawn("tri", "matB", 1);
  s->frame();

  const auto &draws = s->rec().draws;
  ASSERT_EQ(draws.size(), 2u);
  // matA (shader a) sorts before matB (shader b).
  EXPECT_EQ(draws[0].state, api::RenderState{});
  EXPECT_EQ(draws[1].state, decal);
}

// --- Sky ---------------------------------------------------------------------

namespace {
bool hasUniform(Scene &s, int shader, const char *name) {
  const auto &v = s.rec().looseUniforms[shader];
  return std::find(v.begin(), v.end(), name) != v.end();
}
} // namespace

TEST(RenderPipeline, SkyIsAFullscreenTriangleDrawnBeforeTheSceneWithItsOwnState) {
  auto s = makeScene();
  // Note: no "cube" mesh exists in this scene. The sky needs no mesh.
  s->assets().loadShader("none.vert", "none.frag", nullptr, "skybox"); // id 3
  s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
  s->spawn("tri", "matA");
  s->frame();

  ASSERT_EQ(s->rec().fullscreenDraws.size(), 1u);
  const auto &sky = s->rec().fullscreenDraws[0];
  EXPECT_EQ(sky.shaderId, 3);
  EXPECT_EQ(sky.state.cull, api::CullMode::None);
  EXPECT_EQ(sky.state.depthFunc, api::DepthFunc::LessEqual);
  EXPECT_FALSE(sky.state.depthWrite);

  EXPECT_EQ(s->rec().drawSequence,
            (std::vector<std::string>{"fullscreen", "indexed"}))
      << "the sky is drawn first";
  ASSERT_EQ(s->rec().draws.size(), 1u);
  EXPECT_EQ(s->rec().draws[0].state, api::RenderState{})
      << "scene geometry must not inherit the sky's state";
}

TEST(RenderPipeline, NoSkyEntityMeansNoSkyDraw) {
  auto s = makeScene();
  s->spawn("tri", "matA");
  s->frame();
  EXPECT_TRUE(s->rec().fullscreenDraws.empty());
}

TEST(RenderPipeline, OnlyTheFirstSkyIsDrawn) {
  auto s = makeScene();
  s->assets().loadShader("none.vert", "none.frag", nullptr, "skybox");
  s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
  s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
  s->frame();
  EXPECT_EQ(s->rec().fullscreenDraws.size(), 1u);
}

TEST(RenderPipeline, SkyUsesTheShaderNamedByTheComponent) {
  auto s = makeScene();
  s->assets().loadShader("none.vert", "none.frag", nullptr, "skybox"); // 3
  s->assets().loadShader("none.vert", "none.frag", nullptr, "dusk");   // 4
  SkyboxComponent sky;
  sky.shaderName = Name("dusk");
  s->world->addComponents(s->world->createEntity(), SkyboxComponent(sky));
  s->frame();

  ASSERT_EQ(s->rec().fullscreenDraws.size(), 1u);
  EXPECT_EQ(s->rec().fullscreenDraws[0].shaderId, 4);
}

TEST(RenderPipeline, SkyGetsItsOwnUniformsAndLooseFrameValuesOnlyWithoutTheBlock) {
  // Sky shader is id 3. In the first scene it has no FrameData block.
  {
    auto s = makeScene();
    s->assets().loadShader("none.vert", "none.frag", nullptr, "skybox");
    s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
    s->frame();
    for (const char *n : {"invSkyViewProj", "skyColor", "horizonColor",
                          "lightDir", "lightColor", "fogColor"}) {
      EXPECT_TRUE(hasUniform(*s, 3, n)) << n;
    }
  }
  // In the second it does: the per-frame values come from the buffer.
  {
    auto s = makeScene([](fake::Recorder &rec) {
      rec.shaderDeclaresFrameBlock[3] = true;
    });
    s->assets().loadShader("none.vert", "none.frag", nullptr, "skybox");
    s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
    s->frame();
    for (const char *n : {"invSkyViewProj", "skyColor", "horizonColor"}) {
      EXPECT_TRUE(hasUniform(*s, 3, n)) << n;
    }
    for (const char *n : {"lightDir", "lightColor", "fogColor", "viewProj"}) {
      EXPECT_FALSE(hasUniform(*s, 3, n)) << n;
    }
  }
}

// --- Texture options ---------------------------------------------------------

TEST(TextureOptions, DefaultsMatchTheEnginesOriginalFixedBehaviour) {
  auto s = makeScene(); // t1, t2 loaded without a TextureDesc
  ASSERT_GE(s->rec().textureDescs.size(), 2u);
  for (int i = 0; i < 2; ++i) {
    const api::TextureDesc &d = s->rec().textureDescs[i];
    EXPECT_EQ(d.wrap, api::TextureWrap::ClampToEdge);
    EXPECT_EQ(d.minFilter, api::TextureFilter::Linear);
    EXPECT_EQ(d.magFilter, api::TextureFilter::Linear);
    EXPECT_TRUE(d.mipmaps);
    EXPECT_EQ(d.anisotropy, 1.0f);
    EXPECT_FALSE(d.srgb);
  }
}

TEST(TextureOptions, LoadTextureForwardsTheDescriptionToTheDevice) {
  auto s = makeScene();
  api::TextureDesc desc;
  desc.wrap = api::TextureWrap::Repeat;
  desc.minFilter = api::TextureFilter::Nearest;
  desc.magFilter = api::TextureFilter::Nearest;
  desc.mipmaps = false;
  desc.anisotropy = 8.0f;
  desc.srgb = true;

  const std::string white =
      utils::Utils::getAssetPath("assets/textures/white.png");
  s->assets().loadTexture(white.c_str(), false, "tiled", desc);

  const api::TextureDesc &got = s->rec().textureDescs.back();
  EXPECT_EQ(got.wrap, api::TextureWrap::Repeat);
  EXPECT_EQ(got.minFilter, api::TextureFilter::Nearest);
  EXPECT_EQ(got.magFilter, api::TextureFilter::Nearest);
  EXPECT_FALSE(got.mipmaps);
  EXPECT_EQ(got.anisotropy, 8.0f);
  EXPECT_TRUE(got.srgb);
}

TEST(TextureOptions, AlphaFlagIsHonouredWhenLoading) {
  auto s = makeScene();
  const std::string white =
      utils::Utils::getAssetPath("assets/textures/white.png");
  s->assets().loadTexture(white.c_str(), true, "with_alpha");
  s->assets().loadTexture(white.c_str(), false, "without_alpha");

  // The two before them were loaded with alpha = false.
  const size_t n = s->rec().textureHadAlpha.size();
  EXPECT_TRUE(s->rec().textureHadAlpha[n - 2]);
  EXPECT_FALSE(s->rec().textureHadAlpha[n - 1]);
}

TEST(TextureOptions, MissingFileBecomesAOneByOneFallbackInsteadOfGarbage) {
  auto s = makeScene();
  const size_t before = s->rec().textureSizes.size();
  s->assets().loadTexture("definitely/not/a/file.png", false, "missing");

  ASSERT_EQ(s->rec().textureSizes.size(), before + 1);
  EXPECT_EQ(s->rec().textureSizes.back(), (std::pair<uint32_t, uint32_t>(1, 1)));
  EXPECT_NO_THROW(s->assets().getTextureID("missing"));
}

// --- Instance data ends up where the shader expects it -----------------------

TEST(RenderPipeline, EachInstanceCarriesItsOwnModelMatrixAndColor) {
  auto s = makeScene();
  EntityID a = s->spawn("tri", "matA", 7.0f);
  EntityID b = s->spawn("tri", "matA", 9.0f);
  s->world->addComponent(b, ColorComponent{.color = Vec4(0.2f, 0.4f, 0.6f, 1.0f)});
  (void)a;
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 1u);
  const auto &draw = s->rec().draws[0];
  ASSERT_EQ(draw.instanceCount, 2u);
  ASSERT_GE(draw.instanceFloats.size(), 40u); // 2 instances x 20 floats

  // Instance layout: 16 floats of model matrix (column-major, so the
  // translation is floats 12..14), then 4 of colour.
  const float *first = draw.instanceFloats.data();
  const float *second = first + 20;
  EXPECT_EQ(first[12], 7.0f);
  EXPECT_EQ(second[12], 9.0f);
  EXPECT_EQ(first[0], 1.0f) << "unit scale on the diagonal";
  EXPECT_EQ(first[15], 1.0f);

  // Entities were spawned a then b and share an archetype only when they have
  // the same components; b has an extra ColorComponent, so which instance
  // comes first is not specified. Check the colour pairs with its own matrix.
  for (const float *inst : {first, second}) {
    if (inst[12] == 7.0f) {
      EXPECT_EQ(inst[16], 1.0f);
      EXPECT_EQ(inst[17], 1.0f);
    } else {
      EXPECT_EQ(inst[16], 0.2f);
      EXPECT_EQ(inst[17], 0.4f);
      EXPECT_EQ(inst[18], 0.6f);
    }
  }
}
