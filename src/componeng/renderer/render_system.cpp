#include "componeng/renderer/render_system.hpp"

#include "componeng/camera/camera_component.hpp"
#include "componeng/camera/main_camera.hpp"
#include "componeng/core/debug_ui.hpp"
#include "componeng/core/transform_component.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset/material.hpp"
#include "componeng/renderer/asset/pbr.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/batching/render_queue.hpp"
#include "componeng/renderer/component/light_component.hpp"
#include "componeng/renderer/component/material_component.hpp"
#include "componeng/renderer/component/mesh_component.hpp"
#include "componeng/renderer/component/skybox_component.hpp"
#include "componeng/renderer/frame_data.hpp"
#include "componeng/resources/fog.hpp"

#include <algorithm>
#include <string>
#include <unordered_set>

namespace componeng::renderer {

static constexpr core::Vec3 DEFAULT_LIGHT_DIR = core::Vec3(-0.2f, 1.0f, -0.3f);
static constexpr core::Vec3 DEFAULT_LIGHT_COLOR = core::Vec3(1.0f, 1.0f, 1.0f);
static constexpr core::Vec3 AMBIENT_COLOR = core::Vec3(0.3f, 0.4f, 0.6f);
static constexpr float AMBIENT_STRENGTH = 0.4f;

static void setShaderUniforms(const Shader &shader, const IMaterial &material) {
  // Set material uniforms
  for (const auto &[name, value] : material.getUniforms()) {
    const char *iname = name;
    std::visit(
        [&shader, iname](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, float>) {
            shader.setFloat(iname, arg);
          } else if constexpr (std::is_same_v<T, core::Vec2>) {
            shader.setVector2f(iname, arg);
          } else if constexpr (std::is_same_v<T, core::Vec3>) {
            shader.setVector3f(iname, arg);
          } else if constexpr (std::is_same_v<T, core::Vec4>) {
            shader.setVector4f(iname, arg);
          } else if constexpr (std::is_same_v<T, core::Mat4>) {
            shader.setMatrix4(iname, arg);
          }
        },
        value);
  }
}

// Binds the extra textures, texture buffers and uniform blocks a material
// declared (IMaterial::setTexture and friends). Problems are reported once.
static void bindMaterialResources(api::IRenderDevice &device,
                                  const AssetManager &assets,
                                  const Shader &shader,
                                  const IMaterial &material,
                                  std::unordered_set<std::string> &reported) {
  auto report = [&reported](const std::string &key, const char *message,
                            const std::string &name) {
    if (reported.insert(key).second) {
      LOG_ERROR("%s: %s", message, name.c_str());
    }
  };

  uint32_t unit = api::kFirstMaterialTextureUnit;
  for (const auto &b : material.getTextureBindings()) {
    if (unit >= api::kTextureUnitLimit) {
      report("units:" + b.sampler, "out of texture units for sampler",
             b.sampler);
      break;
    }
    device.setActiveTextureUnit(unit);
    assets.getTexture(b.texture).bind();
    shader.setInteger(b.sampler.c_str(), static_cast<int>(unit));
    ++unit;
  }
  for (const auto &b : material.getBufferTextureBindings()) {
    if (unit >= api::kTextureUnitLimit) {
      report("units:" + b.sampler, "out of texture units for sampler",
             b.sampler);
      break;
    }
    device.bindBufferTexture(unit, assets.getBuffer(b.buffer), b.format);
    shader.setInteger(b.sampler.c_str(), static_cast<int>(unit));
    ++unit;
  }
  if (unit != api::kFirstMaterialTextureUnit) {
    // Back to unit 0 so the albedo binding the loop tracks stays the one that
    // later binds and draws see.
    device.setActiveTextureUnit(0);
  }

  uint32_t binding = api::kFirstMaterialUniformBinding;
  for (const auto &b : material.getUniformBlockBindings()) {
    if (binding >= api::kUniformBindingLimit) {
      report("bindings:" + b.block, "out of uniform buffer bindings for block",
             b.block);
      break;
    }
    if (shader.bindUniformBlock(b.block.c_str(), binding)) {
      device.bindUniformBuffer(binding, assets.getBuffer(b.buffer));
    } else {
      report("block:" + b.block, "the shader declares no uniform block",
             b.block);
    }
    ++binding;
  }
}

