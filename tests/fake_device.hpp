#pragma once

#include "componeng/renderer/backend/api/rhi/device.hpp"

#include <cstring>
#include <map>
#include <string>
#include <vector>

// A device that draws nothing and records the command stream it was given, so
// the renderer can be tested without a GPU. It also checks the call order the
// real backends rely on and reports a misuse in Recorder::errors.
namespace fakerhi {

using namespace componeng::renderer;

constexpr uint64_t bindingKey(uint32_t set, uint32_t binding) {
  return (static_cast<uint64_t>(set) << 32) | binding;
}

inline uint32_t bytesPerPixel(rhi::Format f) {
  switch (f) {
    case rhi::Format::R8:
      return 1;
    case rhi::Format::RG8:
    case rhi::Format::R16F:
      return 2;
    case rhi::Format::RGBA8:
    case rhi::Format::RGBA8Srgb:
    case rhi::Format::BGRA8:
    case rhi::Format::RG16F:
    case rhi::Format::R32F:
    case rhi::Format::D32F:
      return 4;
    case rhi::Format::RGBA16F:
    case rhi::Format::RG32F:
      return 8;
    case rhi::Format::RGB32F:
      return 12;
    case rhi::Format::RGBA32F:
      return 16;
    case rhi::Format::Undefined:
      return 0;
  }
  return 0;
}

// One draw call with the state it was issued under.
struct Draw {
  bool indexed = false;
  uint32_t vertexCount = 0; // draw()
  uint32_t indexCount = 0;  // drawIndexed()
  uint32_t instanceCount = 0;
  uint32_t firstVertex = 0;
  uint32_t firstIndex = 0;
  int32_t vertexOffset = 0;
  uint32_t firstInstance = 0;
  rhi::RenderState state; // of the bound pipeline
  int pipelineId = 0;
  int vertexModuleId = 0;
  int fragmentModuleId = 0;
  // Which shader the draw's vertex module belongs to, counting vertex modules
  // from 1 in creation order (a Shader makes exactly one, and its depth
  // variants reuse it). A variant differs only by fragmentModuleId.
  int shaderId = 0;
  // The image at set 1, binding 1: where the shaders name their albedoMap.
  int albedoTextureId = 0;
  int targetId = 0;                 // colour image drawn into (0: the window)
  std::map<uint32_t, int> groupIds; // set -> group
  std::map<uint64_t, int> imageAt;  // bindingKey
  std::map<uint64_t, int> bufferAt; // bindingKey
  std::map<uint64_t, std::vector<unsigned char>> bufferBytes; // at draw time
  std::map<uint32_t, int> vertexBufferIds;                    // slot -> buffer
  // The floats of vertex slot 1 for the drawn instances (firstInstance on),
  // as the per-instance binding of the pipeline strides them.
  std::vector<float> instanceFloats;
};

struct Command {
  enum Kind {
    BeginFrame,
    Present,
    BeginRendering,
    EndRendering,
    BindPipeline,
    BindGroup,
    BindVertexBuffer,
    BindIndexBuffer,
    Draw,
    DrawIndexed,
    DrawIndexedIndirect,
    BindComputePipeline,
    Dispatch,
    Barrier,
    CopyBuffer,
    SetViewport,
    SetScissor,
  };
  Kind kind;
  int id = 0; // the object the command names, where there is one
};

struct CreatedTarget {
  int id = 0;
  uint32_t width = 0, height = 0;
  int colorTextureId = 0; // the image the target's colour is sampled as
};

struct ImageUpload {
  int imageId = 0;
  rhi::Rect region;
  std::vector<unsigned char> pixels;
};

struct CreatedImage {
  int id = 0;
  rhi::ImageDesc desc;
};

struct Recorder {
  int buffersCreated = 0;
  int imagesCreated = 0;
  int samplersCreated = 0;
  int modulesCreated = 0;
  int layoutsCreated = 0;
  int groupsCreated = 0;
  int pipelinesCreated = 0;
  int computePipelinesCreated = 0;

