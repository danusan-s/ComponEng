#include "render_scene_fixture.hpp"

#include "componeng/core/profiling.hpp"
#include "componeng/resources/render_stats.hpp"
#include "gtest/gtest.h"

#include <algorithm>
#include <cstring>

using namespace rscene;

namespace {

// The bytes of the parameter block (set 1, binding 0) a draw saw, and where
// the shader says `member` lives in it.
const std::vector<unsigned char> &materialBytes(const fakerhi::Draw &d) {
  return d.bufferBytes.at(fakerhi::bindingKey(1, 0));
}

float floatAt(const std::vector<unsigned char> &bytes, uint32_t offset) {
  float v = 0.0f;
  std::memcpy(&v, bytes.data() + offset, sizeof(v));
  return v;
}

uint32_t memberOffset(Scene &s, const char *shader, const char *member) {
  const Shader &sh = s.assets().getShader(s.assets().getShaderID(shader));
  return sh.materialBlock()->member(member)->offset;
}

// Whether the fragment module is the shared empty one a depth-only draw uses.
bool isDepthOnly(Scene &s, const char *shader, int fragmentId) {
  const Shader &sh = s.assets().getShader(s.assets().getShaderID(shader));
  return fragmentId == fragmentModuleId(sh, Shader::Variant::DepthOnly);
}

} // namespace

// --- Decoupling from the GL backend -----------------------------------------

TEST(RenderPipeline, AssetsAreCreatedThroughTheDevice) {
  auto s = makeScene();
  // Two shaders, each a vertex and a fragment module.
  EXPECT_EQ(s->rec().modulesCreated, 4);
  // Two textures (the draw-time fallbacks come later).
  EXPECT_EQ(s->rec().imagesCreated, 2);
  EXPECT_EQ(s->rec().samplersCreated, 2);
  // Two meshes, a vertex and an index buffer each.
  EXPECT_EQ(s->rec().buffersCreated, 4);
  EXPECT_TRUE(s->rec().errors.empty());
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

TEST(RenderPipeline, SkipsPipelineAndMaterialRebindsBetweenSharedDraws) {
  auto s = makeScene();
  spawnMixed(*s);
  s->frame();

  // Four draws, three materials, two distinct shaders: matA and matC share
  // shader a's pipeline, and sorting makes each pipeline and material
  // contiguous, so none is bound twice.
  EXPECT_EQ(s->rec().pipelineBinds.size(), 2u);
  // Set 0 once for the rendering, then one material group per material.
  std::vector<int> materialGroups;
  for (const auto &c : s->rec().commands) {
    if (c.kind == fakerhi::Command::BindGroup && c.id != 1) {
      materialGroups.push_back(c.id);
    }
  }
  EXPECT_EQ(materialGroups.size(), 3u);
  EXPECT_TRUE(s->rec().errors.empty());
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

  // The frame group (the first group made) is bound once for the one
  // rendering, not once per draw.
  int frameBinds = 0;
  for (int id : s->rec().groupBinds) {
    frameBinds += id == 1;
  }
  EXPECT_EQ(frameBinds, 1);

  ASSERT_FALSE(s->rec().draws.empty());
  const auto &bytes = s->rec().draws[0].bufferBytes.at(
      fakerhi::bindingKey(0, kFrameDataBinding));
  ASSERT_EQ(bytes.size(), sizeof(FrameData));

  FrameData fd;
  std::memcpy(&fd, bytes.data(), sizeof(fd));
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
TEST(RenderPipeline, ShadersWithLooseUniformsAreInvalidAndDrawNothing) {
  // Per-frame values only go out through the FrameData block: a shader that
  // reads them as plain uniforms does not compile, instead of reading zeros.
  auto s = makeScene();
  s->assets().compileShader(
      "loose",
      "#version 460\nlayout(location = 0) in vec3 aPos;\n"
      "layout(location = 0) uniform mat4 viewProj;\n"
      "void main() { gl_Position = viewProj * vec4(aPos, 1.0); }\n",
      testFragmentSource(false));
  const Shader &loose = s->assets().getShader(s->assets().getShaderID("loose"));
  EXPECT_FALSE(loose.valid());

  s->assets().registerMaterial<DiffuseMaterial>("matLoose", "loose", "t1");
  s->spawn("tri", "matLoose");
  s->frame();
  EXPECT_TRUE(s->rec().draws.empty());
  EXPECT_TRUE(s->rec().errors.empty());
}
// --- Render state -----------------------------------------------------------

namespace {
// What an Opaque material draws with when there is no depth pre-pass.
api::RenderState opaqueState() {
  api::RenderState st;
  st.blend = api::BlendMode::None;
  return st;
}

void makeTransparent(Scene &s, const char *material,
                     const api::RenderState &state) {
  auto &m = s.assets().getMaterial(s.assets().getMaterialID(material));
  m.setType(MaterialType::Transparent);
  m.setRenderState(state);
}
} // namespace

TEST(RenderPipeline, FrameClearsTheTargetItDrawsInBeforeDrawing) {
  auto s = makeScene();
  s->spawn("tri", "matA");
  s->frame();

  EXPECT_EQ(s->rec().clears, 1);
  ASSERT_EQ(s->rec().commands.size() > 2, true);
  EXPECT_EQ(s->rec().commands[1].kind, fakerhi::Command::BeginRendering)
      << "the first thing in a frame is its clearing rendering";
  EXPECT_EQ(s->rec().clearTargetIds, (std::vector<int>{0}));
  EXPECT_TRUE(s->rec().errors.empty());
}
TEST(RenderPipeline, EachMaterialsStateIsInEffectForItsDraw) {
  auto s = makeScene();
  api::RenderState decal;
  decal.cull = api::CullMode::None;
  decal.blend = api::BlendMode::Additive;
  decal.depthWrite = false;
  auto &matB = s->assets().getMaterial(s->assets().getMaterialID("matB"));
  matB.setType(MaterialType::Transparent);
  matB.setRenderState(decal);

  s->spawn("tri", "matA", 0);
  s->spawn("tri", "matB", 1);
  s->frame();

  const auto &draws = s->rec().draws;
  ASSERT_EQ(draws.size(), 2u);
  // matA is opaque, so it is drawn first; matB is transparent and gets its
  // state exactly as declared.
  EXPECT_EQ(draws[0].state, opaqueState());
  EXPECT_EQ(draws[1].state, decal);
}

// --- Material type -----------------------------------------------------------

TEST(MaterialType, DefaultsToOpaqueAndOnlyTransparentIsTransparent) {
  auto s = makeScene();
  auto &m = s->assets().getMaterial(s->assets().getMaterialID("matA"));
  EXPECT_EQ(m.getType(), MaterialType::Opaque);
  EXPECT_FALSE(m.isTransparent());

  // A blended, depth-write-off state no longer makes a material transparent
  // by itself: the type says which pass it belongs to.
  api::RenderState blended;
  blended.depthWrite = false;
  m.setRenderState(blended);
  EXPECT_FALSE(m.isTransparent());

  m.setType(MaterialType::Transparent);
  EXPECT_TRUE(m.isTransparent());
}

TEST(MaterialType, OpaqueMaterialKeepsItsCullModeButNotItsBlending) {
  auto s = makeScene();
  api::RenderState st;
  st.cull = api::CullMode::None;
  st.blend = api::BlendMode::Alpha;
  st.depthWrite = false;
  s->assets().getMaterial(s->assets().getMaterialID("matA")).setRenderState(st);
  s->spawn("tri", "matA");
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 1u);
  api::RenderState expected = opaqueState();
  expected.cull = api::CullMode::None;
  EXPECT_EQ(s->rec().draws[0].state, expected);
}

// --- Depth pre-pass ----------------------------------------------------------

namespace {
void enablePrepass(Scene &s) {
  s.world->getResource<resources::RenderSettings>().depthPrepass = true;
}
} // namespace

TEST(DepthPrepass, OpaqueBatchesAreDrawnDepthOnlyThenShadedWhereDepthIsEqual) {
  auto s = makeScene();
  enablePrepass(*s);
  s->spawn("tri", "matA", 0);
  s->spawn("tri", "matA", 1);
  s->spawn("tri", "matB", 2);
  s->frame();

  const auto &draws = s->rec().draws;
  ASSERT_EQ(draws.size(), 4u) << "2 batches, each drawn in both passes";

  for (int i = 0; i < 2; ++i) {
    const api::RenderState &st = draws[i].state;
    EXPECT_FALSE(st.colorWrite) << "pre-pass draw " << i;
    EXPECT_TRUE(st.depthWrite) << "pre-pass draw " << i;
    EXPECT_TRUE(st.depthTest);
    EXPECT_EQ(st.depthFunc, api::DepthFunc::Less);
    EXPECT_EQ(st.blend, api::BlendMode::None);
  }
  for (int i = 2; i < 4; ++i) {
    const api::RenderState &st = draws[i].state;
    EXPECT_TRUE(st.colorWrite) << "colour draw " << i;
    EXPECT_FALSE(st.depthWrite) << "colour draw " << i;
    EXPECT_EQ(st.depthFunc, api::DepthFunc::Equal);
    EXPECT_EQ(st.blend, api::BlendMode::None);
  }

  // Same geometry, same instances, in the same order, in both passes.
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(draws[i].indexCount, draws[i + 2].indexCount);
    EXPECT_EQ(draws[i].instanceCount, draws[i + 2].instanceCount);
    EXPECT_EQ(draws[i].instanceFloats, draws[i + 2].instanceFloats);
  }
}

