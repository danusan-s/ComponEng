#include "componeng/renderer/shader/compiler.hpp"
#include "componeng/renderer/shader/include_expander.hpp"
#include "componeng/renderer/shader/reflection.hpp"
#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace componeng::renderer;
using namespace componeng::renderer::shader;
namespace fs = std::filesystem;

namespace {

const char *kVertex = R"(#version 460
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in mat4 instanceModel;
layout(location = 7) in vec4 instanceColor;
layout(std140, BIND(0, 0)) uniform FrameData {
  mat4 viewProj;
  vec3 cameraPos;
  float time;
};
layout(std140, BIND(1, 0)) uniform Material {
  vec4 tint;
  float shininess;
  vec3 emissive;
};
layout(BIND(1, 1)) uniform sampler2D albedoMap;
layout(location = 0) out vec4 vColor;
void main() {
  vec4 t = texture(albedoMap, aUV);
  vColor = instanceColor * tint * t +
           vec4(emissive * shininess + cameraPos * time + aNormal, 0.0);
  gl_Position = viewProj * instanceModel * vec4(aPos, 1.0);
}
)";

const char *kFragment = R"(#version 460
layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 outColor;
layout(std140, BIND(0, 0)) uniform FrameData {
  mat4 viewProj;
  vec3 cameraPos;
  float time;
};
layout(std140, BIND(1, 0)) uniform Material {
  vec4 tint;
  float shininess;
  vec3 emissive;
};
layout(BIND(1, 1)) uniform sampler2D albedoMap;
void main() {
  outColor = vColor + texture(albedoMap, vec2(0.0)) * tint +
             vec4(cameraPos * time + emissive * shininess, 0.0);
}
)";

CompiledStage compileVertex(const std::string &src, Target t,
                            ShaderFlag f = ShaderFlag::None) {
  return compile(rhi::ShaderStage::Vertex, src, "test.vert", t, f);
}

CompiledStage compileFragment(const std::string &src, Target t,
                              ShaderFlag f = ShaderFlag::None) {
  return compile(rhi::ShaderStage::Fragment, src, "test.frag", t, f);
}

constexpr uint32_t kSpirvMagic = 0x07230203;

struct TempDir {
  fs::path path;
  TempDir() {
    path = fs::temp_directory_path() /
           ("componeng_shader_" + std::to_string(getpid()));
    fs::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  std::string write(const std::string &rel, const std::string &text) const {
    const fs::path p = path / rel;
    fs::create_directories(p.parent_path());
    std::ofstream(p) << text;
    return p.string();
  }
};

} // namespace

class ShaderCompilerTarget : public ::testing::TestWithParam<Target> {};

TEST_P(ShaderCompilerTarget, ProducesSpirv) {
  const CompiledStage vs = compileVertex(kVertex, GetParam());
  ASSERT_TRUE(vs.ok) << vs.log;
  ASSERT_FALSE(vs.spirv.empty());
  EXPECT_EQ(vs.spirv[0], kSpirvMagic);
  const CompiledStage fs = compileFragment(kFragment, GetParam());
  ASSERT_TRUE(fs.ok) << fs.log;
  EXPECT_EQ(fs.spirv[0], kSpirvMagic);
}

TEST_P(ShaderCompilerTarget, ReflectsBlocksSamplersAndInputs) {
  const CompiledStage vs = compileVertex(kVertex, GetParam());
  ASSERT_TRUE(vs.ok) << vs.log;
  const Reflection &r = vs.reflection;

  ASSERT_EQ(r.resources.size(), 3u);
  const ReflectedResource *frame = r.find("FrameData");
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(frame->kind, ResourceKind::UniformBlock);
  EXPECT_EQ(frame->set, 0u);
  EXPECT_EQ(frame->binding, 0u);
  EXPECT_EQ(frame->sizeBytes, 80u);
  ASSERT_NE(frame->member("cameraPos"), nullptr);
  EXPECT_EQ(frame->member("cameraPos")->offset, 64u);
  EXPECT_EQ(frame->member("cameraPos")->type, ValueType::Vec3);
  EXPECT_EQ(frame->member("time")->offset, 76u);

  const ReflectedResource *material = r.find("Material");
  ASSERT_NE(material, nullptr);
  EXPECT_EQ(material->set, 1u);
  EXPECT_EQ(material->binding, 0u);
  // vec3 ends at 44; std140 rounds the block up to 48.
  EXPECT_EQ(material->sizeBytes, 48u);
  EXPECT_EQ(material->member("tint")->offset, 0u);
  EXPECT_EQ(material->member("shininess")->offset, 16u);
  EXPECT_EQ(material->member("emissive")->offset, 32u);

  const ReflectedResource *albedo = r.find("albedoMap");
  ASSERT_NE(albedo, nullptr);
  EXPECT_EQ(albedo->kind, ResourceKind::Sampler2D);
  EXPECT_EQ(albedo->set, 1u);
  EXPECT_EQ(albedo->binding, 1u);

  ASSERT_EQ(r.vertexInputs.size(), 5u);
  EXPECT_EQ(r.vertexInputs[3].name, "instanceModel");
  EXPECT_EQ(r.vertexInputs[3].location, 3u);
  EXPECT_EQ(r.vertexInputs[3].type, ValueType::Mat4);
  EXPECT_EQ(r.vertexInputs[4].name, "instanceColor");
  EXPECT_EQ(r.vertexInputs[4].location, 7u);
  EXPECT_EQ(r.vertexInputs[4].type, ValueType::Vec4);
}