  int frames = 0;
  int presents = 0;
  int clears = 0; // beginRendering calls that cleared the colour attachment
  bool frameOpen = false;
  bool beginFrameResult = true;
  rhi::Backend backend = rhi::Backend::Vulkan;
  // What windowViewportExtent() reports; tests change it to resize the window.
  rhi::Extent2D viewport{1280, 720};

  std::vector<Command> commands;
  std::vector<Draw> draws;
  std::vector<Draw> fullscreenDraws; // draw() calls, which have no mesh
  std::vector<int> pipelineBinds;    // pipeline id per BindPipeline
  std::vector<int> groupBinds;       // group id per bindGroup, any set
  // The colour attachments (render targets) created, in creation order, and
  // the target each clearing beginRendering hit (0: the window).
  std::vector<CreatedTarget> createdTargets;
  std::vector<int> clearTargetIds;
  int currentTarget = 0;
  std::vector<rhi::SamplerDesc> createdSamplers;
  std::map<int, int> vertexOrdinal;      // vertex module id -> shader id
  std::vector<std::string> drawSequence; // "indexed" / "fullscreen"
  // beginGpuPass / endGpuPass as "begin <name>" / "end <name>", with each draw
  // between them.
  std::vector<std::string> gpuPassEvents;
  std::vector<int> targetBinds; // colour image id per beginRendering
  std::vector<CreatedImage> createdImages;
  std::vector<ImageUpload> imageUploads;
  std::vector<std::string> errors;

  void error(const std::string &message) {
    errors.push_back(message);
  }
};

class FakeBuffer final : public rhi::IBuffer {
public:
  FakeBuffer(Recorder &rec, int id, const rhi::BufferDesc &desc)
      : m_rec(rec), m_id(id), m_desc(desc), m_bytes(desc.size, 0) {
  }
  int id() const {
    return m_id;
  }
  uint64_t size() const override {
    return m_desc.size;
  }
  void write(uint64_t offset, const void *data, uint64_t bytes) override {
    if (m_desc.memory != rhi::MemoryKind::Upload) {
      m_rec.error("write to a buffer that is not Upload memory");
      return;
    }
    if (offset + bytes > m_bytes.size()) {
      m_rec.error("buffer write past its end");
      return;
    }
    std::memcpy(m_bytes.data() + offset, data, bytes);
  }
  void fill(uint64_t offset, const void *data, uint64_t bytes) {
    if (offset + bytes > m_bytes.size()) {
      m_rec.error("buffer upload past its end");
      return;
    }
    std::memcpy(m_bytes.data() + offset, data, bytes);
  }
  const rhi::BufferDesc &desc() const {
    return m_desc;
  }
  const std::vector<unsigned char> &bytes() const {
    return m_bytes;
  }

private:
  Recorder &m_rec;
  int m_id;
  rhi::BufferDesc m_desc;
  std::vector<unsigned char> m_bytes;
};

class FakeImage final : public rhi::IImage {
public:
  FakeImage(int id, const rhi::ImageDesc &desc) : m_id(id), m_desc(desc) {
  }
  int id() const {
    return m_id;
  }
  rhi::Extent2D extent() const override {
    return m_desc.extent;
  }
  rhi::Format format() const override {
    return m_desc.format;
  }
  uint32_t mipLevels() const override {
    return m_desc.mipLevels;
  }
  const rhi::ImageDesc &desc() const {
    return m_desc;
  }

private:
  int m_id;
  rhi::ImageDesc m_desc;
};

class FakeSampler final : public rhi::ISampler {
public:
  FakeSampler(int id, const rhi::SamplerDesc &desc) : m_id(id), m_desc(desc) {
  }
  int id() const {
    return m_id;
  }
  const rhi::SamplerDesc &desc() const {
    return m_desc;
  }

private:
  int m_id;
  rhi::SamplerDesc m_desc;
};

class FakeShaderModule final : public rhi::IShaderModule {
public:
  FakeShaderModule(int id, const rhi::ShaderModuleDesc &desc)
      : m_id(id), m_stage(desc.stage),
        m_words(desc.code, desc.code + desc.wordCount) {
  }
  int id() const {
    return m_id;
  }
  rhi::ShaderStage stage() const {
    return m_stage;
  }
  const std::vector<uint32_t> &words() const {
    return m_words;
  }

private:
  int m_id;
  rhi::ShaderStage m_stage;
  std::vector<uint32_t> m_words;
};

class FakeBindGroupLayout final : public rhi::IBindGroupLayout {
public:
  FakeBindGroupLayout(int id, const rhi::BindGroupLayoutDesc &desc)
      : m_id(id), m_desc(desc) {
  }
  int id() const {
    return m_id;
  }
  const rhi::BindGroupLayoutDesc &desc() const {
    return m_desc;
  }
  const rhi::BindingDesc *binding(uint32_t b) const {
    for (const rhi::BindingDesc &d : m_desc.bindings) {
      if (d.binding == b) {
        return &d;
      }
    }
    return nullptr;
  }

private:
  int m_id;
  rhi::BindGroupLayoutDesc m_desc;
};

class FakeBindGroup final : public rhi::IBindGroup {
public:
  struct Entry {
    uint32_t binding = 0;
    FakeBuffer *buffer = nullptr;
    int imageId = 0;
    int samplerId = 0;
    uint64_t offset = 0;
    uint64_t range = 0;
  };