TEST(DepthPrepass, UsesTheShadersDepthVariantNotTheShaderItself) {
  auto s = makeScene();
  enablePrepass(*s);
  s->spawn("tri", "matA", 0); // shader 1
  s->spawn("tri", "matB", 1); // shader 2
  s->frame();

  const auto &draws = s->rec().draws;
  ASSERT_EQ(draws.size(), 4u);
  // The vertex stage is the shader's own, so the depth matches the colour
  // pass bit for bit; only the fragment stage is the empty one.
  EXPECT_EQ(draws[0].shaderId, 1);
  EXPECT_EQ(draws[1].shaderId, 2);
  EXPECT_EQ(draws[2].shaderId, 1);
  EXPECT_EQ(draws[3].shaderId, 2);
  EXPECT_EQ(draws[0].vertexModuleId, draws[2].vertexModuleId);
  EXPECT_EQ(draws[1].vertexModuleId, draws[3].vertexModuleId);
  EXPECT_TRUE(isDepthOnly(*s, "a", draws[0].fragmentModuleId));
  EXPECT_TRUE(isDepthOnly(*s, "b", draws[1].fragmentModuleId));
  EXPECT_NE(draws[0].fragmentModuleId, draws[1].fragmentModuleId);
  EXPECT_FALSE(isDepthOnly(*s, "a", draws[2].fragmentModuleId));
  EXPECT_FALSE(isDepthOnly(*s, "b", draws[3].fragmentModuleId));
}
TEST(DepthPrepass, DepthVariantsAreBuiltOnceNotEveryFrame) {
  auto s = makeScene();
  enablePrepass(*s);
  s->spawn("tri", "matA", 0);
  s->spawn("tri", "matB", 1);
  s->frame();
  // The two shaders' four modules, plus one empty fragment module each.
  EXPECT_EQ(s->rec().modulesCreated, 6);
  const int pipelines = s->rec().pipelinesCreated;
  EXPECT_EQ(pipelines, 4) << "depth and colour pipeline for each shader";
  s->frame();
  s->frame();
  EXPECT_EQ(s->rec().modulesCreated, 6);
  EXPECT_EQ(s->rec().pipelinesCreated, pipelines);
}
TEST(DepthPrepass, TransparentBatchesAreNotInThePrepass) {
  auto s = makeScene();
  enablePrepass(*s);
  makeTransparent(*s, "matB", api::RenderState{});
  s->spawn("tri", "matA", 0);
  s->spawn("tri", "matB", 1);
  s->frame();

  const auto &draws = s->rec().draws;
  ASSERT_EQ(draws.size(), 3u) << "matA twice, matB once";
  EXPECT_FALSE(draws[0].state.colorWrite);
  EXPECT_EQ(draws[1].state.depthFunc, api::DepthFunc::Equal);
  EXPECT_EQ(draws[2].state, api::RenderState{})
      << "transparent keeps its own state, depth-writing and blended";
}

TEST(DepthPrepass, SkyIsDrawnAfterTheColourPassNotBetweenThePasses) {
  auto s = makeScene();
  enablePrepass(*s);
  s->loadSky();
  s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
  s->spawn("tri", "matA");
  s->frame();

  EXPECT_EQ(s->rec().drawSequence,
            (std::vector<std::string>{"indexed", "indexed", "fullscreen"}));
}

TEST(DepthPrepass, NothingOpaqueMeansNoPrepass) {
  auto s = makeScene();
  enablePrepass(*s);
  makeTransparent(*s, "matA", api::RenderState{});
  s->spawn("tri", "matA");
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 1u);
  EXPECT_EQ(s->rec().draws[0].state, api::RenderState{});
  EXPECT_EQ(s->rec().modulesCreated, 4) << "no depth variant needed";
}

TEST(DepthPrepass, DisabledByTheSettingOpaqueDrawsDepthWritingWithLess) {
  auto s = makeScene(); // fixture default: off
  s->spawn("tri", "matA");
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 1u);
  EXPECT_EQ(s->rec().draws[0].state, opaqueState());
  EXPECT_EQ(s->rec().modulesCreated, 4);
}

