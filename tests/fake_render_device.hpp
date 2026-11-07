#pragma once

#include "componeng/renderer/backend/api/irender_device.hpp"

#include <cstring>
#include <map>
#include <string>
#include <vector>

// A render device that draws nothing and records what it was asked to do, so
// the engine's batching and render systems can be tested without a GL context.
// It also demonstrates that asset loading no longer needs OpenGL: every
// GPU-side object comes from the device's factory methods.
namespace fake {

using namespace componeng::renderer;

struct Draw {
  size_t indexCount = 0;
  uint32_t instanceCount = 0;
  api::RenderState state;            // pipeline state at the moment of the draw
  int shaderId = 0;                  // shader in use
  int albedoTextureId = 0;           // texture bound to unit 0
  std::vector<float> instanceFloats; // instance buffer contents at draw time
};

struct UniformBufferBind {
  uint32_t binding = 0;
  std::vector<unsigned char> bytes; // buffer contents at bind time
};

// Everything the fake device observed, in one place so shaders, textures and
// meshes (which outlive any single call) can report into it.
struct Recorder {
  int shadersCreated = 0;
  int texturesCreated = 0;
  int meshesCreated = 0;
  int buffersCreated = 0;

  int clears = 0;
  std::vector<api::RenderState> stateRequests; // every setRenderState call
  std::vector<int> shaderUses;                 // shader id per use()
  std::vector<int> unit0TextureBinds;          // texture id per unit-0 bind
  std::vector<UniformBufferBind> uniformBufferBinds;
  std::vector<Draw> draws;
  std::vector<Draw> fullscreenDraws;     // drawFullscreenTriangle calls
  std::vector<std::string> drawSequence; // "fullscreen" / "indexed", in order

  // Names passed to loose uniform setters, per shader id.
  std::map<int, std::vector<std::string>> looseUniforms;

  // Configuration the tests can change before creating assets.
  api::VertexLayout instanceLayout;
  std::map<int, bool> shaderDeclaresFrameBlock; // by shader id (creation order)
  bool defaultShaderDeclaresFrameBlock = false;

  // Current state, updated as calls arrive.
  api::RenderState currentState;
  int currentShader = 0;
  uint32_t activeUnit = 0;
  std::map<uint32_t, int> boundTextureByUnit;
  std::vector<float>
      pendingInstanceFloats; // from the latest setupInstanceAttributes

  // One entry per texture created, in creation order.
  std::vector<api::TextureDesc> textureDescs;
  std::vector<bool> textureHadAlpha;
  std::vector<std::pair<uint32_t, uint32_t>> textureSizes;
};

class FakeBuffer final : public api::IBuffer {
public:
  void setData(const void *data, size_t sizeBytes, Usage) override {
    m_bytes.assign(static_cast<const unsigned char *>(data),
                   static_cast<const unsigned char *>(data) + sizeBytes);
  }
  void setSubData(size_t offset, const void *data, size_t sizeBytes) override {
    if (offset + sizeBytes > m_bytes.size()) {
      m_bytes.resize(offset + sizeBytes);
    }
    std::memcpy(m_bytes.data() + offset, data, sizeBytes);
  }
  size_t size() const override {
    return m_bytes.size();
  }
  void release() override {
  }
  const std::vector<unsigned char> &bytes() const {
    return m_bytes;
  }

private:
  std::vector<unsigned char> m_bytes;
};

class FakeShader final : public api::IShader {
public:
  FakeShader(Recorder &rec, int id) : m_rec(rec), m_id(id) {
  }
  void loadGLSL(const char *, const char *, const char *) override {
  }
  void loadSPIRV(const char *, const char *, const char *) override {
  }
  void use() const override {
    m_rec.shaderUses.push_back(m_id);
    m_rec.currentShader = m_id;
  }
  void release() override {
  }

  void setFloat(const char *n, float) const override {
    note(n);
  }
  void setInteger(const char *n, int) const override {
    note(n);
  }
  void setVector2f(const char *n, float, float) const override {
    note(n);
  }
  void setVector3f(const char *n, float, float, float) const override {
    note(n);
  }
  void setVector4f(const char *n, float, float, float, float) const override {
    note(n);
  }
  void setMatrix4(const char *n, const float *) const override {
    note(n);
  }

  bool bindUniformBlock(const char *, uint32_t, size_t) override {
    auto it = m_rec.shaderDeclaresFrameBlock.find(m_id);
    return it != m_rec.shaderDeclaresFrameBlock.end()
               ? it->second
               : m_rec.defaultShaderDeclaresFrameBlock;
  }