  FakeBindGroup(int id, int layoutId, std::vector<Entry> entries)
      : m_id(id), m_layoutId(layoutId), m_entries(std::move(entries)) {
  }
  int id() const {
    return m_id;
  }
  int layoutId() const {
    return m_layoutId;
  }
  const std::vector<Entry> &entries() const {
    return m_entries;
  }

private:
  int m_id;
  int m_layoutId;
  std::vector<Entry> m_entries;
};

class FakeGraphicsPipeline final : public rhi::IGraphicsPipeline {
public:
  FakeGraphicsPipeline(int id, const rhi::GraphicsPipelineDesc &desc,
                       int vertexId, int fragmentId, std::vector<int> layoutIds)
      : m_id(id), m_vertexId(vertexId), m_fragmentId(fragmentId),
        m_layoutIds(std::move(layoutIds)), m_state(desc.state),
        m_input(desc.vertexInput), m_colorFormat(desc.colorFormat),
        m_depthFormat(desc.depthFormat) {
  }
  int id() const {
    return m_id;
  }
  int vertexModuleId() const {
    return m_vertexId;
  }
  int fragmentModuleId() const {
    return m_fragmentId;
  }
  const std::vector<int> &layoutIds() const {
    return m_layoutIds;
  }
  const rhi::RenderState &state() const {
    return m_state;
  }
  const rhi::VertexInputDesc &vertexInput() const {
    return m_input;
  }
  rhi::Format colorFormat() const {
    return m_colorFormat;
  }
  rhi::Format depthFormat() const {
    return m_depthFormat;
  }

private:
  int m_id;
  int m_vertexId;
  int m_fragmentId;
  std::vector<int> m_layoutIds;
  rhi::RenderState m_state;
  rhi::VertexInputDesc m_input;
  rhi::Format m_colorFormat;
  rhi::Format m_depthFormat;
};

class FakeComputePipeline final : public rhi::IComputePipeline {
public:
  explicit FakeComputePipeline(int id) : m_id(id) {
  }
  int id() const {
    return m_id;
  }

private:
  int m_id;
};

class FakeDevice final : public rhi::IDevice {
public:
  Recorder rec;

  void init(void *) override {
  }

  rhi::Backend backend() const override {
    return rec.backend;
  }
  rhi::Format backbufferFormat() const override {
    return rhi::Format::BGRA8;
  }
  bool frameOpen() const override {
    return rec.frameOpen;
  }

  bool beginFrame() override {
    if (!rec.beginFrameResult) {
      return false;
    }
    if (rec.frameOpen) {
      rec.error("beginFrame while a frame is open");
    }
    rec.frameOpen = true;
    ++rec.frames;
    record(Command::BeginFrame);
    return true;
  }
  void present(void *) override {
    if (!rec.frameOpen) {
      rec.error("present without beginFrame");
    }
    if (m_rendering) {
      rec.error("present inside a rendering");
    }
    rec.frameOpen = false;
    ++rec.presents;
    record(Command::Present);
  }