TEST(DepthPrepass, DepthPassBindsTheSameFrameAndMaterialSetsAsTheColourPass) {
  // The depth-only program is a subset of the colour program, so one frame
  // group and one material group serve both.
  auto s = makeScene();
  enablePrepass(*s);
  s->spawn("tri", "matA");
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 2u);
  EXPECT_EQ(s->rec().draws[0].groupIds, s->rec().draws[1].groupIds);
  EXPECT_TRUE(s->rec().errors.empty());
}
// --- Alpha-tested (Cutout) materials
// ------------------------------------------

namespace {
void makeCutout(Scene &s, const char *material) {
  s.assets()
      .getMaterial(s.assets().getMaterialID(material))
      .setType(MaterialType::Cutout);
}

} // namespace

TEST(MaterialType, CutoutIsAlphaTestedButNotTransparent) {
  auto s = makeScene();
  auto &m = s->assets().getMaterial(s->assets().getMaterialID("matA"));
  EXPECT_FALSE(m.isAlphaTested());

  m.setType(MaterialType::Cutout);
  EXPECT_TRUE(m.isAlphaTested());
  EXPECT_FALSE(m.isTransparent()) << "drawn with the opaque batches";

  m.setType(MaterialType::Transparent);
  EXPECT_FALSE(m.isAlphaTested());
}

TEST(Cutout, DrawsInTheOpaqueStateAndBeforeTransparentBatches) {
  auto s = makeScene(); // pre-pass off
  makeCutout(*s, "matB");
  makeTransparent(*s, "matA", api::RenderState{});
  s->spawn("tri", "matA", 0);
  s->spawn("tri", "matB", 1);
  s->frame();

  const auto &draws = s->rec().draws;
  ASSERT_EQ(draws.size(), 2u);
  EXPECT_EQ(draws[0].shaderId, 2) << "the cutout (matB) goes first";
  EXPECT_EQ(draws[0].state, opaqueState()) << "depth writing, no blending";
  EXPECT_EQ(draws[1].state, api::RenderState{});
}

TEST(Cutout, WithoutAPrepassItIsAnOrdinaryDrawOfItsOwnShader) {
  auto s = makeScene();
  makeCutout(*s, "matA");
  s->spawn("tri", "matA");
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 1u);
  EXPECT_EQ(s->rec().draws[0].shaderId, 1);
  EXPECT_EQ(s->rec().draws[0].state, opaqueState());
  EXPECT_EQ(s->rec().modulesCreated, 4) << "no depth variant needed";
}

TEST(Cutout, PrepassCutsItsHolesAndTheColourPassThenTestsEqual) {
  auto s = makeScene();
  enablePrepass(*s);
  makeCutout(*s, "matA");
  s->spawn("tri", "matA");
  s->frame();

  const auto &draws = s->rec().draws;
  ASSERT_EQ(draws.size(), 2u);
  EXPECT_FALSE(draws[0].state.colorWrite);
  EXPECT_TRUE(draws[0].state.depthWrite);
  EXPECT_EQ(draws[0].state.depthFunc, api::DepthFunc::Less);
  EXPECT_TRUE(draws[1].state.colorWrite);
  EXPECT_FALSE(draws[1].state.depthWrite);
  EXPECT_EQ(draws[1].state.depthFunc, api::DepthFunc::Equal);
}

TEST(Cutout, PrepassUsesTheAlphaTestedVariantAndOpaqueKeepsTheEmptyOne) {
  auto s = makeScene();
  enablePrepass(*s);
  makeCutout(*s, "matA"); // shader 1
  s->spawn("tri", "matA", 0);
  s->spawn("tri", "matB", 1); // opaque, shader 2
  s->frame();

  const auto &draws = s->rec().draws;
  ASSERT_EQ(draws.size(), 4u);
  const Shader &a = s->assets().getShader(s->assets().getShaderID("a"));
  const int alphaTested =
      fragmentModuleId(a, Shader::Variant::AlphaTestedDepth);
  const int colour = fragmentModuleId(a, Shader::Variant::Color);

  EXPECT_EQ(draws[0].fragmentModuleId, alphaTested)
      << "the cutout's pre-pass runs the shader's own fragment stage, compiled "
         "with the macro";
  EXPECT_NE(alphaTested, colour);
  EXPECT_FALSE(isDepthOnly(*s, "a", draws[0].fragmentModuleId));
  EXPECT_TRUE(isDepthOnly(*s, "b", draws[1].fragmentModuleId))
      << "the opaque material keeps the empty fragment stage";

  // The macro changes the code: the variant is not the colour shader's SPIR-V.
  const auto &sh = s->rec();
  (void)sh;
  EXPECT_EQ(draws[2].fragmentModuleId, colour)
      << "the colour shader is untouched";
  EXPECT_EQ(draws[2].shaderId, 1);
  EXPECT_EQ(draws[3].shaderId, 2);
}
TEST(Cutout, OpaqueAndCutoutMaterialsOfOneShaderGetDifferentPrepassPrograms) {
  auto s = makeScene();
  enablePrepass(*s);
  makeCutout(*s, "matC");     // shader a + t2
  s->spawn("tri", "matA", 0); // shader a + t1, opaque
  s->spawn("tri", "matC", 1);
  s->frame();

  const auto &draws = s->rec().draws;
  ASSERT_EQ(draws.size(), 4u);
  // Same shader, but two programs: tracking the bound shader instead of its
  // program would draw the second one with the first's.
  EXPECT_EQ(draws[0].shaderId, draws[1].shaderId);
  EXPECT_NE(draws[0].fragmentModuleId, draws[1].fragmentModuleId);
  EXPECT_EQ(draws[2].shaderId, 1);
  EXPECT_EQ(draws[3].shaderId, 1);
  EXPECT_EQ(s->rec().pipelineBinds.size(), 3u)
      << "two pre-pass programs and the colour program, bound once";
}
TEST(Cutout, PrepassBindsTheAlbedoThatTheAlphaTestReads) {
  auto s = makeScene();
  enablePrepass(*s);
  makeCutout(*s, "matC"); // texture t2
  s->spawn("tri", "matC");
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 2u);
  EXPECT_EQ(s->rec().draws[0].albedoTextureId, 2) << "pre-pass";
  EXPECT_EQ(s->rec().draws[1].albedoTextureId, 2) << "colour pass";
}
TEST(Cutout, OpaquePrepassUsesTheEmptyFragmentStageSoNothingSamplesTheAlbedo) {
  auto s = makeScene();
  enablePrepass(*s);
  s->spawn("tri", "matA"); // texture t1
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 2u);
  EXPECT_TRUE(isDepthOnly(*s, "a", s->rec().draws[0].fragmentModuleId));
  EXPECT_FALSE(isDepthOnly(*s, "a", s->rec().draws[1].fragmentModuleId));
}
TEST(Cutout, DepthVariantsAreBuiltOnceNotEveryFrame) {
  auto s = makeScene();
  enablePrepass(*s);
  makeCutout(*s, "matA");
  s->spawn("tri", "matA", 0);
  s->frame();
  const int created = s->rec().modulesCreated;
  EXPECT_EQ(created, 5) << "four modules and the alpha-tested fragment";
  s->frame();
  s->frame();
  EXPECT_EQ(s->rec().modulesCreated, created);
}
TEST(Cutout, TransparentBatchesStillSkipThePrepass) {
  auto s = makeScene();
  enablePrepass(*s);
  makeCutout(*s, "matA");
  makeTransparent(*s, "matB", api::RenderState{});
  s->spawn("tri", "matA", 0);
  s->spawn("tri", "matB", 1);
  s->frame();

  EXPECT_EQ(s->rec().draws.size(), 3u) << "matA twice, matB once";
}

