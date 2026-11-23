#include "fake_device.hpp"

#include "gtest/gtest.h"

using namespace componeng::renderer;
using namespace fakerhi;

namespace {

// Two sets as the renderer will use them: set 0 holds one uniform buffer, set 1
// a uniform buffer and a sampled image. Instance data is vertex slot 1.
struct Fixture {
  FakeDevice device;
  std::unique_ptr<rhi::IBindGroupLayout> frameLayout;
  std::unique_ptr<rhi::IBindGroupLayout> materialLayout;
  std::unique_ptr<rhi::IShaderModule> vs;
  std::unique_ptr<rhi::IShaderModule> fs;
  std::unique_ptr<rhi::IGraphicsPipeline> pipeline;
  std::unique_ptr<rhi::IBuffer> frameBuffer;
  std::unique_ptr<rhi::IBuffer> materialBuffer;
  std::unique_ptr<rhi::IBuffer> mesh;
  std::unique_ptr<rhi::IBuffer> indices;
  std::unique_ptr<rhi::IBuffer> instances;
  std::unique_ptr<rhi::IImage> albedo;
  std::unique_ptr<rhi::ISampler> sampler;
  std::unique_ptr<rhi::IBindGroup> frameGroup;
  std::unique_ptr<rhi::IBindGroup> materialGroup;

  Fixture() {
    frameLayout = device.createBindGroupLayout(
        {{{0, rhi::BindingType::UniformBuffer, rhi::StageFlags::AllGraphics}}});
    materialLayout = device.createBindGroupLayout(
        {{{0, rhi::BindingType::UniformBuffer, rhi::StageFlags::AllGraphics},
          {1, rhi::BindingType::CombinedImageSampler,
           rhi::StageFlags::AllGraphics}}});
    const uint32_t words[] = {0x07230203};
    rhi::ShaderModuleDesc md;
    md.code = words;
    md.wordCount = 1;
    vs = device.createShaderModule(md);
    md.stage = rhi::ShaderStage::Fragment;
    fs = device.createShaderModule(md);

    rhi::GraphicsPipelineDesc pd;
    pd.vertex = vs.get();
    pd.fragment = fs.get();
    pd.vertexInput.bindings = {{0, 32, rhi::VertexInputRate::PerVertex},
                               {1, 16, rhi::VertexInputRate::PerInstance}};
    pd.vertexInput.attributes = {{0, 0, rhi::Format::RGB32F, 0},
                                 {3, 1, rhi::Format::RGBA32F, 0}};
    pd.setLayouts = {frameLayout.get(), materialLayout.get()};
    pipeline = device.createGraphicsPipeline(pd);

    frameBuffer = device.createBuffer({64, rhi::BufferUsage::Uniform});
    materialBuffer = device.createBuffer({16, rhi::BufferUsage::Uniform});
    mesh = device.createBuffer(
        {96, rhi::BufferUsage::Vertex, rhi::MemoryKind::GpuOnly});
    indices = device.createBuffer(
        {12, rhi::BufferUsage::Index, rhi::MemoryKind::GpuOnly});
    instances = device.createBuffer({64, rhi::BufferUsage::Vertex});
    rhi::ImageDesc id;
    id.extent = {4, 4};
    id.usage = rhi::ImageUsage::Sampled | rhi::ImageUsage::TransferDst;
    albedo = device.createImage(id);
    sampler = device.createSampler({});

    rhi::BindGroupDesc fg;
    fg.layout = frameLayout.get();
    fg.entries = {{0, frameBuffer.get()}};
    frameGroup = device.createBindGroup(fg);
    rhi::BindGroupDesc mg;
    mg.layout = materialLayout.get();
    rhi::BindGroupEntry image;
    image.binding = 1;
    image.image = albedo.get();
    image.sampler = sampler.get();
    mg.entries = {{0, materialBuffer.get()}, image};
    materialGroup = device.createBindGroup(mg);
  }

  void beginRendering(rhi::IImage *target = nullptr) {
    device.beginFrame();
    rhi::RenderingDesc r;
    r.color.image = target;
    r.hasDepth = false;
    device.beginRendering(r);
  }

  void bindAll() {
    device.bindPipeline(*pipeline);
    device.bindGroup(0, *frameGroup);
    device.bindGroup(1, *materialGroup);
    device.bindVertexBuffer(0, *mesh, 0);
    device.bindVertexBuffer(1, *instances, 0);
    device.bindIndexBuffer(*indices, 0, rhi::IndexType::U32);
  }

  void finish() {
    device.endRendering();
    device.present(nullptr);
  }
};

} // namespace

