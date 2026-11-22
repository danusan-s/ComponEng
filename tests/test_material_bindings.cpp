#include "render_scene_fixture.hpp"

#include "componeng/renderer/component/procedural_instances_component.hpp"
#include "gtest/gtest.h"

#include <cstring>

using namespace rscene;

namespace {

// Shader "a" declares the extra resources the bindings below use.
std::unique_ptr<Scene> makeBindingScene() {
  return makeScene([](SceneOptions &o) { o.extras = true; });
}

api::TextureDesc dataTexture() {
  api::TextureDesc desc;
  desc.mipmaps = false;
  desc.minFilter = api::TextureFilter::Nearest;
  desc.magFilter = api::TextureFilter::Nearest;
  return desc;
}

// The binding of `name` in shader a's set 1.
uint64_t slotOf(Scene &s, const char *shader, const char *name) {
  const Shader &sh = s.assets().getShader(s.assets().getShaderID(shader));
  const auto *r = sh.reflection().find(name);
  return fakerhi::bindingKey(r->set, r->binding);
}

} // namespace

// --- Textures from memory ----------------------------------------------------

TEST(TextureFormat, ChannelsAndByteSizes) {
  using api::TextureFormat;
  static_assert(api::textureChannels(TextureFormat::R8) == 1);
  static_assert(api::textureChannels(TextureFormat::RG16F) == 2);
  static_assert(api::textureChannels(TextureFormat::RGB8) == 3);
  static_assert(api::textureChannels(TextureFormat::RGBA32F) == 4);
  static_assert(!api::textureIsFloat(TextureFormat::RGBA8));
  static_assert(api::textureIsFloat(TextureFormat::R16F));
  static_assert(api::textureCpuPixelBytes(TextureFormat::RGB8) == 3);
  // Half-float textures are still supplied as 32-bit floats on the CPU.
  static_assert(api::textureCpuPixelBytes(TextureFormat::R16F) == 4);
  static_assert(api::textureCpuPixelBytes(TextureFormat::RGBA32F) == 16);
  SUCCEED();
}