TEST(ShaderVariants, VariantsAreBuiltOnceAndShareTheVertexModule) {
  auto s = makeScene();
  const Shader &shader = s->assets().getShader(s->assets().getShaderID("a"));
  const Shader::Program colour = shader.program(Shader::Variant::Color);
  const Shader::Program depth = shader.program(Shader::Variant::DepthOnly);
  const Shader::Program alpha =
      shader.program(Shader::Variant::AlphaTestedDepth);

  EXPECT_EQ(colour.vertex, depth.vertex);
  EXPECT_EQ(colour.vertex, alpha.vertex);
  EXPECT_NE(colour.fragment, depth.fragment);
  EXPECT_NE(colour.fragment, alpha.fragment);
  EXPECT_NE(depth.fragment, alpha.fragment);

  const int created = s->rec().modulesCreated;
  EXPECT_EQ(shader.program(Shader::Variant::DepthOnly).fragment,
            depth.fragment);
  EXPECT_EQ(shader.program(Shader::Variant::AlphaTestedDepth).fragment,
            alpha.fragment);
  EXPECT_EQ(s->rec().modulesCreated, created) << "built once";
}

TEST(ShaderVariants, AlphaTestedVariantIsTheOwnFragmentSourceWithTheMacro) {
  auto s = makeScene();
  const Shader &shader = s->assets().getShader(s->assets().getShaderID("a"));
  const auto words = [&](Shader::Variant v) {
    return static_cast<fakerhi::FakeShaderModule *>(shader.program(v).fragment)
        ->words();
  };
  EXPECT_NE(words(Shader::Variant::AlphaTestedDepth),
            words(Shader::Variant::Color))
      << "the macro makes the fragment stage return after its discard";
}

TEST(ShaderVariants, RecompilingDropsTheCachedVariants) {
  auto s = makeScene();
  Shader shader(s->world->getRenderDevice());
  const std::string vs = testVertexSource({});
  const std::vector<std::string> roots = {
      utils::Utils::getAssetPath("assets/shaders")};
  ASSERT_TRUE(shader.compile(vs, testFragmentSource(false), "t", roots));
  const int before = s->rec().modulesCreated;
  shader.program(Shader::Variant::AlphaTestedDepth);
  EXPECT_EQ(s->rec().modulesCreated, before + 1);

  ASSERT_TRUE(shader.compile(vs, testFragmentSource(false), "t", roots));
  shader.program(Shader::Variant::AlphaTestedDepth);
  EXPECT_EQ(s->rec().modulesCreated, before + 4)
      << "two new stages, then the variant rebuilt from the new source";
}

// --- Sky ---------------------------------------------------------------------

TEST(RenderPipeline,
     SkyIsAFullscreenTriangleWithItsOwnStateThatGeometryDoesNotInherit) {
  auto s = makeScene();
  // Note: no "cube" mesh exists in this scene. The sky needs no mesh.
  s->loadSky(); // shader 3
  s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
  s->spawn("tri", "matA");
  s->frame();

  ASSERT_EQ(s->rec().fullscreenDraws.size(), 1u);
  const auto &sky = s->rec().fullscreenDraws[0];
  EXPECT_EQ(sky.shaderId, 3);
  EXPECT_EQ(sky.state.cull, api::CullMode::None);
  EXPECT_EQ(sky.state.depthFunc, api::DepthFunc::LessEqual);
  EXPECT_FALSE(sky.state.depthWrite);

  ASSERT_EQ(s->rec().draws.size(), 1u);
  EXPECT_EQ(s->rec().draws[0].state, opaqueState())
      << "scene geometry must not inherit the sky's state";
}

TEST(RenderPipeline, SkyIsDrawnAfterOpaqueGeometryAndBeforeBlendedGeometry) {
  auto s = makeScene();
  s->loadSky();
  s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
  // matA is blended, matB is opaque.
  makeTransparent(*s, "matA", api::RenderState{});
  s->spawn("tri", "matA", 0); // blended, shader a: sorts first
  s->spawn("tri", "matB", 1); // opaque, shader b
  s->frame();

  EXPECT_EQ(s->rec().drawSequence,
            (std::vector<std::string>{"indexed", "fullscreen", "indexed"}))
      << "opaque, then the sky, then the blended draw, whatever the shader "
         "order";
  ASSERT_EQ(s->rec().draws.size(), 2u);
  EXPECT_EQ(s->rec().draws[0].state, opaqueState());
  EXPECT_EQ(s->rec().draws[1].state, api::RenderState{});
}

TEST(RenderPipeline, GeometryWithoutADepthTestIsDrawnOverTheSky) {
  auto s = makeScene();
  s->loadSky();
  s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
  api::RenderState overlay = opaqueState();
  overlay.depthTest = false; // e.g. a lens flare
  makeTransparent(*s, "matA", overlay);
  s->spawn("tri", "matA");
  s->frame();

  EXPECT_EQ(s->rec().drawSequence,
            (std::vector<std::string>{"fullscreen", "indexed"}));
}

TEST(RenderPipeline, GeometryStillDrawsAfterTheSkyWithItsOwnShaderBound) {
  // The sky binds its own shader; the next batch must bind its own again
  // even if it is the shader the batch loop used last.
  auto s = makeScene();
  s->loadSky();
  s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
  makeTransparent(*s, "matC", api::RenderState{});
  s->spawn("tri", "matA", 0);
  s->spawn("tri", "matC", 1); // matC: shader a, blended: drawn after the sky
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 2u);
  EXPECT_EQ(s->rec().draws[0].shaderId, s->rec().draws[1].shaderId);
  EXPECT_EQ(s->rec().draws[1].shaderId, 1);
}

TEST(RenderPipeline, NoSkyEntityMeansNoSkyDraw) {
  auto s = makeScene();
  s->spawn("tri", "matA");
  s->frame();
  EXPECT_TRUE(s->rec().fullscreenDraws.empty());
}

TEST(RenderPipeline, OnlyTheFirstSkyIsDrawn) {
  auto s = makeScene();
  s->loadSky();
  s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
  s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
  s->frame();
  EXPECT_EQ(s->rec().fullscreenDraws.size(), 1u);
}

