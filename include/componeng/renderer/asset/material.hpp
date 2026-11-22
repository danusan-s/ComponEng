#pragma once

#include "componeng/core/types.hpp"
#include "componeng/renderer/asset/parameter_block.hpp"
#include "componeng/renderer/asset/shader.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/batching/instance_data.hpp"
#include "componeng/utils/logger.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace componeng::renderer {

/**
 * @brief Which pass a material is drawn in.
 *
 * Opaque: writes depth and fully covers what is behind it. Drawn first, after
 * a depth pre-pass (see resources::RenderSettings), so each pixel is shaded
 * once. The RenderSystem owns the blend and depth state of these draws: the
 * material's RenderState supplies only the culling mode. The fragment shader
 * must not `discard`, since the pre-pass would not know about it (the LOD
 * cross-fade is the exception: see supportsLodFade()).
 *
 * Cutout: opaque, except that the fragment shader may `discard` (an
 * alpha-tested leaf card, a fence, a billboard). Drawn with the opaque batches
 * and in the same state, and in the depth pre-pass too, but there with the
 * shader's own fragment stage compiled under COMPONENG_DEPTH_PREPASS (see
 * Shader::alphaTestedDepthVariant) so the holes are cut into the depth buffer.
 * The colour pass then still shades only the nearest visible texel of each
 * pixel, however many cards overlap there. The pre-pass binds the material's
 * albedo texture and its own textures (setTexture), not the PBR set.
 *
 * Transparent: blended and/or ignores depth. Drawn after the opaque batches
 * and the sky, with the material's RenderState applied exactly as given. Use
 * it for anything blended, and for depth-less overlays (set depthTest = false).
 */