TEST(CreateTexture, RecordsFormatSizeDescAndPixels) {
  auto s = makeBindingScene();
  const size_t imagesBefore = s->rec().createdImages.size();
  const float heights[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  s->assets().createTexture("heights", 2, 2, api::TextureFormat::R32F, heights,
                            dataTexture());

  ASSERT_TRUE(s->assets().textureExists("heights"));
  ASSERT_EQ(s->rec().createdImages.size(), imagesBefore + 1);
  const auto &image = s->rec().createdImages.back().desc;
  EXPECT_EQ(image.extent, (rhi::Extent2D{2, 2}));
  EXPECT_EQ(image.format, rhi::Format::R32F);
  EXPECT_EQ(image.mipLevels, 1u) << "no mipmaps requested";
  EXPECT_EQ(s->rec().createdSamplers.back().magFilter, rhi::Filter::Nearest);

  const auto &upload = s->rec().imageUploads.back();
  ASSERT_EQ(upload.pixels.size(), sizeof(heights));
  EXPECT_EQ(std::memcmp(upload.pixels.data(), heights, sizeof(heights)), 0);

  const auto &tex = s->assets().getTexture(s->assets().getTextureID("heights"));
  EXPECT_EQ(tex.width(), 2u);
  EXPECT_EQ(tex.height(), 2u);
}

TEST(CreateTexture, NullPixelsOnlyAllocate) {
  auto s = makeBindingScene();
  const size_t uploadsBefore = s->rec().imageUploads.size();
  s->assets().createTexture("empty", 8, 8, api::TextureFormat::RGBA16F,
                            nullptr);
  EXPECT_EQ(s->rec().createdImages.back().desc.format, rhi::Format::RGBA16F);
  EXPECT_EQ(s->rec().imageUploads.size(), uploadsBefore);
}

TEST(UpdateTexture, ForwardsTheRegionAndItsPixels) {
  auto s = makeBindingScene();
  const unsigned char zeros[16] = {};
  s->assets().createTexture("mask", 4, 4, api::TextureFormat::R8, zeros);
  const size_t before = s->rec().imageUploads.size();
  const unsigned char patch[4] = {10, 20, 30, 40};

  EXPECT_TRUE(s->assets().updateTexture(s->assets().getTextureID("mask"), 1, 2,
                                        2, 2, patch));
  ASSERT_EQ(s->rec().imageUploads.size(), before + 1);
  const auto &u = s->rec().imageUploads.back();
  EXPECT_EQ(u.region.x, 1);
  EXPECT_EQ(u.region.y, 2);
  EXPECT_EQ(u.region.width, 2u);
  EXPECT_EQ(u.region.height, 2u);
  EXPECT_EQ(u.pixels, std::vector<unsigned char>(patch, patch + 4));
}

TEST(UpdateTexture, RegionsOutsideTheTextureAreRejected) {
  auto s = makeBindingScene();
  const unsigned char zeros[16] = {};
  s->assets().createTexture("mask", 4, 4, api::TextureFormat::R8, zeros);
  const HandleID id = s->assets().getTextureID("mask");
  const unsigned char patch[16] = {};
  const size_t before = s->rec().imageUploads.size();

  EXPECT_FALSE(s->assets().updateTexture(id, 3, 3, 2, 2, patch)); // spills over
  EXPECT_FALSE(s->assets().updateTexture(id, 5, 0, 1, 1, patch)); // starts out
  EXPECT_FALSE(s->assets().updateTexture(id, 0, 0, 0, 2, patch)); // empty
  EXPECT_EQ(s->rec().imageUploads.size(), before);
  // The whole texture is still a valid region.
  EXPECT_TRUE(s->assets().updateTexture(id, 0, 0, 4, 4, patch));
}

// --- Buffers -----------------------------------------------------------------

TEST(Buffers, CreateUpdateAndRangeChecks) {
  auto s = makeBindingScene();
  const float values[4] = {1, 2, 3, 4};
  s->assets().createBuffer("values", values, sizeof(values));
  ASSERT_TRUE(s->assets().bufferExists("values"));
  const HandleID id = s->assets().getBufferID("values");
  EXPECT_EQ(s->assets().getBuffer(id).size(), sizeof(values));

  const float patch = 9.0f;
  EXPECT_TRUE(
      s->assets().updateBuffer(id, sizeof(float), &patch, sizeof(patch)));
  EXPECT_FALSE(
      s->assets().updateBuffer(id, sizeof(values), &patch, sizeof(patch)));
  EXPECT_FALSE(s->assets().bufferExists("nope"));
}

// --- Material bindings -------------------------------------------------------

TEST(MaterialTextures, BoundAtTheSamplersOwnSlotInTheMaterialSet) {
  auto s = makeBindingScene();
  const float heights[4] = {};
  s->assets().createTexture("heights", 2, 2, api::TextureFormat::R32F, heights);
  const HandleID tex = s->assets().getTextureID("heights");
  s->assets()
      .getMaterial(s->assets().getMaterialID("matA"))
      .setTexture("heightMap", tex);

  s->spawn("tri", "matA");
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 1u);
  const auto &d = s->rec().draws[0];
  const uint64_t slot = slotOf(*s, "a", "heightMap");
  ASSERT_TRUE(d.imageAt.count(slot));
  EXPECT_EQ(d.imageAt.at(slot), int(tex));
  EXPECT_EQ(d.albedoTextureId, 1) << "the albedo is still t1";
}

TEST(MaterialTextures, SeveralCanBeSetAndSettingAgainReplaces) {
  auto s = makeBindingScene();
  const float one = 0.0f;
  s->assets().createTexture("a", 1, 1, api::TextureFormat::R32F, &one);
  s->assets().createTexture("b", 1, 1, api::TextureFormat::R32F, &one);
  s->assets().createTexture("c", 1, 1, api::TextureFormat::R32F, &one);
  auto &mat = s->assets().getMaterial(s->assets().getMaterialID("matA"));
  mat.setTexture("first", s->assets().getTextureID("a"));
  mat.setTexture("second", s->assets().getTextureID("b"));
  mat.setTexture("first", s->assets().getTextureID("c")); // replaces "a"

  EXPECT_EQ(mat.getTextureBindings().size(), 2u);

  s->spawn("tri", "matA");
  s->frame();

  const auto &d = s->rec().draws.at(0);
  EXPECT_EQ(d.imageAt.at(slotOf(*s, "a", "first")),
            int(s->assets().getTextureID("c")));
  EXPECT_EQ(d.imageAt.at(slotOf(*s, "a", "second")),
            int(s->assets().getTextureID("b")));
}