TEST(RenderPipeline, SkyUsesTheShaderNamedByTheComponent) {
  auto s = makeScene();
  s->loadSky();       // 3
  s->loadSky("dusk"); // 4
  SkyboxComponent sky;
  sky.shaderName = Name("dusk");
  s->world->addComponents(s->world->createEntity(), SkyboxComponent(sky));
  s->frame();

  ASSERT_EQ(s->rec().fullscreenDraws.size(), 1u);
  EXPECT_EQ(s->rec().fullscreenDraws[0].shaderId, 4);
}

TEST(RenderPipeline, SkyGetsItsOwnParametersInItsMaterialBlock) {
  // Sky shader is shader 3.
  auto s = makeScene();
  s->loadSky();
  SkyboxComponent sky;
  sky.color = Vec3(0.1f, 0.2f, 0.3f);
  s->world->addComponents(s->world->createEntity(), SkyboxComponent(sky));
  s->frame();

  ASSERT_EQ(s->rec().fullscreenDraws.size(), 1u);
  const auto &bytes = materialBytes(s->rec().fullscreenDraws[0]);
  const uint32_t at = memberOffset(*s, "skybox", "skyColor");
  EXPECT_EQ(floatAt(bytes, at), 0.1f);
  EXPECT_EQ(floatAt(bytes, at + 4), 0.2f);
  EXPECT_EQ(floatAt(bytes, at + 8), 0.3f);
  // The camera has no rotation in the fixture, so the matrix is not the zero
  // the block starts as.
  const uint32_t inv = memberOffset(*s, "skybox", "invSkyViewProj");
  bool anyNonZero = false;
  for (int i = 0; i < 16; ++i) {
    anyNonZero |= floatAt(bytes, inv + 4 * i) != 0.0f;
  }
  EXPECT_TRUE(anyNonZero);
  EXPECT_TRUE(s->rec().errors.empty());
}
// --- Texture options ---------------------------------------------------------