// Sends FrameData's values as loose uniforms, for shaders that predate the
// FrameData block. The values come from the same struct that is uploaded to
// the uniform buffer, so both paths always agree.
static void setLooseFrameUniforms(const Shader &shader, const FrameData &frame,
                                  const core::Mat4 &viewProj) {
  shader.setMatrix4("viewProj", viewProj);
  shader.setVector3f("lightDir", frame.lightDir[0], frame.lightDir[1],
                     frame.lightDir[2]);
  shader.setVector3f("lightColor", frame.lightColor[0], frame.lightColor[1],
                     frame.lightColor[2]);
  shader.setVector3f("cameraPos", frame.cameraPos[0], frame.cameraPos[1],
                     frame.cameraPos[2]);
  shader.setFloat("time", frame.time);

  shader.setVector4f("fogColor", frame.fogColor[0], frame.fogColor[1],
                     frame.fogColor[2], frame.fogColor[3]);
  shader.setFloat("fogDensity", frame.fogDensity);
  shader.setFloat("fogStart", frame.fogStart);
  shader.setFloat("fogEnd", frame.fogEnd);

  shader.setVector3f("ambientColor", frame.ambientColor[0],
                     frame.ambientColor[1], frame.ambientColor[2]);
  shader.setFloat("ambientStrength", frame.ambientStrength);
}