TEST(MaterialTextures, ChangingOneRebuildsTheGroupAndTheNextDrawSeesIt) {
  auto s = makeBindingScene();
  const float one = 0.0f;
  s->assets().createTexture("a", 1, 1, api::TextureFormat::R32F, &one);
  s->assets().createTexture("b", 1, 1, api::TextureFormat::R32F, &one);
  auto &mat = s->assets().getMaterial(s->assets().getMaterialID("matA"));
  mat.setTexture("heightMap", s->assets().getTextureID("a"));
  s->spawn("tri", "matA");
  s->frame();
  const int groupsBefore = s->rec().groupsCreated;

  s->frame();
  EXPECT_EQ(s->rec().groupsCreated, groupsBefore) << "unchanged: reused";

  mat.setTexture("heightMap", s->assets().getTextureID("b"));
  s->frame();
  EXPECT_EQ(s->rec().groupsCreated, groupsBefore + 1);
  EXPECT_EQ(s->rec().draws.back().imageAt.at(slotOf(*s, "a", "heightMap")),
            int(s->assets().getTextureID("b")));
}

TEST(MaterialTextures, ASamplerNobodyGaveATextureReadsTheWhiteFallback) {
  auto s = makeBindingScene();
  s->spawn("tri", "matA");
  s->frame();

  const auto &d = s->rec().draws.at(0);
  // The draw happened, with something bound at every sampler.
  EXPECT_NE(d.imageAt.at(slotOf(*s, "a", "heightMap")), 0);
  EXPECT_TRUE(s->rec().errors.empty());
}

TEST(MaterialTextures, OtherMaterialsDoNotGetThem) {
  auto s = makeBindingScene();
  const float one = 0.0f;
  s->assets().createTexture("a", 1, 1, api::TextureFormat::R32F, &one);
  s->assets()
      .getMaterial(s->assets().getMaterialID("matA"))
      .setTexture("heightMap", s->assets().getTextureID("a"));

  s->spawn("tri", "matB"); // shader "b" has no heightMap at all
  s->frame();

  const auto &d = s->rec().draws.at(0);
  const uint64_t slot = fakerhi::bindingKey(1, 3);
  EXPECT_FALSE(d.imageAt.count(slot));
}

TEST(MaterialUniformBlocks, BufferBoundAtTheBlocksOwnBinding) {
  auto s = makeBindingScene();
  const float settings[4] = {0.25f, 0.5f, 0.75f, 1.0f};
  s->assets().createBuffer("settings", settings, sizeof(settings));
  s->assets()
      .getMaterial(s->assets().getMaterialID("matA"))
      .setUniformBuffer("Settings", s->assets().getBufferID("settings"));

  s->spawn("tri", "matA");
  s->frame();

  const auto &d = s->rec().draws.at(0);
  const uint64_t slot = slotOf(*s, "a", "Settings");
  ASSERT_TRUE(d.bufferBytes.count(slot));
  const auto &bytes = d.bufferBytes.at(slot);
  ASSERT_GE(bytes.size(), sizeof(settings));
  EXPECT_EQ(std::memcmp(bytes.data(), settings, sizeof(settings)), 0);
}

TEST(MaterialUniformBlocks, AnUpdatedBufferIsSeenWithoutRebuildingAnything) {
  auto s = makeBindingScene();
  const float settings[4] = {1, 2, 3, 4};
  s->assets().createBuffer("settings", settings, sizeof(settings));
  const HandleID id = s->assets().getBufferID("settings");
  s->assets()
      .getMaterial(s->assets().getMaterialID("matA"))
      .setUniformBuffer("Settings", id);
  s->spawn("tri", "matA");
  s->frame();
  const int groups = s->rec().groupsCreated;

  const float patch = 9.0f;
  s->assets().updateBuffer(id, 0, &patch, sizeof(patch));
  s->frame();

  EXPECT_EQ(s->rec().groupsCreated, groups);
  const auto &bytes =
      s->rec().draws.back().bufferBytes.at(slotOf(*s, "a", "Settings"));
  float first = 0;
  std::memcpy(&first, bytes.data(), sizeof(first));
  EXPECT_EQ(first, 9.0f);
}