TEST(FakeDevice, RecordsAnIndexedDrawWithItsBindings) {
  Fixture f;
  const float frame[4] = {1, 2, 3, 4};
  f.frameBuffer->write(0, frame, sizeof(frame));
  const float material[1] = {0.5f};
  f.materialBuffer->write(0, material, sizeof(material));

  f.beginRendering();
  f.bindAll();
  f.device.drawIndexed(3, 2, 0, 0, 0);
  f.finish();

  EXPECT_TRUE(f.device.rec.errors.empty());
  ASSERT_EQ(f.device.rec.draws.size(), 1u);
  const Draw &d = f.device.rec.draws[0];
  EXPECT_TRUE(d.indexed);
  EXPECT_EQ(d.indexCount, 3u);
  EXPECT_EQ(d.instanceCount, 2u);
  EXPECT_EQ(d.targetId, 0);
  EXPECT_EQ(d.pipelineId, 1);
  EXPECT_EQ(d.state, rhi::RenderState{});
  EXPECT_EQ(d.imageAt.at(bindingKey(1, 1)), 1);
  EXPECT_EQ(d.bufferAt.at(bindingKey(0, 0)), 1);
  ASSERT_EQ(d.bufferBytes.at(bindingKey(0, 0)).size(), 64u);
  float seen = 0;
  std::memcpy(&seen, d.bufferBytes.at(bindingKey(0, 0)).data() + 8, 4);
  EXPECT_EQ(seen, 3.0f);
  EXPECT_EQ(f.device.rec.frames, 1);
  EXPECT_EQ(f.device.rec.presents, 1);
}

TEST(FakeDevice, InstanceFloatsFollowTheBoundOffsetAndFirstInstance) {
  Fixture f;
  float data[16];
  for (int i = 0; i < 16; ++i) {
    data[i] = static_cast<float>(i);
  }
  f.instances->write(0, data, sizeof(data));

  f.beginRendering();
  f.bindAll();
  f.device.drawIndexed(3, 2, 0, 0, 1);
  // The same data bound 16 bytes in, drawn from instance 0.
  f.device.bindVertexBuffer(1, *f.instances, 16);
  f.device.drawIndexed(3, 2, 0, 0, 0);
  f.finish();

  EXPECT_TRUE(f.device.rec.errors.empty());
  ASSERT_EQ(f.device.rec.draws.size(), 2u);
  const std::vector<float> want = {4, 5, 6, 7, 8, 9, 10, 11};
  EXPECT_EQ(f.device.rec.draws[0].instanceFloats, want);
  EXPECT_EQ(f.device.rec.draws[1].instanceFloats, want);
}

TEST(FakeDevice, RecordsTheTargetAndClears) {
  Fixture f;
  rhi::ImageDesc id;
  id.extent = {8, 8};
  id.usage = rhi::ImageUsage::ColorAttachment | rhi::ImageUsage::Sampled;
  id.flags = rhi::ImageFlags::PerFrame;
  auto target = f.device.createImage(id);

  f.beginRendering(target.get());
  f.bindAll();
  f.device.drawIndexed(3, 1, 0, 0, 0);
  f.device.endRendering();
  rhi::RenderingDesc window;
  window.hasDepth = false;
  f.device.beginRendering(window);
  f.bindAll();
  f.device.drawIndexed(3, 1, 0, 0, 0);
  f.finish();

  EXPECT_TRUE(f.device.rec.errors.empty());
  EXPECT_EQ(f.device.rec.clears, 2);
  EXPECT_EQ(f.device.rec.targetBinds, (std::vector<int>{2, 0}));
  EXPECT_EQ(f.device.rec.draws[0].targetId, 2);
  EXPECT_EQ(f.device.rec.draws[1].targetId, 0);
}

TEST(FakeDevice, FullscreenDrawNeedsNoVertexBuffers) {
  FakeDevice device;
  auto layout = device.createBindGroupLayout({});
  const uint32_t words[] = {0x07230203};
  rhi::ShaderModuleDesc md;
  md.code = words;
  md.wordCount = 1;
  auto vs = device.createShaderModule(md);
  auto fs = device.createShaderModule(md);
  rhi::GraphicsPipelineDesc pd;
  pd.vertex = vs.get();
  pd.fragment = fs.get();
  pd.setLayouts = {layout.get()};
  auto pipeline = device.createGraphicsPipeline(pd);
  rhi::BindGroupDesc gd;
  gd.layout = layout.get();
  auto group = device.createBindGroup(gd);

  device.beginFrame();
  rhi::RenderingDesc r;
  r.hasDepth = false;
  device.beginRendering(r);
  device.bindPipeline(*pipeline);
  device.bindGroup(0, *group);
  device.draw(3, 1, 0, 0);
  device.endRendering();
  device.present(nullptr);

  EXPECT_TRUE(device.rec.errors.empty());
  EXPECT_TRUE(device.rec.draws.empty());
  ASSERT_EQ(device.rec.fullscreenDraws.size(), 1u);
  EXPECT_FALSE(device.rec.fullscreenDraws[0].indexed);
  EXPECT_EQ(device.rec.fullscreenDraws[0].vertexCount, 3u);
  EXPECT_EQ(device.rec.drawSequence, std::vector<std::string>{"fullscreen"});
}