TEST(TextureOptions, DefaultsMatchTheEnginesOriginalFixedBehaviour) {
  auto s = makeScene(); // t1, t2 loaded without a TextureDesc
  ASSERT_GE(s->rec().createdSamplers.size(), 2u);
  for (int i = 0; i < 2; ++i) {
    const rhi::SamplerDesc &d = s->rec().createdSamplers[i];
    EXPECT_EQ(d.wrapU, rhi::WrapMode::ClampToEdge);
    EXPECT_EQ(d.wrapV, rhi::WrapMode::ClampToEdge);
    EXPECT_EQ(d.minFilter, rhi::Filter::Linear);
    EXPECT_EQ(d.magFilter, rhi::Filter::Linear);
    EXPECT_EQ(d.mipFilter, rhi::Filter::Linear);
    EXPECT_EQ(d.anisotropy, 1.0f);
    const rhi::ImageDesc &img = s->rec().createdImages[i].desc;
    EXPECT_EQ(img.mipLevels, 0u) << "a full mip chain";
    EXPECT_EQ(img.format, rhi::Format::RGBA8) << "not sRGB";
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

  const rhi::SamplerDesc &got = s->rec().createdSamplers.back();
  EXPECT_EQ(got.wrapU, rhi::WrapMode::Repeat);
  EXPECT_EQ(got.wrapV, rhi::WrapMode::Repeat);
  EXPECT_EQ(got.minFilter, rhi::Filter::Nearest);
  EXPECT_EQ(got.magFilter, rhi::Filter::Nearest);
  EXPECT_EQ(got.anisotropy, 8.0f);
  const rhi::ImageDesc &img = s->rec().createdImages.back().desc;
  EXPECT_EQ(img.mipLevels, 1u);
  EXPECT_EQ(img.format, rhi::Format::RGBA8Srgb);
}

TEST(TextureOptions, ThreeChannelDataIsExpandedToOpaqueRgba) {
  auto s = makeScene();
  const std::string white =
      utils::Utils::getAssetPath("assets/textures/white.png");
  s->assets().loadTexture(white.c_str(), true, "with_alpha");
  s->assets().loadTexture(white.c_str(), false, "without_alpha");

  // Both reach the device as RGBA8; the one loaded without alpha has it set
  // opaque on the CPU.
  const auto &uploads = s->rec().imageUploads;
  ASSERT_GE(uploads.size(), 2u);
  const auto &noAlpha = uploads.back();
  ASSERT_GE(noAlpha.pixels.size(), 4u);
  EXPECT_EQ(noAlpha.pixels.size(),
            size_t(noAlpha.region.width) * noAlpha.region.height * 4);
  for (size_t i = 3; i < noAlpha.pixels.size(); i += 4) {
    ASSERT_EQ(noAlpha.pixels[i], 255) << "pixel " << i / 4;
  }
}

TEST(TextureOptions, MissingFileBecomesAOneByOneFallbackInsteadOfGarbage) {
  auto s = makeScene();
  const size_t before = s->rec().createdImages.size();
  s->assets().loadTexture("definitely/not/a/file.png", false, "missing");

  ASSERT_EQ(s->rec().createdImages.size(), before + 1);
  EXPECT_EQ(s->rec().createdImages.back().desc.extent, (rhi::Extent2D{1, 1}));
  EXPECT_NO_THROW(s->assets().getTextureID("missing"));
}

// --- Instance data ends up where the shader expects it -----------------------

TEST(RenderPipeline, EachInstanceCarriesItsOwnModelMatrixAndColor) {
  auto s = makeScene();
  EntityID a = s->spawn("tri", "matA", 7.0f);
  EntityID b = s->spawn("tri", "matA", 9.0f);
  s->world->addComponent(b,
                         ColorComponent{.color = Vec4(0.2f, 0.4f, 0.6f, 1.0f)});
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

// --- Render stats and pass timers --------------------------------------------

namespace {
const resources::RenderStats &statsOf(Scene &s) {
  return s.world->getResource<resources::RenderStats>();
}
} // namespace

TEST(RenderStats, CountDrawsInstancesTrianglesAndShaderBindsPerPass) {
  auto s = makeScene();
  enablePrepass(*s);
  makeTransparent(*s, "matB", api::RenderState{});
  s->spawn("tri", "matA", 0);
  s->spawn("tri", "matA", 1);
  s->spawn("quad", "matA", 2); // 6 indices: 2 triangles
  s->spawn("tri", "matB", 3);
  s->frame();

  const auto &st = statsOf(*s);
  for (auto pass : {api::PassId::DepthPrepass, api::PassId::Opaque}) {
    EXPECT_EQ(st.of(pass).drawCalls, 2) << api::passName(pass);
    EXPECT_EQ(st.of(pass).instances, 3u) << api::passName(pass);
    EXPECT_EQ(st.of(pass).triangles, 4u) << "2 x tri + 1 x quad";
    EXPECT_EQ(st.of(pass).shaderBinds, 1) << "both batches use shader a";
  }
  const auto &blended = st.of(api::PassId::Transparent);
  EXPECT_EQ(blended.drawCalls, 1);
  EXPECT_EQ(blended.instances, 1u);
  EXPECT_EQ(blended.triangles, 1u);
  EXPECT_EQ(blended.shaderBinds, 1);

  const auto &sky = st.of(api::PassId::Sky);
  EXPECT_EQ(sky.drawCalls, 0);

  const auto total = st.total();
  EXPECT_EQ(total.drawCalls, 5);
  EXPECT_EQ(total.instances, 7u);
}

TEST(RenderStats, SkyIsOneFullscreenTriangle) {
  auto s = makeScene();
  s->loadSky();
  s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
  s->spawn("tri", "matA");
  s->frame();

  const auto &sky = statsOf(*s).of(api::PassId::Sky);
  EXPECT_EQ(sky.drawCalls, 1);
  EXPECT_EQ(sky.instances, 1u);
  EXPECT_EQ(sky.triangles, 1u);
  EXPECT_EQ(sky.shaderBinds, 1);
}

TEST(RenderStats, ResetEveryFrameRatherThanAccumulating) {
  auto s = makeScene();
  s->spawn("tri", "matA");
  s->frame();
  s->frame();
  s->frame();

  EXPECT_EQ(statsOf(*s).of(api::PassId::Opaque).drawCalls, 1);
  EXPECT_EQ(statsOf(*s).of(api::PassId::Opaque).instances, 1u);
}

TEST(RenderStats, PrepassCountersStayZeroWhenThePrepassIsOff) {
  auto s = makeScene(); // fixture default: off
  s->spawn("tri", "matA");
  s->frame();

  EXPECT_EQ(statsOf(*s).of(api::PassId::DepthPrepass).drawCalls, 0);
  EXPECT_EQ(statsOf(*s).of(api::PassId::Opaque).drawCalls, 1);
}

TEST(RenderStats, UploadBytesCoverEveryBatchsInstanceData) {
  auto s = makeScene();
  s->spawn("tri", "matA", 0);
  s->spawn("tri", "matA", 1);
  s->frame();

  // Two instances of the diffuse layout: 16 floats of matrix + 4 of colour.
  EXPECT_EQ(statsOf(*s).uploadBytes, 2u * 20u * sizeof(float));
}

TEST(GpuPassTimers, EachPassIsBracketedAroundItsOwnDraws) {
  if constexpr (!core::Profiling::kEnabled) {
    GTEST_SKIP() << "pass timers are compiled out without profiling";
  }
  auto s = makeScene();
  enablePrepass(*s);
  s->loadSky();
  s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
  makeTransparent(*s, "matB", api::RenderState{});
  s->spawn("tri", "matA", 0);
  s->spawn("tri", "matB", 1);
  s->frame();

  EXPECT_EQ(s->rec().gpuPassEvents,
            (std::vector<std::string>{
                "begin Depth pre-pass", "indexed", "end Depth pre-pass",
                "begin Opaque", "indexed", "end Opaque",           //
                "begin Sky", "fullscreen", "end Sky",              //
                "begin Transparent", "indexed", "end Transparent", //
                "begin Post-process", "end Post-process"}));
}

TEST(GpuPassTimers, EmptyPassesAreStillBracketed) {
  if constexpr (!core::Profiling::kEnabled) {
    GTEST_SKIP() << "pass timers are compiled out without profiling";
  }
  auto s = makeScene();
  s->frame();

  // So a pass that did nothing reads ~0 instead of keeping an old value.
  EXPECT_EQ(s->rec().gpuPassEvents,
            (std::vector<std::string>{
                "begin Depth pre-pass", "end Depth pre-pass", "begin Opaque",
                "end Opaque", "begin Sky", "end Sky", "begin Transparent",
                "end Transparent", "begin Post-process", "end Post-process"}));
}

// LOD cross-fade

namespace {

constexpr size_t kFadeFloat = 20; // index of instanceFade within an instance

std::unique_ptr<Scene> makeFadeScene() {
  return makeScene([](SceneOptions &o) { o.fade = true; });
}

// An entity mid cross-fade from `from` to `to` (mesh names).
EntityID spawnFading(Scene &s, const char *to, const char *from, float fade) {
  EntityID e = s.spawn(to, "matA");
  auto &m = s.world->getComponent<MeshComponent>(e);
  m.meshID = s.assets().getMeshID(to);
  m.fadeMeshID = from ? s.assets().getMeshID(from) : 0;
  m.fade = fade;
  return e;
}

const fakerhi::Draw *drawWithIndexCount(const fakerhi::Recorder &r,
                                        size_t count) {
  for (const fakerhi::Draw &d : r.draws) {
    if (d.indexCount == count) {
      return &d;
    }
  }
  return nullptr;
}

} // namespace

TEST(LodFadeBatching, MaterialsLearnWhetherTheirShaderCanFade) {
  auto fade = makeFadeScene();
  EXPECT_TRUE(fade->assets()
                  .getMaterial(fade->assets().getMaterialID("matA"))
                  .supportsLodFade());

  auto plain = makeScene();
  EXPECT_FALSE(plain->assets()
                   .getMaterial(plain->assets().getMaterialID("matA"))
                   .supportsLodFade());
}

TEST(LodFadeBatching, ACrossFadeDrawsBothMeshesWithComplementaryFades) {
  auto s = makeFadeScene();
  spawnFading(*s, "quad", "tri", -0.25f); // quad dithering in, a quarter shown
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 2u);
  const fakerhi::Draw *quad = drawWithIndexCount(s->rec(), 6);
  const fakerhi::Draw *tri = drawWithIndexCount(s->rec(), 3);
  ASSERT_TRUE(quad && tri);
  ASSERT_EQ(quad->instanceFloats.size(), 21u);
  ASSERT_EQ(tri->instanceFloats.size(), 21u);

  EXPECT_FLOAT_EQ(quad->instanceFloats[kFadeFloat], -0.25f);
  // Where the new mesh keeps pixels with noise < 0.75, the old keeps the rest.
  EXPECT_FLOAT_EQ(tri->instanceFloats[kFadeFloat], 0.75f);
}

TEST(LodFadeBatching, ADitheringOutPrimaryPairsWithAnIncomingSecondary) {
  auto s = makeFadeScene();
  spawnFading(*s, "quad", "tri", 0.25f);
  s->frame();

  const fakerhi::Draw *quad = drawWithIndexCount(s->rec(), 6);
  const fakerhi::Draw *tri = drawWithIndexCount(s->rec(), 3);
  ASSERT_TRUE(quad && tri);
  EXPECT_FLOAT_EQ(quad->instanceFloats[kFadeFloat], 0.25f);
  EXPECT_FLOAT_EQ(tri->instanceFloats[kFadeFloat], -0.75f);
}

TEST(LodFadeBatching, FadingToNothingDrawsTheOldMeshAlone) {
  auto s = makeFadeScene();
  spawnFading(*s, "tri", nullptr, 0.5f);
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 1u);
  EXPECT_FLOAT_EQ(s->rec().draws[0].instanceFloats[kFadeFloat], 0.5f);
}