  void setWindowViewport(int, int, int w, int h) override {
    rec.viewport = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
  }
  rhi::Extent2D windowViewportExtent() const override {
    return rec.viewport;
  }

  std::unique_ptr<rhi::IBuffer>
  createBuffer(const rhi::BufferDesc &d) override {
    return std::make_unique<FakeBuffer>(rec, ++rec.buffersCreated, d);
  }
  std::unique_ptr<rhi::IImage> createImage(const rhi::ImageDesc &d) override {
    const int id = ++rec.imagesCreated;
    rec.createdImages.push_back({id, d});
    if (hasFlag(d.usage, rhi::ImageUsage::ColorAttachment)) {
      rec.createdTargets.push_back({id, d.extent.width, d.extent.height, id});
    }
    return std::make_unique<FakeImage>(id, d);
  }
  std::unique_ptr<rhi::ISampler>
  createSampler(const rhi::SamplerDesc &d) override {
    rec.createdSamplers.push_back(d);
    return std::make_unique<FakeSampler>(++rec.samplersCreated, d);
  }
  std::unique_ptr<rhi::IShaderModule>
  createShaderModule(const rhi::ShaderModuleDesc &d) override {
    const int id = ++rec.modulesCreated;
    if (d.stage == rhi::ShaderStage::Vertex) {
      const int ordinal = static_cast<int>(rec.vertexOrdinal.size()) + 1;
      rec.vertexOrdinal[id] = ordinal;
    }
    return std::make_unique<FakeShaderModule>(id, d);
  }
  std::unique_ptr<rhi::IBindGroupLayout>
  createBindGroupLayout(const rhi::BindGroupLayoutDesc &d) override {
    return std::make_unique<FakeBindGroupLayout>(++rec.layoutsCreated, d);
  }
  std::unique_ptr<rhi::IBindGroup>
  createBindGroup(const rhi::BindGroupDesc &d) override {
    auto *layout = static_cast<FakeBindGroupLayout *>(d.layout);
    if (!layout) {
      rec.error("bind group without a layout");
      return std::make_unique<FakeBindGroup>(
          ++rec.groupsCreated, 0, std::vector<FakeBindGroup::Entry>{});
    }
    std::vector<FakeBindGroup::Entry> entries;
    for (const rhi::BindGroupEntry &e : d.entries) {
      const rhi::BindingDesc *slot = layout->binding(e.binding);
      if (!slot) {
        rec.error("bind group entry for a binding the layout lacks");
        continue;
      }
      const bool isBuffer = slot->type == rhi::BindingType::UniformBuffer ||
                            slot->type == rhi::BindingType::StorageBuffer;
      if (isBuffer != (e.buffer != nullptr)) {
        rec.error("bind group entry does not match its binding type");
        continue;
      }
      FakeBindGroup::Entry entry;
      entry.binding = e.binding;
      entry.buffer = static_cast<FakeBuffer *>(e.buffer);
      entry.offset = e.offset;
      entry.range = e.range;
      if (e.image) {
        entry.imageId = static_cast<FakeImage *>(e.image)->id();
      }
      if (e.sampler) {
        entry.samplerId = static_cast<FakeSampler *>(e.sampler)->id();
      }
      entries.push_back(entry);
    }
    for (const rhi::BindingDesc &b : layout->desc().bindings) {
      bool found = false;
      for (const FakeBindGroup::Entry &e : entries) {
        found |= e.binding == b.binding;
      }
      if (!found) {
        rec.error("bind group leaves binding " + std::to_string(b.binding) +
                  " empty");
      }
    }
    return std::make_unique<FakeBindGroup>(++rec.groupsCreated, layout->id(),
                                           std::move(entries));
  }
  std::unique_ptr<rhi::IGraphicsPipeline>
  createGraphicsPipeline(const rhi::GraphicsPipelineDesc &d) override {
    if (!d.vertex || !d.fragment) {
      rec.error("graphics pipeline needs a vertex and a fragment module");
    }
    std::vector<int> layoutIds;
    for (rhi::IBindGroupLayout *l : d.setLayouts) {
      layoutIds.push_back(static_cast<FakeBindGroupLayout *>(l)->id());
    }
    return std::make_unique<FakeGraphicsPipeline>(
        ++rec.pipelinesCreated, d,
        d.vertex ? static_cast<FakeShaderModule *>(d.vertex)->id() : 0,
        d.fragment ? static_cast<FakeShaderModule *>(d.fragment)->id() : 0,
        std::move(layoutIds));
  }
  std::unique_ptr<rhi::IComputePipeline>
  createComputePipeline(const rhi::ComputePipelineDesc &) override {
    return std::make_unique<FakeComputePipeline>(++rec.computePipelinesCreated);
  }

