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
#include "componeng/resources/fog.hpp"
#include "componeng/renderer/component/skybox_component.hpp"

namespace componeng::renderer {

static constexpr core::Vec3 DEFAULT_LIGHT_DIR = core::Vec3(-0.2f, 1.0f, -0.3f);
static constexpr core::Vec3 DEFAULT_LIGHT_COLOR = core::Vec3(1.0f, 1.0f, 1.0f);

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

void RenderSystem::onUpdate(const ecs::SystemState &state) {
  api::IRenderDevice &renderDevice = state.world->getRenderDevice();
  renderDevice.clear(0.0f, 0.0f, 0.0f, 1.0f);

  ecs::EntityID mainCameraID =
      state.world->getResource<camera::MainCamera>().entity;

  core::Vec3 &cameraPos =
      state.world->getComponent<core::TransformComponent>(mainCameraID)
          .position;
  core::Mat4 &viewProj =
      state.world->getComponent<camera::CameraComponent>(mainCameraID)
          .viewProjectionMatrix;

  // Read fog resource
  auto &fog = state.world->getResource<resources::Fog>();

  int drawCalls = 0;

  core::Vec3 lightDir = DEFAULT_LIGHT_DIR;
  core::Vec3 lightColor = DEFAULT_LIGHT_COLOR;

  state.world->query<renderer::DirectionalLightComponent>().each(
      [&](renderer::DirectionalLightComponent &l) {
        lightDir = l.direction;
        lightColor = l.color;
      });

  // ------------------------------------------------------------------
  // Skybox pass: procedural sky drawn first, behind everything.
  // The cube is viewed from the inside, so culling is off, and the shader
  // pins depth to exactly 1.0, so the depth test must accept equal depth.
  // The horizon colour follows the fog so distant geometry blends into it.
  // ------------------------------------------------------------------
  state.world->query<renderer::SkyboxComponent>().each(
      [&](renderer::SkyboxComponent &skybox) {
        const auto &assetManager = state.world->getResource<AssetManager>();
        const Shader &shader =
            assetManager.getShader(assetManager.getShaderID("skybox"));
        const Mesh &model =
            assetManager.getMesh(assetManager.getMeshID("cube"));

        auto &cam =
            state.world->getComponent<camera::CameraComponent>(mainCameraID);

        shader.use();
        // Projection x rotation-only view: the sky follows the camera's
        // orientation but not its position.
        shader.setMatrix4("viewProj", cam.projectionMatrix * cam.viewMatrix);
        shader.setVector3f("skyColor", skybox.color.x, skybox.color.y,
                           skybox.color.z);
        shader.setVector3f("horizonColor", fog.color.x, fog.color.y,
                           fog.color.z);
        shader.setVector3f("sunDir", lightDir.x, lightDir.y, lightDir.z);
        shader.setVector3f("sunColor", lightColor.x, lightColor.y,
                           lightColor.z);

        renderDevice.setCullEnabled(false);
        renderDevice.setDepthFunc(api::DepthFunc::LessEqual);
        model.getImpl().bind();
        renderDevice.drawIndexedInstanced(model.indexCount(), 1);
        renderDevice.setDepthFunc(api::DepthFunc::Less);
        renderDevice.setCullEnabled(true);
      });

  int instancesRendered = 0;
  auto &renderQueue = state.world->getResource<RenderQueue>();

  for (const auto &batch : renderQueue.getBatches()) {
    const core::HandleID meshID = batch.meshID;
    const core::HandleID materialID = batch.materialID;
    const auto &data = batch;
    const api::VertexLayout &layout = data.vertexLayout;

    const auto &assetManager = state.world->getResource<AssetManager>();

    auto &material = assetManager.getMaterial(materialID);
    const Mesh &model = assetManager.getMesh(meshID);
    const Shader &shader = assetManager.getShader(material.getShaderID());
    const Texture2D &albedoTex = assetManager.getTexture(material.getTextureID());

    shader.use();
    shader.setMatrix4("viewProj", viewProj);
    shader.setVector3f("lightDir", lightDir);
    shader.setVector3f("lightColor", lightColor);
    shader.setVector3f("cameraPos", cameraPos.x, cameraPos.y, cameraPos.z);
    shader.setFloat("time", state.world->time);

    // Fog uniforms
    shader.setVector4f("fogColor", fog.color.x, fog.color.y, fog.color.z,
                       fog.color.w);
    shader.setFloat("fogDensity", fog.density);
    shader.setFloat("fogStart", fog.start);
    shader.setFloat("fogEnd", fog.end);

    // Ambient / environment
    shader.setVector3f("ambientColor", 0.3f, 0.4f, 0.6f); // sky-like
    shader.setFloat("ambientStrength", 0.4f);

    // Texture toggles (PBR materials use textures; diffuse may not)
    bool isPbr = dynamic_cast<const PbrMaterial *>(&material) != nullptr;
    shader.setInteger("useTextures", isPbr ? 1 : 0);
    shader.setInteger("useNormalMap", isPbr ? 1 : 0);

    // Bind albedo to texture unit 0
    renderDevice.setActiveTextureUnit(0);
    albedoTex.bind();
    shader.setInteger("albedoMap", 0);

    // Bind PBR textures to units 1-4 if available
    if (isPbr) {
      const auto *pbr = dynamic_cast<const PbrMaterial *>(&material);

      renderDevice.setActiveTextureUnit(1);
      if (pbr->getNormalMapID() != 0) {
        const Texture2D &normalTex = assetManager.getTexture(pbr->getNormalMapID());
        normalTex.bind();
      } else {
        albedoTex.bind(); // fallback
      }
      shader.setInteger("normalMap", 1);

      renderDevice.setActiveTextureUnit(2);
      if (pbr->getMetallicMapID() != 0) {
        const Texture2D &metallicTex = assetManager.getTexture(pbr->getMetallicMapID());
        metallicTex.bind();
      } else {
        albedoTex.bind();
      }
      shader.setInteger("metallicMap", 2);

      renderDevice.setActiveTextureUnit(3);
      if (pbr->getRoughnessMapID() != 0) {
        const Texture2D &roughTex = assetManager.getTexture(pbr->getRoughnessMapID());
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
    }

    // Set material uniforms (fogColor etc. from PbrMaterial constructor)
    setShaderUniforms(shader, material);

    model.getImpl().bind();

    std::vector<float> flatInstanceData;
    flatInstanceData.reserve(data.instanceDatas.size() *
                             (layout.stride / sizeof(float)));
    for (const auto &instance : data.instanceDatas) {
      flatInstanceData.insert(flatInstanceData.end(), instance.begin(),
                              instance.end());
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
    size_t requiredBytes = flatInstanceData.size() * sizeof(float);
    if (requiredBytes > buf->size()) {
      buf->setData(flatInstanceData.data(), requiredBytes,
                   api::IBuffer::Usage::Dynamic);
    } else if (requiredBytes > 0) {
      buf->setSubData(0, flatInstanceData.data(), requiredBytes);
    }

    renderDevice.setupInstanceAttributes(*buf, material.getVertexLayout());

    renderDevice.drawIndexedInstanced(model.indexCount(),
                                      data.instanceDatas.size());

    renderDevice.unbindInstanceAttributes(material.getVertexLayout());

    // Reset active texture unit to 0
    renderDevice.setActiveTextureUnit(0);

    ++drawCalls;
    instancesRendered += batch.instanceDatas.size();
  }

  renderQueue.clear();

  core::DebugUI::addValue("Instances Rendered", instancesRendered);
  core::DebugUI::addValue("Draw Calls", drawCalls);
}

} // namespace componeng::renderer