TEST(MaterialUniformBlocks, ABlockTheShaderLacksIsIgnored) {
  auto s = makeBindingScene();
  const float v = 1.0f;
  s->assets().createBuffer("other", &v, sizeof(v));
  s->assets()
      .getMaterial(s->assets().getMaterialID("matA"))
      .setUniformBuffer("NotInTheShader", s->assets().getBufferID("other"));

  s->spawn("tri", "matA");
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 1u);
  const int otherId =
      static_cast<fakerhi::FakeBuffer &>(
          s->assets().getBuffer(s->assets().getBufferID("other")))
          .id();
  for (const auto &[slot, id] : s->rec().draws[0].bufferAt) {
    EXPECT_NE(id, otherId) << "binding " << (slot & 0xffffffff);
  }
  EXPECT_TRUE(s->rec().errors.empty());
}

TEST(MaterialStorageBuffers, BoundAtTheBlocksSlotWithItsContents) {
  auto s = makeBindingScene();
  const float positions[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  s->assets().createBuffer("positions", positions, sizeof(positions));
  s->assets()
      .getMaterial(s->assets().getMaterialID("matA"))
      .setStorageBuffer("Positions", s->assets().getBufferID("positions"));

  s->spawn("tri", "matA");
  s->frame();

  const auto &d = s->rec().draws.at(0);
  const uint64_t slot = slotOf(*s, "a", "Positions");
  ASSERT_TRUE(d.bufferBytes.count(slot));
  const auto &bytes = d.bufferBytes.at(slot);
  ASSERT_EQ(bytes.size(), sizeof(positions));
  EXPECT_EQ(std::memcmp(bytes.data(), positions, sizeof(positions)), 0);
}

TEST(MaterialTextures, WorkForProceduralDrawsToo) {
  auto s = makeScene([](SceneOptions &o) {
    o.extras = true;
    o.instanced = false;
  });
  const float one = 0.0f;
  s->assets().createTexture("heights", 1, 1, api::TextureFormat::R32F, &one);
  const HandleID tex = s->assets().getTextureID("heights");
  s->assets()
      .getMaterial(s->assets().getMaterialID("matA"))
      .setTexture("heightMap", tex);

  EntityID e = s->spawn("tri", "matA");
  s->world->addComponents(e, ProceduralInstancesComponent{.count = 100});
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 1u);
  EXPECT_EQ(s->rec().draws[0].instanceCount, 100u);
  EXPECT_EQ(s->rec().draws[0].imageAt.at(slotOf(*s, "a", "heightMap")),
            int(tex));
}

// --- Material parameters -----------------------------------------------------

namespace {

// A shader whose material block (set 1, binding 0) holds a float, an int, a
// vec3 and a mat4.
std::unique_ptr<Scene> makeParameterScene() {
  auto s = makeScene();
  s->assets().compileShader(
      "params", testVertexSource({}),
      "#version 460\n"
      "layout(location = 0) in vec2 vUV;\n"
      "layout(location = 1) in vec4 vColor;\n"
      "layout(location = 2) flat in float vFade;\n"
      "layout(std140, BIND(1, 0)) uniform Material {\n"
      "  float shininess;\n  int mode;\n  vec3 tint;\n  mat4 extra;\n};\n"
      "layout(BIND(1, 1)) uniform sampler2D albedoMap;\n"
      "layout(location = 0) out vec4 outColor;\n"
      "void main() {\n"
      "  outColor = texture(albedoMap, vUV) * shininess * float(mode) +\n"
      "             vec4(tint, 1.0) + extra[0];\n}\n");
  s->assets().registerMaterial<DiffuseMaterial>("matParams", "params", "t1");
  return s;
}

float readFloat(const std::vector<unsigned char> &b, uint32_t offset) {
  float v;
  std::memcpy(&v, b.data() + offset, sizeof(v));
  return v;
}

} // namespace

TEST(MaterialParameters, LandAtTheOffsetsTheShaderDeclares) {
  auto s = makeParameterScene();
  auto &mat = s->assets().getMaterial(s->assets().getMaterialID("matParams"));
  mat.setUniform("shininess", 0.5f);
  mat.setUniform("mode", 3);
  mat.setUniform("tint", Vec3(0.1f, 0.2f, 0.3f));
  Mat4 m(1.0f);
  m[0][0] = 7.0f;
  m[3][3] = 9.0f;
  mat.setUniform("extra", m);

  s->spawn("tri", "matParams");
  s->frame();

  ASSERT_EQ(s->rec().draws.size(), 1u);
  const auto &bytes =
      s->rec().draws[0].bufferBytes.at(fakerhi::bindingKey(1, 0));
  const shader::ReflectedResource *block =
      s->assets().getShader(s->assets().getShaderID("params")).materialBlock();
  ASSERT_NE(block, nullptr);
  EXPECT_EQ(readFloat(bytes, block->member("shininess")->offset), 0.5f);
  int mode = 0;
  std::memcpy(&mode, bytes.data() + block->member("mode")->offset,
              sizeof(mode));
  EXPECT_EQ(mode, 3);
  const uint32_t tint = block->member("tint")->offset;
  EXPECT_EQ(readFloat(bytes, tint), 0.1f);
  EXPECT_EQ(readFloat(bytes, tint + 8), 0.3f);
  const uint32_t extra = block->member("extra")->offset;
  EXPECT_EQ(readFloat(bytes, extra), 7.0f);
  EXPECT_EQ(readFloat(bytes, extra + 15 * 4), 9.0f);
}