  api::VertexLayout reflectInstanceLayout() const override {
    return m_rec.instanceLayout;
  }
  std::vector<componeng::core::Name>
  reflectActiveUniformNames() const override {
    return {};
  }

private:
  void note(const char *name) const {
    m_rec.looseUniforms[m_id].push_back(name);
  }
  Recorder &m_rec;
  int m_id;
};

class FakeTexture final : public api::ITexture {
public:
  FakeTexture(Recorder &rec, int id) : m_rec(rec), m_id(id) {
  }
  void generate(uint32_t w, uint32_t h, const unsigned char *, bool alpha,
                const api::TextureDesc &desc) override {
    m_width = w;
    m_height = h;
    m_rec.textureDescs.push_back(desc);
    m_rec.textureHadAlpha.push_back(alpha);
    m_rec.textureSizes.push_back({w, h});
  }
  void bind() const override {
    m_rec.boundTextureByUnit[m_rec.activeUnit] = m_id;
    if (m_rec.activeUnit == 0) {
      m_rec.unit0TextureBinds.push_back(m_id);
    }
  }
  void release() override {
  }

private:
  Recorder &m_rec;
  int m_id;
};

class FakeMesh final : public api::IMesh {
public:
  void upload(const float *, size_t, const uint32_t *, size_t indexCount,
              const api::VertexLayout &) override {
    m_indexCount = indexCount;
  }
  void bind() const override {
  }
  void release() override {
  }
  size_t indexCount() const override {
    return m_indexCount;
  }

private:
  size_t m_indexCount = 0;
};

class FakeRenderDevice final : public api::IRenderDevice {
public:
  Recorder rec;

  void init(void *) override {
  }
  void setViewport(int, int, int, int) override {
  }
  void setScissor(int, int, int, int) override {
  }
  void clear(float, float, float, float) override {
    ++rec.clears;
  }
  void present(void *) override {
  }
  int checkErrors() const override {
    return 0;
  }

  std::unique_ptr<api::IShader> createShader() override {
    return std::make_unique<FakeShader>(rec, ++rec.shadersCreated);
  }
  std::unique_ptr<api::ITexture> createTexture() override {
    return std::make_unique<FakeTexture>(rec, ++rec.texturesCreated);
  }
  std::unique_ptr<api::IMesh> createMesh() override {
    ++rec.meshesCreated;
    return std::make_unique<FakeMesh>();
  }
  std::unique_ptr<api::IBuffer> createBuffer() override {
    ++rec.buffersCreated;
    return std::make_unique<FakeBuffer>();
  }

  void setRenderState(const api::RenderState &state) override {
    rec.stateRequests.push_back(state);
    rec.currentState = state;
  }
  void bindUniformBuffer(uint32_t binding, api::IBuffer &buffer) override {
    rec.uniformBufferBinds.push_back(
        {binding, static_cast<FakeBuffer &>(buffer).bytes()});
  }
  void setActiveTextureUnit(uint32_t unit) override {
    rec.activeUnit = unit;
  }
  void setupInstanceAttributes(api::IBuffer &buffer,
                               const api::VertexLayout &) override {
    const auto &bytes = static_cast<FakeBuffer &>(buffer).bytes();
    rec.pendingInstanceFloats.resize(bytes.size() / sizeof(float));
    std::memcpy(rec.pendingInstanceFloats.data(), bytes.data(),
                rec.pendingInstanceFloats.size() * sizeof(float));
  }
  void unbindInstanceAttributes(const api::VertexLayout &) override {
  }
  void drawFullscreenTriangle() override {
    Draw d;
    d.state = rec.currentState;
    d.shaderId = rec.currentShader;
    d.instanceCount = 1;
    rec.fullscreenDraws.push_back(d);
    rec.drawSequence.push_back("fullscreen");
  }
  void drawIndexedInstanced(size_t indexCount,
                            uint32_t instanceCount) override {
    Draw d;
    d.indexCount = indexCount;
    d.instanceCount = instanceCount;
    d.state = rec.currentState;
    d.instanceFloats = rec.pendingInstanceFloats;
    d.shaderId = rec.currentShader;
    auto it = rec.boundTextureByUnit.find(0);
    d.albedoTextureId = it != rec.boundTextureByUnit.end() ? it->second : 0;
    rec.draws.push_back(d);
    rec.drawSequence.push_back("indexed");
  }
};

} // namespace fake
