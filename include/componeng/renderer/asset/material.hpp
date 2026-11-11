#pragma once

#include "componeng/core/types.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/batching/instance_data.hpp"
#include "componeng/utils/logger.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace componeng::renderer {

class IMaterial {
public:
  IMaterial(core::HandleID shaderID, core::HandleID textureID)
      : m_shaderID(shaderID), m_textureID(textureID) {
  }

  virtual ~IMaterial() = default;

  core::HandleID getShaderID() const {
    return m_shaderID;
  }

  core::HandleID getTextureID() const {
    return m_textureID;
  }

  // PBR texture IDs (default 0 = not used). Overridden by PbrMaterial.
  virtual core::HandleID getNormalMapID() const {
    return 0;
  }
  virtual core::HandleID getMetallicMapID() const {
    return 0;
  }
  virtual core::HandleID getRoughnessMapID() const {
    return 0;
  }
  virtual core::HandleID getAOMapID() const {
    return 0;
  }

  void setUniform(const char *name, core::UniformMap::mapped_type value) {
    if (!m_knownUniforms.empty() &&
        m_knownUniforms.find(core::Name(name)) == m_knownUniforms.end()) {
      LOG_ERROR("setUniform: '%s' is not an active uniform in this "
                "material's shader",
                name);
    }
    m_uniforms[name] = value;
  }

  const core::UniformMap &getUniforms() const {
    return m_uniforms;
  }

  // --- Extra resources the shader reads ------------------------------------
  //
  // Beyond its albedo (and a PbrMaterial's maps), a material can hand its
  // shader any number of textures and buffers, by the names the shader
  // declares. The RenderSystem binds them before each of the material's draws:
  // textures and buffer textures take units from api::kFirstMaterialTextureUnit
  // up, uniform blocks take binding points from
  // api::kFirstMaterialUniformBinding up.

  struct TextureBinding {
    std::string sampler; // `uniform sampler2D <sampler>`
    core::HandleID texture = 0;
  };
  struct BufferTextureBinding {
    std::string sampler; // `uniform samplerBuffer <sampler>`
    core::HandleID buffer = 0;
    api::BufferTextureFormat format = api::BufferTextureFormat::R32F;
  };
  struct UniformBlockBinding {
    std::string block; // `layout(std140) uniform <block> { ... }`
    core::HandleID buffer = 0;
  };

  /** Binds a texture (AssetManager::getTextureID) to the shader's sampler2D
   *  `sampler`. Setting the same sampler again replaces the texture. */
  void setTexture(const char *sampler, core::HandleID texture) {
    warnIfNotUniform(sampler, "setTexture");
    for (TextureBinding &b : m_textureBindings) {
      if (b.sampler == sampler) {
        b.texture = texture;
        return;
      }
    }
    m_textureBindings.push_back({sampler, texture});
  }

  /** Binds a buffer (AssetManager::getBufferID) to the shader's samplerBuffer
   *  `sampler`, read with texelFetch. Unlike a uniform block there is no small
   *  size limit, so this is how to give a shader a long list of values. */
  void setBufferTexture(const char *sampler, core::HandleID buffer,
                        api::BufferTextureFormat format) {
    warnIfNotUniform(sampler, "setBufferTexture");
    for (BufferTextureBinding &b : m_bufferTextureBindings) {
      if (b.sampler == sampler) {
        b.buffer = buffer;
        b.format = format;
        return;
      }
    }
    m_bufferTextureBindings.push_back({sampler, buffer, format});
  }

  /** Binds a buffer to the shader's std140 uniform block `block`. */
  void setUniformBuffer(const char *block, core::HandleID buffer) {
    for (UniformBlockBinding &b : m_uniformBlockBindings) {
      if (b.block == block) {
        b.buffer = buffer;
        return;
      }
    }
    m_uniformBlockBindings.push_back({block, buffer});
  }

  const std::vector<TextureBinding> &getTextureBindings() const {
    return m_textureBindings;
  }
  const std::vector<BufferTextureBinding> &getBufferTextureBindings() const {
    return m_bufferTextureBindings;
  }
  const std::vector<UniformBlockBinding> &getUniformBlockBindings() const {
    return m_uniformBlockBindings;
  }