TEST(LodFadeBatching, EntitiesNotFadingAreWrittenWithNoFade) {
  auto s = makeFadeScene();
  s->spawn("tri", "matA", 0);
  spawnFading(*s, "tri", "quad", -0.5f);
  s->frame();

  const fakerhi::Draw *tri = drawWithIndexCount(s->rec(), 3);
  ASSERT_TRUE(tri);
  ASSERT_EQ(tri->instanceCount, 2u);
  std::vector<float> fades = {tri->instanceFloats[kFadeFloat],
                              tri->instanceFloats[21 + kFadeFloat]};
  std::sort(fades.begin(), fades.end());
  EXPECT_FLOAT_EQ(fades[0], -0.5f);
  EXPECT_FLOAT_EQ(fades[1], 0.0f) << "an ordinary instance reads 0";
}

TEST(LodFadeBatching, ShadersWithoutAFadeAttributeKeepTheHardSwitch) {
  auto s = makeScene(); // diffuseLayout: no instanceFade
  spawnFading(*s, "quad", "tri", -0.25f);
  s->frame();

  // Only the incoming mesh, and as an ordinary instance.
  ASSERT_EQ(s->rec().draws.size(), 1u);
  EXPECT_EQ(s->rec().draws[0].indexCount, 6u);
  EXPECT_EQ(s->rec().draws[0].instanceFloats.size(), 20u);
}

TEST(LodFadeBatching,
     ShadersWithoutAFadeAttributeHideAtOnceWhenFadingToNothing) {
  auto s = makeScene();
  spawnFading(*s, "tri", nullptr, 0.5f);
  s->frame();
  EXPECT_TRUE(s->rec().draws.empty());
}

TEST(LodFadeBatching, AFadingBatchGetsTheAlphaTestedPrepassVariant) {
  auto s = makeFadeScene();
  enablePrepass(*s);
  spawnFading(*s, "quad", "tri", -0.25f);
  s->frame();

  // quad and tri batches: depth pass then colour pass, each.
  const auto &draws = s->rec().draws;
  ASSERT_EQ(draws.size(), 4u);
  for (int i = 0; i < 2; ++i) {
    EXPECT_FALSE(isDepthOnly(*s, "a", draws[i].fragmentModuleId))
        << "pre-pass draw " << i << " must discard like the colour pass";
  }
  EXPECT_EQ(draws[2].shaderId, 1);
  EXPECT_EQ(draws[3].shaderId, 1);
}
TEST(LodFadeBatching, ABatchWithNoFadingInstanceKeepsTheEmptyPrepassShader) {
  auto s = makeFadeScene();
  enablePrepass(*s);
  s->spawn("tri", "matA");
  s->frame();

  const auto &draws = s->rec().draws;
  ASSERT_EQ(draws.size(), 2u);
  EXPECT_TRUE(isDepthOnly(*s, "a", draws[0].fragmentModuleId));
}
// Anti-aliasing

namespace {
// The "post_process" shader is shader id 3 in these scenes.
constexpr int kPostProcessShader = 3;

std::unique_ptr<Scene> makePostProcessScene(resources::AntiAliasing mode) {
  auto s = makeScene();
  s->loadPostProcess();
  s->world->getResource<resources::RenderSettings>().antiAliasing = mode;
  return s;
}

int postProcessDrawsOnWindow(Scene &s) {
  int n = 0;
  for (const auto &d : s.rec().fullscreenDraws) {
    n += d.shaderId == kPostProcessShader && d.targetId == 0;
  }
  return n;
}
} // namespace

TEST(AntiAliasing, NoTargetAndNoPassWhenOff) {
  auto s = makePostProcessScene(resources::AntiAliasing::None);
  s->spawn("tri", "matA");
  s->frame();

  EXPECT_TRUE(s->rec().createdTargets.empty());
  ASSERT_EQ(s->rec().draws.size(), 1u);
  EXPECT_EQ(s->rec().draws[0].targetId, 0);
  EXPECT_TRUE(s->rec().fullscreenDraws.empty());
  EXPECT_EQ(statsOf(*s).of(api::PassId::PostProcess).drawCalls, 0);
}

TEST(AntiAliasing, IsOffWithoutAPostProcessShader) {
  auto s = makeScene();
  s->world->getResource<resources::RenderSettings>().antiAliasing =
      resources::AntiAliasing::Fxaa;
  s->spawn("tri", "matA");
  s->frame();

  EXPECT_TRUE(s->rec().createdTargets.empty());
  EXPECT_EQ(s->rec().draws[0].targetId, 0);
}

TEST(AntiAliasing, SceneGoesToATargetTheSizeOfTheViewport) {
  auto s = makePostProcessScene(resources::AntiAliasing::Fxaa);
  s->rec().viewport = {1000, 500};
  s->spawn("tri", "matA");
  s->frame();

  ASSERT_EQ(s->rec().createdTargets.size(), 1u);
  const auto &target = s->rec().createdTargets[0];
  EXPECT_EQ(target.width, 1000u);
  EXPECT_EQ(target.height, 500u);
  ASSERT_EQ(s->rec().draws.size(), 1u);
  EXPECT_EQ(s->rec().draws[0].targetId, target.id);
  // Cleared where it is drawn: the target, not the window.
  ASSERT_FALSE(s->rec().clearTargetIds.empty());
  EXPECT_EQ(s->rec().clearTargetIds[0], target.id);
}

TEST(AntiAliasing, PostProcessCopiesTheTargetOntoTheWindowLast) {
  auto s = makePostProcessScene(resources::AntiAliasing::Fxaa);
  s->loadSky();
  s->world->addComponents(s->world->createEntity(), SkyboxComponent{});
  s->spawn("tri", "matA");
  s->frame();

  ASSERT_EQ(s->rec().createdTargets.size(), 1u);
  const auto &target = s->rec().createdTargets[0];

  // The sky is part of the scene; FXAA is the one full-screen draw on the
  // window, after everything.
  ASSERT_EQ(s->rec().fullscreenDraws.size(), 2u);
  EXPECT_EQ(s->rec().fullscreenDraws[0].targetId, target.id);
  const auto &fxaa = s->rec().fullscreenDraws[1];
  EXPECT_EQ(fxaa.shaderId, kPostProcessShader);
  EXPECT_EQ(fxaa.targetId, 0);
  EXPECT_EQ(s->rec().drawSequence.back(), "fullscreen");

  // It reads the target's colour (its sceneColor sampler) and draws with no
  // depth or blend.
  EXPECT_EQ(fxaa.albedoTextureId, target.colorTextureId);
  EXPECT_FALSE(fxaa.state.depthTest);
  EXPECT_FALSE(fxaa.state.depthWrite);
  EXPECT_EQ(fxaa.state.blend, api::BlendMode::None);
  EXPECT_EQ(fxaa.state.cull, api::CullMode::None);

  EXPECT_EQ(s->rec().targetBinds.back(), 0);
  EXPECT_EQ(statsOf(*s).of(api::PassId::PostProcess).drawCalls, 1);
}