TEST(FakeDevice, BindingsDoNotCarryIntoTheNextRendering) {
  Fixture f;
  f.beginRendering();
  f.bindAll();
  f.device.endRendering();
  rhi::RenderingDesc r;
  r.hasDepth = false;
  f.device.beginRendering(r);
  f.device.drawIndexed(3, 1, 0, 0, 0);
  f.finish();
  EXPECT_FALSE(f.device.rec.errors.empty());
}

TEST(FakeDevice, ReportsMisuse) {
  {
    Fixture f;
    ASSERT_TRUE(f.device.rec.errors.empty());
    f.device.beginFrame();
    f.device.drawIndexed(3, 1, 0, 0, 0);
    EXPECT_FALSE(f.device.rec.errors.empty()); // outside a rendering
  }
  {
    Fixture f;
    ASSERT_TRUE(f.device.rec.errors.empty());
    f.beginRendering();
    f.device.bindPipeline(*f.pipeline);
    f.device.bindGroup(0, *f.frameGroup);
    f.device.bindGroup(1, *f.materialGroup);
    f.device.drawIndexed(3, 1, 0, 0, 0);
    EXPECT_FALSE(f.device.rec.errors.empty()); // no vertex buffers, no indices
  }
  {
    Fixture f;
    ASSERT_TRUE(f.device.rec.errors.empty());
    f.beginRendering();
    f.device.bindPipeline(*f.pipeline);
    f.device.bindGroup(1, *f.frameGroup);
    EXPECT_FALSE(f.device.rec.errors.empty()); // wrong layout for the set
  }
  {
    Fixture f;
    ASSERT_TRUE(f.device.rec.errors.empty());
    f.device.createBindGroup(
        {f.materialLayout.get(), {{0, f.materialBuffer.get()}}});
    EXPECT_FALSE(f.device.rec.errors.empty()); // binding 1 left empty
  }
  {
    Fixture f;
    ASSERT_TRUE(f.device.rec.errors.empty());
    f.mesh->write(0, "x", 1);
    EXPECT_FALSE(f.device.rec.errors.empty()); // GpuOnly memory is not written
  }
  {
    Fixture f;
    ASSERT_TRUE(f.device.rec.errors.empty());
    rhi::ImageDesc id;
    id.usage = rhi::ImageUsage::Sampled;
    auto notTarget = f.device.createImage(id);
    f.beginRendering(notTarget.get());
    EXPECT_FALSE(f.device.rec.errors.empty());
  }
}

TEST(FakeDevice, UploadsFillGpuOnlyBuffersAndRecordImageRegions) {
  Fixture f;
  const uint32_t idx[3] = {0, 1, 2};
  f.device.uploadBuffer(*f.indices, 0, idx, sizeof(idx));
  EXPECT_EQ(static_cast<FakeBuffer &>(*f.indices).bytes()[4], 1);

  const unsigned char pixels[4 * 4 * 4] = {7};
  f.device.uploadImage(*f.albedo, {0, 0, 4, 4}, pixels);
  ASSERT_EQ(f.device.rec.imageUploads.size(), 1u);
  EXPECT_EQ(f.device.rec.imageUploads[0].pixels.size(), sizeof(pixels));
  EXPECT_EQ(f.device.rec.imageUploads[0].pixels[0], 7);
  EXPECT_TRUE(f.device.rec.errors.empty());
}

TEST(FakeDevice, GpuPassEventsInterleaveWithDraws) {
  Fixture f;
  f.beginRendering();
  f.bindAll();
  f.device.beginGpuPass(api::PassId::Opaque);
  f.device.drawIndexed(3, 1, 0, 0, 0);
  f.device.endGpuPass(api::PassId::Opaque);
  f.finish();
  EXPECT_EQ(
      f.device.rec.gpuPassEvents,
      (std::vector<std::string>{"begin Opaque", "indexed", "end Opaque"}));
}

TEST(FakeDevice, SkippedFrameRecordsNothing) {
  FakeDevice device;
  device.rec.beginFrameResult = false;
  EXPECT_FALSE(device.beginFrame());
  EXPECT_EQ(device.rec.frames, 0);
  EXPECT_TRUE(device.rec.commands.empty());
}