TEST_P(ShaderCompilerTarget, StorageBlocksAreReflected) {
  const std::string src = R"(#version 460
layout(std430, BIND(1, 2)) readonly buffer Positions { vec4 positions[]; };
void main() { gl_Position = positions[INSTANCE_INDEX]; }
)";
  const CompiledStage vs = compileVertex(src, GetParam());
  ASSERT_TRUE(vs.ok) << vs.log;
  const ReflectedResource *r = vs.reflection.find("Positions");
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->kind, ResourceKind::StorageBlock);
  EXPECT_EQ(r->set, 1u);
  EXPECT_EQ(r->binding, 2u);
}

TEST_P(ShaderCompilerTarget, IndexMacrosCompile) {
  const std::string src = R"(#version 460
void main() {
  gl_Position = vec4(float(VERTEX_INDEX), float(INSTANCE_INDEX), 0.0, 1.0);
}
)";
  const CompiledStage vs = compileVertex(src, GetParam());
  EXPECT_TRUE(vs.ok) << vs.log;
}

TEST_P(ShaderCompilerTarget, DepthPrepassFlagDefinesTheMacro) {
  const std::string src = R"(#version 460
layout(location = 0) out vec4 o;
void main() {
#ifdef COMPONENG_DEPTH_PREPASS
#error flag was set
#endif
  o = vec4(1.0);
}
)";
  EXPECT_TRUE(compileFragment(src, GetParam()).ok);
  const CompiledStage flagged =
      compileFragment(src, GetParam(), ShaderFlag::DepthPrepass);
  EXPECT_FALSE(flagged.ok);
  EXPECT_NE(flagged.log.find("flag was set"), std::string::npos);
}

TEST_P(ShaderCompilerTarget, SameSourceGivesSameSpirv) {
  // The depth variants rely on the vertex stage being identical every time.
  const CompiledStage a = compileVertex(kVertex, GetParam());
  const CompiledStage b = compileVertex(kVertex, GetParam());
  ASSERT_TRUE(a.ok && b.ok);
  EXPECT_EQ(a.spirv, b.spirv);
}

INSTANTIATE_TEST_SUITE_P(Targets, ShaderCompilerTarget,
                         ::testing::Values(Target::Vulkan, Target::OpenGL));

TEST(ShaderCompiler, TargetsReflectTheSameLogicalBindings) {
  const CompiledStage vk = compileVertex(kVertex, Target::Vulkan);
  const CompiledStage gl = compileVertex(kVertex, Target::OpenGL);
  ASSERT_TRUE(vk.ok && gl.ok);
  ASSERT_EQ(vk.reflection.resources.size(), gl.reflection.resources.size());
  for (std::size_t i = 0; i < vk.reflection.resources.size(); ++i) {
    EXPECT_TRUE(
        vk.reflection.resources[i].sameDeclaration(gl.reflection.resources[i]));
  }
  EXPECT_NE(vk.spirv, gl.spirv);
}

TEST(ShaderCompiler, RejectsLooseUniforms) {
  const std::string src = R"(#version 460
uniform float strength;
void main() { gl_Position = vec4(strength); }
)";
  const CompiledStage vs = compileVertex(src, Target::Vulkan);
  EXPECT_FALSE(vs.ok);
  EXPECT_NE(vs.log.find("strength"), std::string::npos);
}

TEST(ShaderCompiler, RejectsLooseUniformsWithALocation) {
  const std::string src = R"(#version 460
layout(location = 0) uniform float strength;
void main() { gl_Position = vec4(strength); }
)";
  const CompiledStage vs = compileVertex(src, Target::OpenGL);
  EXPECT_FALSE(vs.ok);
  EXPECT_NE(vs.log.find("loose uniform 'strength'"), std::string::npos);
}

TEST(ShaderCompiler, RejectsUnsupportedResourceTypes) {
  const std::string src = R"(#version 460
layout(BIND(1, 1)) uniform samplerCube env;
void main() { gl_Position = texture(env, vec3(0.0)); }
)";
  const CompiledStage vs = compileVertex(src, Target::Vulkan);
  EXPECT_FALSE(vs.ok);
  EXPECT_NE(vs.log.find("unsupported resource 'env'"), std::string::npos);
}