  void uploadBuffer(rhi::IBuffer &dst, uint64_t offset, const void *data,
                    uint64_t bytes) override {
    static_cast<FakeBuffer &>(dst).fill(offset, data, bytes);
  }
  void uploadImage(rhi::IImage &dst, const rhi::Rect &region,
                   const void *pixels) override {
    auto &image = static_cast<FakeImage &>(dst);
    const size_t n = static_cast<size_t>(region.width) * region.height *
                     bytesPerPixel(image.format());
    ImageUpload u;
    u.imageId = image.id();
    u.region = region;
    u.pixels.assign(static_cast<const unsigned char *>(pixels),
                    static_cast<const unsigned char *>(pixels) + n);
    rec.imageUploads.push_back(std::move(u));
  }

  void beginRendering(const rhi::RenderingDesc &d) override {
    requireFrame("beginRendering");
    if (m_rendering) {
      rec.error("beginRendering inside a rendering");
    }
    m_rendering = true;
    m_target =
        d.color.image ? static_cast<FakeImage *>(d.color.image)->id() : 0;
    if (d.color.image &&
        !hasFlag(static_cast<FakeImage *>(d.color.image)->desc().usage,
                 rhi::ImageUsage::ColorAttachment)) {
      rec.error("colour attachment image lacks ColorAttachment usage");
    }
    if (d.hasDepth && d.depth.image &&
        !hasFlag(static_cast<FakeImage *>(d.depth.image)->desc().usage,
                 rhi::ImageUsage::DepthAttachment)) {
      rec.error("depth attachment image lacks DepthAttachment usage");
    }
    if (d.color.load == rhi::LoadOp::Clear) {
      ++rec.clears;
      rec.clearTargetIds.push_back(m_target);
    }
    rec.currentTarget = m_target;
    rec.targetBinds.push_back(m_target);
    resetBindings();
    record(Command::BeginRendering, m_target);
  }
  void endRendering() override {
    if (!m_rendering) {
      rec.error("endRendering outside a rendering");
    }
    m_rendering = false;
    rec.currentTarget = 0;
    record(Command::EndRendering);
  }
  void setViewport(const rhi::Rect &) override {
    requireRendering("setViewport");
    record(Command::SetViewport);
  }
  void setScissor(const rhi::Rect &) override {
    requireRendering("setScissor");
    record(Command::SetScissor);
  }