  /** Called by AssetManager::registerMaterial right after construction, from
   *  the shader's reflected active uniforms; makes setUniform() typo-safe. */
  void setKnownUniforms(const std::vector<core::Name> &names) {
    m_knownUniforms.clear();
    for (const auto &n : names) {
      m_knownUniforms.insert(n);
    }
  }

  /** Pipeline state (culling, depth, blending) this material draws with.
   *  Defaults to the engine baseline: back-face culling, depth test and
   *  write, alpha blending. */
  void setRenderState(const api::RenderState &state) {
    m_renderState = state;
  }

  const api::RenderState &getRenderState() const {
    return m_renderState;
  }

  /** True when this material blends without writing depth. RenderSystem draws
   *  such batches after all the others so they blend over a finished opaque
   *  scene. */
  bool isTransparent() const {
    return m_renderState.blend != api::BlendMode::None &&
           !m_renderState.depthWrite;
  }

  /** Metres added on every side of an entity's bounding box when culling it.
   *  Needed when the vertex shader moves geometry outside the mesh's own
   *  bounds (billboards, wind sway, waves); otherwise such entities would be
   *  culled while part of them is still on screen. Default 0. */
  void setBoundsPadding(float metres) {
    m_boundsPadding = metres;
  }

  float getBoundsPadding() const {
    return m_boundsPadding;
  }

  void setVertexLayout(const api::VertexLayout &layout) {
    m_vertexLayout = layout;
    resolveInstanceSlots(layout);
  }

  /** Called whenever the material receives its vertex layout (registration,
   *  or a changed shader). Look up the attributes you write here, with
   *  findInstanceSlot()/findInstanceMatrixSlot(), and keep the results as
   *  members: writeInstanceData() runs once per entity per frame and should
   *  not search the layout by name each time. */
  virtual void resolveInstanceSlots(const api::VertexLayout &) {
  }

  const api::VertexLayout &getVertexLayout() const {
    return m_vertexLayout;
  }

  /** Writes one entity's per-instance attributes into `out`, keyed by the
   *  shader's own attribute names (see IShader::reflectInstanceLayout).
   *
   *  This is the hot path: it runs once per rendered entity per frame, writing
   *  straight into the batch's flat instance buffer. Override it, avoid heap
   *  allocation, and write through slots resolved in resolveInstanceSlots()
   *  rather than by Name.
   *
   *  The default implementation adapts the legacy buildInstanceData() below so
   *  existing materials keep working, at the cost of a map per entity. */
  virtual void writeInstanceData(const InstanceContext &ctx,
                                 InstanceWriter &out) const {
    m_scratchInstanceData.clear();
    buildInstanceData(ctx.world, ctx.entity, m_scratchInstanceData);
    for (const auto &[name, value] : m_scratchInstanceData) {
      out.set(name, value);
    }
  }

  /** @deprecated Override writeInstanceData() instead. Kept so materials
   *  written against the old map-based API still compile and run. */
  virtual void buildInstanceData(ecs::World &, ecs::EntityID,
                                 core::UniformMap &) const {
  }

private:
  void warnIfNotUniform(const char *name, const char *caller) const {
    if (!m_knownUniforms.empty() &&
        m_knownUniforms.find(core::Name(name)) == m_knownUniforms.end()) {
      LOG_ERROR("%s: '%s' is not an active uniform in this material's shader",
                caller, name);
    }
  }

  core::HandleID m_shaderID = 0;
  core::HandleID m_textureID = 0;
  api::VertexLayout m_vertexLayout;
  api::RenderState m_renderState;
  float m_boundsPadding = 0.0f;
  core::UniformMap m_uniforms;
  std::vector<TextureBinding> m_textureBindings;
  std::vector<BufferTextureBinding> m_bufferTextureBindings;
  std::vector<UniformBlockBinding> m_uniformBlockBindings;
  std::unordered_set<core::Name> m_knownUniforms;
  mutable core::UniformMap m_scratchInstanceData;
};

} // namespace componeng::renderer
