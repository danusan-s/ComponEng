#include "componeng/renderer/frame_data.hpp"
#include "componeng/renderer/shader/compiler.hpp"
#include "componeng/renderer/shader/reflection.hpp"
#include "componeng/utils/utils.hpp"
#include "gtest/gtest.h"

#include <fstream>
#include <sstream>

using namespace componeng::renderer;
using namespace componeng::renderer::shader;

namespace {

std::string shadersDir() {
  return componeng::utils::Utils::getAssetPath("assets/shaders");
}

std::string readShader(const std::string &file) {
  std::ifstream f(shadersDir() + "/" + file);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

CompiledStage build(rhi::ShaderStage stage, const std::string &file, Target t,
                    ShaderFlag flags = ShaderFlag::None) {
  return compile(stage, readShader(file), shadersDir() + "/" + file, t, flags,
                 {shadersDir()});
}

struct Bundled {
  const char *name;
  bool instanced;
  // Members a material sets by name; empty when the shader has no block.
  std::vector<const char *> materialMembers;
};

const Bundled kShaders[] = {
    {"diffuse", true, {}},
    {"pbr", true, {"useTextures", "useNormalMap"}},
    {"skybox_procedural",
     false,
     {"invSkyViewProj", "skyColor", "horizonColor"}},
    {"post_process", false, {"invResolution", "fxaaOn"}},
};

} // namespace

class BundledShader
    : public ::testing::TestWithParam<std::tuple<Bundled, Target>> {};

TEST_P(BundledShader, CompilesAndMergesBothStages) {
  const auto &[shader, target] = GetParam();
  const CompiledStage vs = build(rhi::ShaderStage::Vertex,
                                 std::string(shader.name) + ".vert", target);
  const CompiledStage fs = build(rhi::ShaderStage::Fragment,
                                 std::string(shader.name) + ".frag", target);
  ASSERT_TRUE(vs.ok) << vs.log;
  ASSERT_TRUE(fs.ok) << fs.log;

  Reflection merged;
  std::string error;
  ASSERT_TRUE(mergeReflection({&vs.reflection, &fs.reflection}, merged, error))
      << error;

  if (!shader.materialMembers.empty()) {
    const ReflectedResource *block = nullptr;
    for (const ReflectedResource &r : merged.resources) {
      if (r.set == 1 && r.binding == 0) {
        block = &r;
      }
    }
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(block->kind, ResourceKind::UniformBlock);
    for (const char *m : shader.materialMembers) {
      EXPECT_NE(block->member(m), nullptr) << m;
    }
  }

  // Every instanced shader must take the per-instance inputs the diffuse
  // material writes; the others must take none.
  std::size_t instanceInputs = 0;
  for (const VertexInput &in : merged.vertexInputs) {
    instanceInputs += in.location >= 3 ? 1 : 0;
  }
  EXPECT_EQ(instanceInputs > 0, shader.instanced);
}

INSTANTIATE_TEST_SUITE_P(Bundled, BundledShader,
                         ::testing::Combine(::testing::ValuesIn(kShaders),
                                            ::testing::Values(Target::Vulkan,
                                                              Target::OpenGL)));

TEST(BundledShaders, FrameDataMatchesTheCppStruct) {
  for (const char *file :
       {"diffuse.vert", "pbr.frag", "skybox_procedural.frag"}) {
    const std::string f = file;
    const CompiledStage s = build(f.rfind(".vert") != std::string::npos
                                      ? rhi::ShaderStage::Vertex
                                      : rhi::ShaderStage::Fragment,
                                  f, Target::OpenGL);
    ASSERT_TRUE(s.ok) << s.log;
    const ReflectedResource *frame = s.reflection.find("FrameData");
    ASSERT_NE(frame, nullptr) << file;
    EXPECT_EQ(frame->set, 0u);
    EXPECT_EQ(frame->binding, kFrameDataBinding);
    EXPECT_EQ(frame->sizeBytes, sizeof(FrameData)) << file;
    EXPECT_EQ(frame->member("cameraPos")->offset,
              offsetof(FrameData, cameraPos));
    EXPECT_EQ(frame->member("lightDir")->offset, offsetof(FrameData, lightDir));
    EXPECT_EQ(frame->member("lightColor")->offset,
              offsetof(FrameData, lightColor));
    EXPECT_EQ(frame->member("ambientColor")->offset,
              offsetof(FrameData, ambientColor));
    EXPECT_EQ(frame->member("fogColor")->offset, offsetof(FrameData, fogColor));
    EXPECT_EQ(frame->member("fogEnd")->offset, offsetof(FrameData, fogEnd));
  }
}

TEST(BundledShaders, DepthVariantsShareTheVertexModule) {
  // The depth pre-pass and the colour pass must compute the same depth, so the
  // vertex stage is compiled once, without flags, and reused.
  for (Target t : {Target::Vulkan, Target::OpenGL}) {
    const CompiledStage a = build(rhi::ShaderStage::Vertex, "pbr.vert", t);
    const CompiledStage b = build(rhi::ShaderStage::Vertex, "pbr.vert", t);
    ASSERT_TRUE(a.ok && b.ok);
    EXPECT_EQ(a.spirv, b.spirv);
  }
}

TEST(BundledShaders, AlphaTestedDepthVariantsCompile) {
  for (const char *file : {"diffuse.frag", "pbr.frag"}) {
    for (Target t : {Target::Vulkan, Target::OpenGL}) {
      const CompiledStage s =
          build(rhi::ShaderStage::Fragment, file, t, ShaderFlag::DepthPrepass);
      EXPECT_TRUE(s.ok) << file << ": " << s.log;
    }
  }
}