void RenderSystem::onUpdate(const ecs::SystemState &state) {
  api::IRenderDevice &renderDevice = state.world->getRenderDevice();
  renderDevice.setRenderState(api::RenderState{});
  renderDevice.clear(0.0f, 0.0f, 0.0f, 1.0f);

  ecs::EntityID mainCameraID =
      state.world->getResource<camera::MainCamera>().entity;

  const core::Vec3 &cameraPos =
      state.world->readComponent<core::TransformComponent>(mainCameraID)
          .position;
  core::Mat4 &viewProj =
      state.world->getComponent<camera::CameraComponent>(mainCameraID)
          .viewProjectionMatrix;

  // Read fog resource
  auto &fog = state.world->getResource<resources::Fog>();

  int drawCalls = 0;

  core::Vec3 lightDir = DEFAULT_LIGHT_DIR;
  core::Vec3 lightColor = DEFAULT_LIGHT_COLOR;

  state.world->query().read<renderer::DirectionalLightComponent>().each(
      [&](const renderer::DirectionalLightComponent &l) {
        lightDir = l.direction;
        lightColor = l.color;
      });

  // Per-frame values shared by every shader that declares the FrameData block.
  FrameData frame{};
  std::copy_n(core::value_ptr(viewProj), 16, frame.viewProj);
  std::copy_n(core::value_ptr(cameraPos), 3, frame.cameraPos);
  frame.time = static_cast<float>(state.world->time);
  std::copy_n(core::value_ptr(lightDir), 3, frame.lightDir);
  std::copy_n(core::value_ptr(lightColor), 3, frame.lightColor);
  std::copy_n(core::value_ptr(AMBIENT_COLOR), 3, frame.ambientColor);
  frame.ambientStrength = AMBIENT_STRENGTH;
  std::copy_n(core::value_ptr(fog.color), 4, frame.fogColor);
  frame.fogDensity = fog.density;
  frame.fogStart = fog.start;
  frame.fogEnd = fog.end;

  if (!m_frameBuffer) {
    m_frameBuffer = renderDevice.createBuffer();
  }
  if (m_frameBuffer->size() < sizeof(FrameData)) {
    m_frameBuffer->setData(&frame, sizeof(FrameData),
                           api::IBuffer::Usage::Dynamic);
  } else {
    m_frameBuffer->setSubData(0, &frame, sizeof(FrameData));
  }
  renderDevice.bindUniformBuffer(kFrameDataBinding, *m_frameBuffer);

  // ------------------------------------------------------------------
  // Sky pass: drawn first, behind everything, as a full-screen triangle.
  // The shader pins depth to exactly 1.0, so the depth test must accept equal
  // depth, and there is nothing behind it to protect so it does not write
  // depth. Only the first SkyboxComponent is drawn.
  // ------------------------------------------------------------------
  bool skyDrawn = false;
  state.world->query().read<renderer::SkyboxComponent>().each(
      [&](const renderer::SkyboxComponent &sky) {
        if (skyDrawn) {
          return;
        }
        skyDrawn = true;

        const auto &assets = state.world->getResource<AssetManager>();
        const Shader &shader =
            assets.getShader(assets.getShaderID(sky.shaderName));
        auto &cam =
            state.world->getComponent<camera::CameraComponent>(mainCameraID);

        shader.use();
        if (!shader.usesFrameData()) {
          setLooseFrameUniforms(shader, frame, viewProj);
        }

        // Projection x rotation-only view, inverted: turns a pixel back into
        // the direction the camera sees through it. No camera translation, so
        // the sky stays fixed relative to the camera's orientation.
        shader.setMatrix4("invSkyViewProj",
                          core::inverse(cam.projectionMatrix * cam.viewMatrix));
        shader.setVector3f("skyColor", sky.color);
        // The fog colour as horizon lets fogged ground fade into the sky.
        shader.setVector3f(
            "horizonColor",
            sky.horizonFromFog
                ? core::Vec3(fog.color.x, fog.color.y, fog.color.z)
                : sky.horizonColor);

        api::RenderState skyState;
        skyState.cull = api::CullMode::None;
        skyState.depthFunc = api::DepthFunc::LessEqual;
        skyState.depthWrite = false;
        renderDevice.setRenderState(skyState);

        renderDevice.drawFullscreenTriangle();

        renderDevice.setRenderState(api::RenderState{});
      });

  int instancesRendered = 0;
  auto &renderQueue = state.world->getResource<RenderQueue>();
  const auto &assetManager = state.world->getResource<AssetManager>();

  // The queue is sorted by shader, then texture, so consecutive batches often
  // share GPU state. Track what is currently bound and skip redundant work.
  // GL state is only trusted within this loop: ImGui or anything else may have
  // changed it between frames.
  core::HandleID boundShader = core::INVALID_HANDLE;
  core::HandleID boundAlbedo = core::INVALID_HANDLE;
  // A material's own uniforms can overwrite a frame-wide one (e.g. its own
  // fogColor). The shader keeps that value, so after such a material the
  // frame-wide uniforms must be sent again even on the same shader.
  bool materialOverrodeUniforms = false;

  for (std::size_t batchIndex = 0; batchIndex < renderQueue.size();
       ++batchIndex) {
    const RenderBatch &batch = renderQueue.sorted(batchIndex);
    const core::HandleID meshID = batch.meshID;
    const core::HandleID materialID = batch.materialID;

    auto &material = assetManager.getMaterial(materialID);
    const api::VertexLayout &layout = material.getVertexLayout();
    const Mesh &model = assetManager.getMesh(meshID);
    const Shader &shader = assetManager.getShader(material.getShaderID());
    const Texture2D &albedoTex =
        assetManager.getTexture(material.getTextureID());

    const bool shaderChanged = material.getShaderID() != boundShader;
    if (shaderChanged) {
      shader.use();
      boundShader = material.getShaderID();
    }

    renderDevice.setRenderState(material.getRenderState());

    if (shaderChanged || materialOverrodeUniforms) {
      if (!shader.usesFrameData()) {
        setLooseFrameUniforms(shader, frame, viewProj);
      }
      shader.setInteger("albedoMap", 0);
    }

    // Texture toggles (PBR materials use textures; diffuse may not)
    const auto *pbr = dynamic_cast<const PbrMaterial *>(&material);
    const bool isPbr = pbr != nullptr;
    shader.setInteger("useTextures", isPbr ? 1 : 0);
    shader.setInteger("useNormalMap", isPbr ? 1 : 0);

    // Bind albedo to texture unit 0
    if (material.getTextureID() != boundAlbedo) {
      renderDevice.setActiveTextureUnit(0);
      albedoTex.bind();
      boundAlbedo = material.getTextureID();
    }

    // Bind PBR textures to units 1-4 if available
    if (isPbr) {
      renderDevice.setActiveTextureUnit(1);
      if (pbr->getNormalMapID() != 0) {
        const Texture2D &normalTex =
            assetManager.getTexture(pbr->getNormalMapID());
        normalTex.bind();
      } else {
        albedoTex.bind(); // fallback
      }
      shader.setInteger("normalMap", 1);

      renderDevice.setActiveTextureUnit(2);
      if (pbr->getMetallicMapID() != 0) {
        const Texture2D &metallicTex =
            assetManager.getTexture(pbr->getMetallicMapID());
        metallicTex.bind();
      } else {
        albedoTex.bind();
      }
      shader.setInteger("metallicMap", 2);

      renderDevice.setActiveTextureUnit(3);
      if (pbr->getRoughnessMapID() != 0) {
        const Texture2D &roughTex =
            assetManager.getTexture(pbr->getRoughnessMapID());
        roughTex.bind();
      } else {
        albedoTex.bind();
      }
      shader.setInteger("roughnessMap", 3);

      renderDevice.setActiveTextureUnit(4);
      if (pbr->getAOMapID() != 0) {
        const Texture2D &aoTex = assetManager.getTexture(pbr->getAOMapID());
        aoTex.bind();
      } else {
        albedoTex.bind();
      }
      shader.setInteger("aoMap", 4);

      // Back to unit 0 so the albedo binding tracked above stays the one that
      // later binds and draws see.
      renderDevice.setActiveTextureUnit(0);
    }

    bindMaterialResources(renderDevice, assetManager, shader, material,
                          m_reportedBindingProblems);

    // Set material uniforms (fogColor etc. from PbrMaterial constructor)
    setShaderUniforms(shader, material);

    materialOverrodeUniforms = !material.getUniforms().empty();

    model.getImpl().bind();

    // Procedural batches carry no per-instance data: nothing to upload and no
    // instance attributes to enable; the shader reads gl_InstanceID.
    if (batch.procedural) {
      renderDevice.drawIndexedInstanced(model.indexCount(),
                                        batch.instanceCount);
      ++drawCalls;
      instancesRendered += batch.instanceCount;
      continue;
    }

    // Reuse one GPU buffer per (mesh,material) combo across frames instead of
    // creating/destroying one every draw call; only reallocate (setData) when
    // this batch needs more room than it did before, otherwise update the
    // existing buffer in place (setSubData).
    std::unique_ptr<api::IBuffer> &buf =
        m_instanceBuffers[DrawKey{meshID, materialID}];
    if (!buf) {
      buf = renderDevice.createBuffer();
    }
    const size_t requiredBytes = batch.instanceData.size() * sizeof(float);
    if (requiredBytes > buf->size()) {
      buf->setData(batch.instanceData.data(), requiredBytes,
                   api::IBuffer::Usage::Dynamic);
    } else if (requiredBytes > 0) {
      buf->setSubData(0, batch.instanceData.data(), requiredBytes);
    }

    renderDevice.setupInstanceAttributes(*buf, layout);

    renderDevice.drawIndexedInstanced(model.indexCount(), batch.instanceCount);

    renderDevice.unbindInstanceAttributes(layout);

    ++drawCalls;
    instancesRendered += batch.instanceCount;
  }

  // The queue is cleared by BatchingSystem at the start of next frame, which
  // lets its batch storage be reused instead of freed.

  core::DebugUI::addValue("Instances Rendered", instancesRendered);
  core::DebugUI::addValue("Draw Calls", drawCalls);
}

} // namespace componeng::renderer