enum class MaterialType { Opaque, Cutout, Transparent };

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

  /** Sets the parameter `name` of the shader's material block (the uniform
   *  block at set 1, binding 0) to `value`. The type must match the member
   *  exactly. An unknown name or a wrong type is logged (once) and ignored. */
  void setUniform(const char *name, UniformValue value) {
    m_uniforms[name] = value;
    applyUniform(name, value);
  }

  const core::UniformMap &getUniforms() const {
    return m_uniforms;
  }

  // --- Extra resources the shader reads ------------------------------------
  //
  // Beyond its albedo (and a PbrMaterial's maps), a material can hand its
  // shader any number of textures and buffers, by the names the shader
  // declares. They are bound together with the material's parameters as set 1.

  struct TextureBinding {
    std::string sampler; // `uniform sampler2D <sampler>`
    core::HandleID texture = 0;
  };
  struct BufferBinding {
    std::string block; // `uniform <block> { ... }` or `buffer <block> { ... }`
    core::HandleID buffer = 0;
  };

  /** Binds a texture (AssetManager::getTextureID) to the shader's sampler2D
   *  `sampler`. Setting the same sampler again replaces the texture. */
  void setTexture(const char *sampler, core::HandleID texture) {
    warnIfUnknown(sampler, shader::ResourceKind::Sampler2D, "setTexture");
    for (TextureBinding &b : m_textureBindings) {
      if (b.sampler == sampler) {
        b.texture = texture;
        touch();
        return;
      }
    }
    m_textureBindings.push_back({sampler, texture});
    touch();
  }

  /** Binds a buffer to the shader's std140 uniform block `block`. The block
   *  must not be the material's own parameter block (binding 0). */
  void setUniformBuffer(const char *block, core::HandleID buffer) {
    warnIfUnknown(block, shader::ResourceKind::UniformBlock,
                  "setUniformBuffer");
    setBufferBinding(m_uniformBlockBindings, block, buffer);
  }

  /** Binds a buffer to the shader's std430 storage block `block`, which a
   *  shader reads like `readonly buffer Positions { vec4 positions[]; };`.
   *  There is no small size limit, so this is how to give a shader a long
   *  list of values. */
  void setStorageBuffer(const char *block, core::HandleID buffer) {
    warnIfUnknown(block, shader::ResourceKind::StorageBlock,
                  "setStorageBuffer");
    setBufferBinding(m_storageBlockBindings, block, buffer);
  }

  const std::vector<TextureBinding> &getTextureBindings() const {
    return m_textureBindings;
  }
  const std::vector<BufferBinding> &getUniformBlockBindings() const {
    return m_uniformBlockBindings;
  }
  const std::vector<BufferBinding> &getStorageBlockBindings() const {
    return m_storageBlockBindings;
  }

  /** Called by AssetManager::registerMaterial right after construction: sizes
   *  the parameter block from the shader's reflection, applies any uniforms
   *  already set, and takes the shader's instance layout. */
  void bindShader(const Shader &shader, rhi::IDevice &device) {
    m_resources.clear();
    for (const shader::ReflectedResource &r : shader.reflection().resources) {
      if (r.set == 1) {
        m_resources.push_back({r.name, r.kind, r.binding});
      }
    }
    m_params = ParameterBlock();
    if (const shader::ReflectedResource *block = shader.materialBlock()) {
      m_params.init(device, *block);
      for (const auto &[name, value] : m_optionalUniforms) {
        m_params.set(name.c_str(), value);
      }
      for (const auto &[name, value] : m_uniforms) {
        applyUniform(name.c_str(), value);
      }
    }
    setVertexLayout(shader.instanceLayout());
    touch();
  }

  ParameterBlock &params() {
    return m_params;
  }
  const ParameterBlock &params() const {
    return m_params;
  }

  /** Changes whenever something a bind group is built from (a texture, a
   *  buffer, the shader) changes; the renderer rebuilds the group then. */
  uint64_t bindingsVersion() const {
    return m_bindingsVersion;
  }

  /** Which pass this material is drawn in; see MaterialType. Default Opaque. */
  void setType(MaterialType type) {
    m_type = type;
  }

  MaterialType getType() const {
    return m_type;
  }

  /** Pipeline state (culling, depth, blending) this material draws with.
   *  Defaults to the engine baseline: back-face culling, depth test and
   *  write, alpha blending. An Opaque or Cutout material only contributes its
   *  culling mode; the rest is decided by the RenderSystem (see MaterialType).
   */
  void setRenderState(const api::RenderState &state) {
    m_renderState = state;
  }

  const api::RenderState &getRenderState() const {
    return m_renderState;
  }

  /** True for MaterialType::Cutout: opaque, but its fragment shader discards,
   *  so its depth pre-pass draw needs the alpha-tested variant. */
  bool isAlphaTested() const {
    return m_type == MaterialType::Cutout;
  }

  /** True for MaterialType::Transparent. RenderSystem draws such batches
   *  after all the others so they blend over a finished opaque scene. */
  bool isTransparent() const {
    return m_type == MaterialType::Transparent;
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
    static const core::Name fadeName("instanceFade");
    m_lodFadeSlot = findInstanceSlot(layout, fadeName);
    resolveInstanceSlots(layout);
  }

  /** True if the shader declares a per-instance `instanceFade` float, which
   *  lets an entity cross-fade between LOD levels (RenderSettings::lodDither).
   *  The engine writes it after writeInstanceData(); 0 means no fade. */
  bool supportsLodFade() const {
    return m_lodFadeSlot.valid();
  }

  /** Writes `fade` into the instance `out` addresses; see supportsLodFade(). */
  void writeLodFade(InstanceWriter &out, float fade) const {
    out.set(m_lodFadeSlot, fade);
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
   *  rather than by Name. */
  virtual void writeInstanceData(const InstanceContext &ctx,
                                 InstanceWriter &out) const = 0;

protected:
  /** For subclasses whose own setters change what gets bound. */
  void touch() {
    ++m_bindingsVersion;
  }

  /** Like setUniform, for a value the engine offers whether or not a shader
   *  declares it: an unknown name or type is silently ignored. */
  void setOptionalUniform(const char *name, UniformValue value) {
    m_optionalUniforms[name] = value;
    if (m_params.valid()) {
      m_params.set(name, value);
    }
  }

private:
  struct ShaderResource {
    std::string name;
    shader::ResourceKind kind;
    uint32_t binding;
  };

  void applyUniform(const char *name, const UniformValue &value) {
    if (!m_params.valid()) {
      return; // applied when bindShader gives the block
    }
    const ParameterBlock::Result r = m_params.set(name, value);
    if (r == ParameterBlock::Result::UnknownName) {
      LOG_ERROR("setUniform: '%s' is not a member of this material's shader "
                "parameter block",
                name);
    } else if (r == ParameterBlock::Result::TypeMismatch) {
      LOG_ERROR("setUniform: '%s' has a different type in the shader", name);
    }
  }

  // Resources only exist once a shader is bound; before that every name is
  // accepted and checked later by the binder.
  void warnIfUnknown(const char *name, shader::ResourceKind kind,
                     const char *caller) const {
    if (m_resources.empty()) {
      return;
    }
    for (const ShaderResource &r : m_resources) {
      if (r.name == name && r.kind == kind) {
        return;
      }
    }
    LOG_ERROR("%s: '%s' is not a matching resource of this material's shader",
              caller, name);
  }

  void setBufferBinding(std::vector<BufferBinding> &list, const char *block,
                        core::HandleID buffer) {
    for (BufferBinding &b : list) {
      if (b.block == block) {
        b.buffer = buffer;
        touch();
        return;
      }
    }
    list.push_back({block, buffer});
    touch();
  }

  core::HandleID m_shaderID = 0;
  core::HandleID m_textureID = 0;
  api::VertexLayout m_vertexLayout;
  InstanceSlot m_lodFadeSlot;
  MaterialType m_type = MaterialType::Opaque;
  api::RenderState m_renderState;
  float m_boundsPadding = 0.0f;
  core::UniformMap m_uniforms;
  core::UniformMap m_optionalUniforms;
  ParameterBlock m_params;
  std::vector<ShaderResource> m_resources;
  uint64_t m_bindingsVersion = 0;
  std::vector<TextureBinding> m_textureBindings;
  std::vector<BufferBinding> m_uniformBlockBindings;
  std::vector<BufferBinding> m_storageBlockBindings;
};

} // namespace componeng::renderer