TEST(ShaderCompiler, RejectsRawDescriptorSets) {
  const std::string src = R"(#version 460
layout(std140, set = 1, binding = 0) uniform Material { vec4 tint; };
void main() { gl_Position = tint; }
)";
  EXPECT_FALSE(compileVertex(src, Target::Vulkan).ok);
  EXPECT_FALSE(compileVertex(src, Target::OpenGL).ok);
}

TEST(ShaderCompiler, RejectsSharedBindings) {
  const std::string src = R"(#version 460
layout(std140, BIND(1, 0)) uniform A { vec4 a; };
layout(std140, BIND(1, 0)) uniform B { vec4 b; };
void main() { gl_Position = a + b; }
)";
  const CompiledStage vs = compileVertex(src, Target::OpenGL);
  EXPECT_FALSE(vs.ok);
}

TEST(ShaderCompiler, RejectsOutOfRangeBindings) {
  const std::string src = R"(#version 460
layout(std140, BIND(2, 0)) uniform A { vec4 a; };
void main() { gl_Position = a; }
)";
  const CompiledStage vs = compileVertex(src, Target::Vulkan);
  EXPECT_FALSE(vs.ok);
  EXPECT_NE(vs.log.find("out of range"), std::string::npos);
}

TEST(ShaderCompiler, RejectsBlocksWithoutABinding) {
  const std::string src = R"(#version 460
layout(std140) uniform A { vec4 a; };
void main() { gl_Position = a; }
)";
  EXPECT_FALSE(compileVertex(src, Target::OpenGL).ok);
}

TEST(ShaderCompiler, RejectsWrongMeshInputs) {
  const std::string src = R"(#version 460
layout(location = 0) in vec4 position;
void main() { gl_Position = position; }
)";
  const CompiledStage vs = compileVertex(src, Target::Vulkan);
  EXPECT_FALSE(vs.ok);
  EXPECT_NE(vs.log.find("aPos"), std::string::npos);
}

TEST(ShaderCompiler, RejectsOtherVersions) {
  const CompiledStage vs = compileVertex(
      "#version 450\nvoid main() { gl_Position = vec4(0); }\n", Target::Vulkan);
  EXPECT_FALSE(vs.ok);
  EXPECT_NE(vs.log.find("460"), std::string::npos);
  EXPECT_FALSE(compileVertex("void main() {}\n", Target::Vulkan).ok);
}

TEST(ShaderCompiler, MergeCombinesStages) {
  const CompiledStage vs = compileVertex(kVertex, Target::Vulkan);
  const CompiledStage fs = compileFragment(kFragment, Target::Vulkan);
  ASSERT_TRUE(vs.ok && fs.ok) << vs.log << fs.log;

  Reflection merged;
  std::string error;
  ASSERT_TRUE(mergeReflection({&vs.reflection, &fs.reflection}, merged, error))
      << error;
  ASSERT_EQ(merged.resources.size(), 3u);
  EXPECT_EQ(merged.resources[0].name, "FrameData");
  EXPECT_EQ(merged.resources[0].stages, rhi::StageFlags::AllGraphics);
  EXPECT_EQ(merged.vertexInputs.size(), 5u);

  const rhi::BindGroupLayoutDesc set1 = setLayout(merged, 1);
  ASSERT_EQ(set1.bindings.size(), 2u);
  EXPECT_EQ(set1.bindings[0].binding, 0u);
  EXPECT_EQ(set1.bindings[0].type, rhi::BindingType::UniformBuffer);
  EXPECT_EQ(set1.bindings[1].type, rhi::BindingType::CombinedImageSampler);
  EXPECT_EQ(setLayout(merged, 0).bindings.size(), 1u);
}

TEST(ShaderCompiler, MergeRejectsDifferentDeclarations) {
  const std::string frag = R"(#version 460
layout(location = 0) out vec4 o;
layout(std140, BIND(0, 0)) uniform FrameData { mat4 viewProj; vec4 other; };
void main() { o = other; }
)";
  const CompiledStage vs = compileVertex(kVertex, Target::Vulkan);
  const CompiledStage fs = compileFragment(frag, Target::Vulkan);
  ASSERT_TRUE(vs.ok && fs.ok) << vs.log << fs.log;
  Reflection merged;
  std::string error;
  EXPECT_FALSE(
      mergeReflection({&vs.reflection, &fs.reflection}, merged, error));
  EXPECT_NE(error.find("FrameData"), std::string::npos);
}