  void bindPipeline(rhi::IGraphicsPipeline &p) override {
    requireRendering("bindPipeline");
    m_pipeline = static_cast<FakeGraphicsPipeline *>(&p);
    rec.pipelineBinds.push_back(m_pipeline->id());
    record(Command::BindPipeline, m_pipeline->id());
  }
  void bindGroup(uint32_t set, rhi::IBindGroup &g) override {
    requireRendering("bindGroup");
    auto *group = static_cast<FakeBindGroup *>(&g);
    if (m_pipeline) {
      if (set >= m_pipeline->layoutIds().size()) {
        rec.error("bindGroup set " + std::to_string(set) +
                  " is beyond the pipeline's layouts");
      } else if (m_pipeline->layoutIds()[set] != group->layoutId()) {
        rec.error("bindGroup set " + std::to_string(set) +
                  " does not match the pipeline's layout");
      }
    }
    m_groups[set] = group;
    rec.groupBinds.push_back(group->id());
    record(Command::BindGroup, group->id());
  }
  void bindVertexBuffer(uint32_t slot, rhi::IBuffer &b,
                        uint64_t byteOffset) override {
    requireRendering("bindVertexBuffer");
    auto *buffer = static_cast<FakeBuffer *>(&b);
    if (!hasFlag(buffer->desc().usage, rhi::BufferUsage::Vertex)) {
      rec.error("vertex buffer lacks Vertex usage");
    }
    m_vertexBuffers[slot] = {buffer, byteOffset};
    record(Command::BindVertexBuffer, buffer->id());
  }
  void bindIndexBuffer(rhi::IBuffer &b, uint64_t, rhi::IndexType) override {
    requireRendering("bindIndexBuffer");
    auto *buffer = static_cast<FakeBuffer *>(&b);
    if (!hasFlag(buffer->desc().usage, rhi::BufferUsage::Index)) {
      rec.error("index buffer lacks Index usage");
    }
    m_indexBuffer = buffer;
    record(Command::BindIndexBuffer, buffer->id());
  }

  void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
            uint32_t firstInstance) override {
    Draw d = snapshot("draw");
    d.vertexCount = vertexCount;
    d.instanceCount = instanceCount;
    d.firstVertex = firstVertex;
    d.firstInstance = firstInstance;
    fillInstanceFloats(d);
    rec.fullscreenDraws.push_back(std::move(d));
    rec.drawSequence.push_back("fullscreen");
    rec.gpuPassEvents.push_back("fullscreen");
    record(Command::Draw);
  }
  void drawIndexed(uint32_t indexCount, uint32_t instanceCount,
                   uint32_t firstIndex, int32_t vertexOffset,
                   uint32_t firstInstance) override {
    Draw d = snapshot("drawIndexed");
    if (!m_indexBuffer) {
      rec.error("drawIndexed without an index buffer");
    }
    d.indexed = true;
    d.indexCount = indexCount;
    d.instanceCount = instanceCount;
    d.firstIndex = firstIndex;
    d.vertexOffset = vertexOffset;
    d.firstInstance = firstInstance;
    fillInstanceFloats(d);
    rec.draws.push_back(std::move(d));
    rec.drawSequence.push_back("indexed");
    rec.gpuPassEvents.push_back("indexed");
    record(Command::DrawIndexed);
  }
  void drawIndexedIndirect(rhi::IBuffer &commands, uint64_t, uint32_t,
                           uint32_t) override {
    snapshot("drawIndexedIndirect");
    if (!hasFlag(static_cast<FakeBuffer &>(commands).desc().usage,
                 rhi::BufferUsage::Indirect)) {
      rec.error("indirect buffer lacks Indirect usage");
    }
    record(Command::DrawIndexedIndirect);
  }

  void bindComputePipeline(rhi::IComputePipeline &p) override {
    record(Command::BindComputePipeline,
           static_cast<FakeComputePipeline &>(p).id());
  }
  void dispatch(uint32_t, uint32_t, uint32_t) override {
    record(Command::Dispatch);
  }
  void memoryBarrier(rhi::Access, rhi::Access) override {
    record(Command::Barrier);
  }
  void copyBuffer(rhi::IBuffer &src, rhi::IBuffer &dst, uint64_t srcOffset,
                  uint64_t dstOffset, uint64_t bytes) override {
    auto &from = static_cast<FakeBuffer &>(src);
    static_cast<FakeBuffer &>(dst).fill(dstOffset,
                                        from.bytes().data() + srcOffset, bytes);
    record(Command::CopyBuffer);
  }

  int checkErrors() const override {
    return static_cast<int>(rec.errors.size());
  }

  void beginGpuPass(api::PassId pass) override {
    rec.gpuPassEvents.push_back(std::string("begin ") + api::passName(pass));
  }
  void endGpuPass(api::PassId pass) override {
    rec.gpuPassEvents.push_back(std::string("end ") + api::passName(pass));
  }

private:
  struct BoundVertexBuffer {
    FakeBuffer *buffer = nullptr;
    uint64_t offset = 0;
  };