TEST(AntiAliasing, TargetIsKeptAcrossFramesAndRemadeWhenTheViewportChanges) {
  auto s = makePostProcessScene(resources::AntiAliasing::Fxaa);
  s->spawn("tri", "matA");
  s->frame();
  s->frame();
  EXPECT_EQ(s->rec().createdTargets.size(), 1u);

  s->rec().viewport = {640, 360};
  s->frame();
  ASSERT_EQ(s->rec().createdTargets.size(), 2u);
  EXPECT_EQ(s->rec().createdTargets[1].width, 640u);
  EXPECT_EQ(s->rec().createdTargets[1].height, 360u);
}

TEST(AntiAliasing, TurningItOffDrawsStraightToTheWindowAgain) {
  auto s = makePostProcessScene(resources::AntiAliasing::Fxaa);
  s->spawn("tri", "matA");
  s->frame();
  EXPECT_EQ(postProcessDrawsOnWindow(*s), 1);

  s->world->getResource<resources::RenderSettings>().antiAliasing =
      resources::AntiAliasing::None;
  s->frame();

  EXPECT_EQ(postProcessDrawsOnWindow(*s), 1) << "no second FXAA draw";
  EXPECT_EQ(s->rec().draws.back().targetId, 0);
  EXPECT_EQ(s->rec().targetBinds.back(), 0);
}

TEST(AntiAliasing, EveryFrameEndsOnTheWindow) {
  auto s = makePostProcessScene(resources::AntiAliasing::Fxaa);
  s->spawn("tri", "matA");
  for (int i = 0; i < 3; ++i) {
    s->frame();
    EXPECT_EQ(s->rec().currentTarget, 0) << "frame " << i;
  }
}

TEST(AntiAliasing, PostProcessPassTellsTheShaderWhetherToSmooth) {
  auto smooth = [](Scene &s) {
    const auto &draws = s.rec().fullscreenDraws;
    EXPECT_FALSE(draws.empty());
    return floatAt(materialBytes(draws.back()),
                   memberOffset(s, "post_process", "fxaaOn"));
  };
  auto on = makePostProcessScene(resources::AntiAliasing::Fxaa);
  on->spawn("tri", "matA");
  on->frame();
  EXPECT_EQ(smooth(*on), 1.0f);

  auto scaled = makePostProcessScene(resources::AntiAliasing::None);
  scaled->world->getResource<resources::RenderSettings>().renderScale = 0.5f;
  scaled->spawn("tri", "matA");
  scaled->frame();
  EXPECT_EQ(smooth(*scaled), 0.0f);
}
// Render scale

TEST(RenderScale, OneWithoutFxaaDrawsStraightToTheWindow) {
  auto s = makePostProcessScene(resources::AntiAliasing::None);
  s->world->getResource<resources::RenderSettings>().renderScale = 1.0f;
  s->spawn("tri", "matA");
  s->frame();

  EXPECT_TRUE(s->rec().createdTargets.empty());
  EXPECT_EQ(s->rec().draws[0].targetId, 0);
}

TEST(RenderScale, ScalesTheSceneTargetEvenWithFxaaOff) {
  auto s = makePostProcessScene(resources::AntiAliasing::None);
  s->rec().viewport = {1000, 500};
  s->world->getResource<resources::RenderSettings>().renderScale = 0.5f;
  s->spawn("tri", "matA");
  s->frame();

  ASSERT_EQ(s->rec().createdTargets.size(), 1u);
  EXPECT_EQ(s->rec().createdTargets[0].width, 500u);
  EXPECT_EQ(s->rec().createdTargets[0].height, 250u);
  EXPECT_EQ(s->rec().draws[0].targetId, s->rec().createdTargets[0].id);
  // The copy to the window still runs.
  EXPECT_EQ(postProcessDrawsOnWindow(*s), 1);
}

TEST(RenderScale, ScalesUpForSupersampling) {
  auto s = makePostProcessScene(resources::AntiAliasing::Fxaa);
  s->rec().viewport = {1000, 500};
  s->world->getResource<resources::RenderSettings>().renderScale = 2.0f;
  s->spawn("tri", "matA");
  s->frame();

  ASSERT_EQ(s->rec().createdTargets.size(), 1u);
  EXPECT_EQ(s->rec().createdTargets[0].width, 2000u);
  EXPECT_EQ(s->rec().createdTargets[0].height, 1000u);
}

TEST(RenderScale, IsClampedToWhatTheUiOffers) {
  auto s = makePostProcessScene(resources::AntiAliasing::None);
  s->rec().viewport = {1000, 500};
  auto &settings = s->world->getResource<resources::RenderSettings>();
  settings.renderScale = 100.0f;
  s->spawn("tri", "matA");
  s->frame();
  EXPECT_EQ(s->rec().createdTargets.back().width, 2000u);

  settings.renderScale = 0.0f;
  s->frame();
  EXPECT_EQ(s->rec().createdTargets.back().width, 250u);
}

TEST(RenderScale, ChangingItRemakesTheTargetAndOneGoesBackToTheWindow) {
  auto s = makePostProcessScene(resources::AntiAliasing::None);
  s->rec().viewport = {1000, 500};
  auto &settings = s->world->getResource<resources::RenderSettings>();
  settings.renderScale = 0.5f;
  s->spawn("tri", "matA");
  s->frame();
  s->frame();
  EXPECT_EQ(s->rec().createdTargets.size(), 1u) << "kept while unchanged";

  settings.renderScale = 0.75f;
  s->frame();
  ASSERT_EQ(s->rec().createdTargets.size(), 2u);
  EXPECT_EQ(s->rec().createdTargets[1].width, 750u);

  settings.renderScale = 1.0f;
  s->frame();
  EXPECT_EQ(s->rec().createdTargets.size(), 2u);
  EXPECT_EQ(s->rec().draws.back().targetId, 0);
}

TEST(RenderScale, IsIgnoredWithoutTheFxaaShader) {
  auto s = makeScene();
  s->world->getResource<resources::RenderSettings>().renderScale = 0.5f;
  s->world->getResource<resources::RenderSettings>().antiAliasing =
      resources::AntiAliasing::None;
  s->spawn("tri", "matA");
  s->frame();

  EXPECT_TRUE(s->rec().createdTargets.empty());
  EXPECT_EQ(s->rec().draws[0].targetId, 0);
}