TEST(IncludeExpander, ExpandsAndWrapsInLineDirectives) {
  TempDir dir;
  dir.write("a.glsl", "float a() { return 1.0; }\n");
  const std::string main = dir.write("main.vert", "");
  const ExpandedSource e =
      expandIncludes("#include \"a.glsl\"\nvoid main() {}\n", main, {});
  EXPECT_TRUE(e.errors.empty());
  EXPECT_NE(e.text.find("float a()"), std::string::npos);
  EXPECT_NE(e.text.find("#line 1 \"" + (dir.path / "a.glsl").string() + "\""),
            std::string::npos);
  EXPECT_NE(e.text.find("#line 2 \"" + main + "\""), std::string::npos);
  EXPECT_EQ(e.text.find("#include"), std::string::npos);
}

TEST(IncludeExpander, IncludesEachFileOnce) {
  TempDir dir;
  dir.write("a.glsl", "float a() { return 1.0; }\n");
  dir.write("b.glsl", "#include \"a.glsl\"\nfloat b() { return a(); }\n");
  const std::string main = dir.write("main.vert", "");
  const ExpandedSource e = expandIncludes(
      "#include \"a.glsl\"\n#include \"b.glsl\"\n#include \"a.glsl\"\n", main,
      {});
  EXPECT_TRUE(e.errors.empty());
  std::size_t count = 0;
  for (std::size_t p = e.text.find("float a()"); p != std::string::npos;
       p = e.text.find("float a()", p + 1)) {
    ++count;
  }
  EXPECT_EQ(count, 1u);
}

TEST(IncludeExpander, SearchesRootsAfterTheIncludingFile) {
  TempDir dir;
  dir.write("shaders/common.glsl", "// from root\n");
  dir.write("game/common.glsl", "// from game\n");
  const std::string main = dir.write("game/main.vert", "");
  const std::string root = (dir.path / "shaders").string();

  ExpandedSource e = expandIncludes("#include \"common.glsl\"\n", main, {root});
  EXPECT_NE(e.text.find("from game"), std::string::npos);

  dir.write("shaders/only_root.glsl", "// only in root\n");
  e = expandIncludes("#include \"only_root.glsl\"\n", main, {root});
  EXPECT_TRUE(e.errors.empty());
  EXPECT_NE(e.text.find("only in root"), std::string::npos);
}

TEST(IncludeExpander, ReportsMissingFiles) {
  const ExpandedSource e =
      expandIncludes("#include \"nope.glsl\"\n", "x.vert", {});
  ASSERT_EQ(e.errors.size(), 1u);
  EXPECT_NE(e.errors[0].find("nope.glsl"), std::string::npos);
  EXPECT_NE(e.errors[0].find("x.vert:1"), std::string::npos);
  EXPECT_NE(e.text.find("#error"), std::string::npos);
}

TEST(IncludeExpander, LimitsNesting) {
  TempDir dir;
  for (int i = 0; i < 20; ++i) {
    dir.write("f" + std::to_string(i) + ".glsl",
              "#include \"f" + std::to_string(i + 1) + ".glsl\"\n");
  }
  dir.write("f20.glsl", "// end\n");
  const std::string main = dir.write("main.vert", "");
  const ExpandedSource e = expandIncludes("#include \"f0.glsl\"\n", main, {});
  ASSERT_FALSE(e.errors.empty());
  EXPECT_NE(e.errors[0].find("deeper"), std::string::npos);
}

TEST(ShaderCompiler, IncludesAndErrorsNameTheRealFile) {
  TempDir dir;
  const std::string inc =
      dir.write("shared.glsl", "float broken() { return nope; }\n");
  const std::string main = dir.write("main.vert", "");
  const std::string src =
      "#version 460\n#include \"shared.glsl\"\nvoid main() { gl_Position = "
      "vec4(broken()); }\n";
  const CompiledStage vs = compile(rhi::ShaderStage::Vertex, src, main,
                                   Target::Vulkan, ShaderFlag::None);
  EXPECT_FALSE(vs.ok);
  EXPECT_NE(vs.log.find(inc + ":1"), std::string::npos) << vs.log;

  dir.write("good.glsl", "float good() { return 1.0; }\n");
  const std::string ok =
      "#version 460\n#include \"good.glsl\"\nvoid main() { gl_Position = "
      "vec4(good()); }\n";
  EXPECT_TRUE(compile(rhi::ShaderStage::Vertex, ok, main, Target::OpenGL,
                      ShaderFlag::None)
                  .ok);
}

TEST(ShaderCompiler, MissingIncludeFailsTheCompile) {
  const std::string src =
      "#version 460\n#include \"missing.glsl\"\nvoid main() {}\n";
  const CompiledStage vs = compileVertex(src, Target::Vulkan);
  EXPECT_FALSE(vs.ok);
  EXPECT_NE(vs.log.find("missing.glsl"), std::string::npos);
}