TEST(MaterialParameters, ValuesSetBeforeTheShaderIsBoundAreApplied) {
  // A material made by hand starts with no block; setUniform keeps the value
  // until bindShader gives it one.
  auto s = makeParameterScene();
  const Shader &shader =
      s->assets().getShader(s->assets().getShaderID("params"));
  DiffuseMaterial mat(s->assets().getShaderID("params"),
                      s->assets().getTextureID("t1"));
  mat.setUniform("shininess", 0.75f);
  EXPECT_FALSE(mat.params().valid());
  mat.bindShader(shader, s->world->getRenderDevice());
  ASSERT_TRUE(mat.params().valid());
  EXPECT_EQ(readFloat(mat.params().bytes(),
                      shader.materialBlock()->member("shininess")->offset),
            0.75f);
}

TEST(MaterialParameters, AnUnknownNameOrAWrongTypeIsIgnored) {
  auto s = makeParameterScene();
  auto &mat = s->assets().getMaterial(s->assets().getMaterialID("matParams"));
  const auto before = mat.params().bytes();
  mat.setUniform("nope", 1.0f);            // no such member
  mat.setUniform("shininess", Vec3(1.0f)); // a float member given a vec3
  mat.setUniform("mode", 1.0f);            // an int member given a float
  EXPECT_EQ(mat.params().bytes(), before);
}

TEST(MaterialParameters, OnlyAChangedValueIsWrittenAgain) {
  auto s = makeParameterScene();
  auto &mat = s->assets().getMaterial(s->assets().getMaterialID("matParams"));
  mat.setUniform("shininess", 0.5f);
  s->spawn("tri", "matParams");
  s->frame();
  const auto before =
      s->rec().draws.back().bufferBytes.at(fakerhi::bindingKey(1, 0));

  mat.setUniform("shininess", 0.5f); // unchanged
  s->frame();
  EXPECT_EQ(s->rec().draws.back().bufferBytes.at(fakerhi::bindingKey(1, 0)),
            before);

  mat.setUniform("shininess", 0.25f);
  s->frame();
  EXPECT_NE(s->rec().draws.back().bufferBytes.at(fakerhi::bindingKey(1, 0)),
            before);
}

TEST(MaterialParameters, PbrTogglesDefaultOffForPlainMaterialsAndOnForPbr) {
  auto s = makeScene();
  s->assets().compileShader(
      "toggles", testVertexSource({}),
      "#version 460\n"
      "layout(location = 0) in vec2 vUV;\n"
      "layout(location = 1) in vec4 vColor;\n"
      "layout(location = 2) flat in float vFade;\n"
      "layout(std140, BIND(1, 0)) uniform Material {\n"
      "  int useTextures;\n  int useNormalMap;\n};\n"
      "layout(BIND(1, 1)) uniform sampler2D albedoMap;\n"
      "layout(location = 0) out vec4 outColor;\n"
      "void main() { outColor = texture(albedoMap, vUV) * float(useTextures + "
      "useNormalMap); }\n");
  s->assets().registerMaterial<DiffuseMaterial>("plain", "toggles", "t1");
  s->assets().registerPbrMaterial<PbrMaterial>("pbr", "toggles", "t1");

  const Shader &shader =
      s->assets().getShader(s->assets().getShaderID("toggles"));
  const uint32_t at = shader.materialBlock()->member("useTextures")->offset;
  auto intAt = [&](const char *material) {
    const auto &p =
        s->assets().getMaterial(s->assets().getMaterialID(material)).params();
    int v;
    std::memcpy(&v, p.bytes().data() + at, sizeof(v));
    return v;
  };
  EXPECT_EQ(intAt("plain"), 0);
  EXPECT_EQ(intAt("pbr"), 1);
}