  void record(Command::Kind kind, int id = 0) {
    rec.commands.push_back({kind, id});
  }
  void requireFrame(const char *what) {
    if (!rec.frameOpen) {
      rec.error(std::string(what) + " outside a frame");
    }
  }
  void requireRendering(const char *what) {
    if (!m_rendering) {
      rec.error(std::string(what) + " outside a rendering");
    }
  }
  // Nothing bound in one rendering carries into the next, as on the real
  // backends.
  void resetBindings() {
    m_pipeline = nullptr;
    m_groups.clear();
    m_vertexBuffers.clear();
    m_indexBuffer = nullptr;
  }

  Draw snapshot(const char *what) {
    requireRendering(what);
    Draw d;
    d.targetId = m_target;
    if (!m_pipeline) {
      rec.error(std::string(what) + " without a pipeline");
      return d;
    }
    d.state = m_pipeline->state();
    d.pipelineId = m_pipeline->id();
    d.vertexModuleId = m_pipeline->vertexModuleId();
    d.fragmentModuleId = m_pipeline->fragmentModuleId();
    auto ordinal = rec.vertexOrdinal.find(d.vertexModuleId);
    d.shaderId = ordinal != rec.vertexOrdinal.end() ? ordinal->second : 0;
    for (size_t set = 0; set < m_pipeline->layoutIds().size(); ++set) {
      auto it = m_groups.find(static_cast<uint32_t>(set));
      if (it == m_groups.end()) {
        rec.error(std::string(what) + " with set " + std::to_string(set) +
                  " unbound");
        continue;
      }
      const FakeBindGroup *g = it->second;
      d.groupIds[static_cast<uint32_t>(set)] = g->id();
      for (const FakeBindGroup::Entry &e : g->entries()) {
        const uint64_t key = bindingKey(static_cast<uint32_t>(set), e.binding);
        if (e.buffer) {
          d.bufferAt[key] = e.buffer->id();
          d.bufferBytes[key] = e.buffer->bytes();
        } else {
          d.imageAt[key] = e.imageId;
        }
      }
    }
    for (const rhi::VertexBindingDesc &b : m_pipeline->vertexInput().bindings) {
      auto it = m_vertexBuffers.find(b.binding);
      if (it == m_vertexBuffers.end()) {
        rec.error(std::string(what) + " with vertex slot " +
                  std::to_string(b.binding) + " unbound");
        continue;
      }
      d.vertexBufferIds[b.binding] = it->second.buffer->id();
    }
    auto albedo = d.imageAt.find(bindingKey(1, 1));
    d.albedoTextureId = albedo != d.imageAt.end() ? albedo->second : 0;
    return d;
  }

  void fillInstanceFloats(Draw &d) {
    if (!m_pipeline) {
      return;
    }
    for (const rhi::VertexBindingDesc &b : m_pipeline->vertexInput().bindings) {
      if (b.binding != 1) {
        continue;
      }
      auto it = m_vertexBuffers.find(1);
      if (it == m_vertexBuffers.end()) {
        return;
      }
      const std::vector<unsigned char> &bytes = it->second.buffer->bytes();
      const uint64_t begin =
          it->second.offset + static_cast<uint64_t>(d.firstInstance) * b.stride;
      const uint64_t end =
          begin + static_cast<uint64_t>(d.instanceCount) * b.stride;
      if (end > bytes.size()) {
        rec.error("instance data read past the end of the vertex buffer");
        return;
      }
      d.instanceFloats.resize((end - begin) / sizeof(float));
      std::memcpy(d.instanceFloats.data(), bytes.data() + begin, end - begin);
    }
  }

  bool m_rendering = false;
  int m_target = 0;
  FakeGraphicsPipeline *m_pipeline = nullptr;
  std::map<uint32_t, FakeBindGroup *> m_groups;
  std::map<uint32_t, BoundVertexBuffer> m_vertexBuffers;
  FakeBuffer *m_indexBuffer = nullptr;
};

} // namespace fakerhi
