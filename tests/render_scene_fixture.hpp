#pragma once

#include "fake_device.hpp"

#include "componeng/camera/camera_component.hpp"
#include "componeng/camera/main_camera.hpp"
#include "componeng/core/transform_component.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset/diffuse.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/batching/batching_system.hpp"
#include "componeng/renderer/component/light_component.hpp"
#include "componeng/renderer/component/material_component.hpp"
#include "componeng/renderer/component/mesh_component.hpp"
#include "componeng/renderer/component/skybox_component.hpp"
#include "componeng/renderer/culling/culling_system.hpp"
#include "componeng/renderer/frame_data.hpp"
#include "componeng/renderer/render_system.hpp"
#include "componeng/resources/fog.hpp"
#include "componeng/resources/render_settings.hpp"
#include "componeng/utils/utils.hpp"

#include <functional>
#include <memory>

// A World wired to the fake render device with the batching and render
// systems (and optionally culling) registered, plus a handful of assets.
namespace rscene {

using namespace componeng;
using namespace componeng::core;
using namespace componeng::ecs;
using namespace componeng::renderer;

// What the test shaders declare. Shader "a" is the one tests customise;
// shader "b" is always the plain one.
struct SceneOptions {
  // Per-instance inputs instanceModel and instanceColor; off for procedural
  // draws, whose shaders read INSTANCE_INDEX instead.
  bool instanced = true;
  // The instanceFade float after them, for LOD cross-fades.
  bool fade = false;
  // Shader "a" also reads a Settings uniform block (set 1, binding 2),
  // samplers heightMap, first and second (bindings 3 to 5) and a storage block
  // positions (binding 6), which materials can bind.
  bool extras = false;
};

inline std::string testVertexSource(const SceneOptions &o) {
  std::string s = "#version 460\n"
                  "#include \"frame_data.glsl\"\n"
                  "layout(location = 0) in vec3 aPos;\n"
                  "layout(location = 1) in vec3 aNormal;\n"
                  "layout(location = 2) in vec2 aUV;\n";
  if (o.instanced) {
    s += "layout(location = 3) in mat4 instanceModel;\n"
         "layout(location = 7) in vec4 instanceColor;\n";
    if (o.fade) {
      s += "layout(location = 8) in float instanceFade;\n";
    }
  }
  s += "layout(location = 0) out vec2 vUV;\n"
       "layout(location = 1) out vec4 vColor;\n"
       "layout(location = 2) flat out float vFade;\n"
       "invariant gl_Position;\n"
       "void main() {\n";
  s += std::string("  vUV = aUV + aNormal.xy;\n  vColor = ") +
       (o.instanced ? "instanceColor" : "vec4(1.0)") +
       ";\n  vFade = " + (o.instanced && o.fade ? "instanceFade" : "0.0") +
       ";\n  gl_Position = viewProj * " +
       (o.instanced ? "instanceModel" : "mat4(1.0)") +
       " * vec4(aPos, 1.0);\n}\n";
  return s;
}

inline std::string testFragmentSource(bool extras) {
  std::string s = "#version 460\n"
                  "#include \"frame_data.glsl\"\n"
                  "#include \"lod_fade.glsl\"\n"
                  "layout(location = 0) in vec2 vUV;\n"
                  "layout(location = 1) in vec4 vColor;\n"
                  "layout(location = 2) flat in float vFade;\n"
                  "layout(BIND(1, 1)) uniform sampler2D albedoMap;\n";
  if (extras) {
    s +=
        "layout(std140, BIND(1, 2)) uniform Settings { vec4 settingsValue; };\n"
        "layout(BIND(1, 3)) uniform sampler2D heightMap;\n"
        "layout(BIND(1, 4)) uniform sampler2D first;\n"
        "layout(BIND(1, 5)) uniform sampler2D second;\n"
        "layout(std430, BIND(1, 6)) readonly buffer Positions { vec4 "
        "positions[]; };\n";
  }
  s += "layout(location = 0) out vec4 outColor;\n"
       "void main() {\n"
       "  lodFadeDiscard(vFade);\n"
       "#ifdef COMPONENG_DEPTH_PREPASS\n"
       "  return;\n"
       "#endif\n"
       "  vec4 c = texture(albedoMap, vUV) * vColor * vec4(lightColor, 1.0);\n";
  if (extras) {
    s += "  c += texture(heightMap, vUV) + texture(first, vUV) + "
         "texture(second, vUV) + settingsValue + positions[0];\n";
  }
  s += "  outColor = c;\n}\n";
  return s;
}

// The id the fake gave shader `name`'s modules, for tests that compare draws
// against a variant of a particular shader.
inline int fragmentModuleId(const Shader &shader, Shader::Variant v) {
  return static_cast<fakerhi::FakeShaderModule *>(shader.program(v).fragment)
      ->id();
}

// A tiny triangle (3 indices) and quad (6 indices) so the two meshes are
// distinguishable by index count.
inline const char *kTriangleObj =
    "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\nf 1/1/1 2/1/1 3/1/1\n";
inline const char *kQuadObj =
    "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\n"
    "f 1/1/1 2/1/1 3/1/1 4/1/1\n";

struct Scene {
  std::unique_ptr<World> world;
  fakerhi::FakeDevice *device = nullptr; // owned by the world
  EntityID camera = INVALID_ENTITY;

  fakerhi::Recorder &rec() {
    return device->rec;
  }
  AssetManager &assets() {
    return world->getResource<AssetManager>();
  }

  // One frame as the window loop runs it: open, update every system, present.
  void frame() {
    device->beginFrame();
    world->updateSystems(0.016f);
    device->present(nullptr);
  }

  // Loads one of the engine's own full-screen shaders under `name`.
  void loadBundled(const char *vertexFile, const char *fragmentFile,
                   const char *name) {
    assets().loadShader(
        utils::Utils::getAssetPath(std::string("assets/shaders/") + vertexFile)
            .c_str(),
        utils::Utils::getAssetPath(std::string("assets/shaders/") +
                                   fragmentFile)
            .c_str(),
        nullptr, name);
  }
  void loadSky(const char *name = "skybox") {
    loadBundled("skybox_procedural.vert", "skybox_procedural.frag", name);
  }
  void loadPostProcess() {
    loadBundled("post_process.vert", "post_process.frag", "post_process");
  }

  EntityID spawn(const char *mesh, const char *material, float x = 0.0f) {
    EntityID e = world->createEntity();
    world->addComponents(e,
                         TransformComponent{.position = Vec3(x, 0.0f, 0.0f),
                                            .rotation = Vec3(0.0f),
                                            .scale = Vec3(1.0f)},
                         MeshComponent{.meshName = mesh, .visible = true},
                         MaterialComponent{.materialName = material});
    return e;
  }
};

// shaders: "a" = shader 1, "b" = shader 2 (see FakeDevice's Draw::shaderId).
// textures: "t1" = image 1, "t2" = image 2. meshes: "tri" (3 indices), "quad"
// (6). materials: matA = shader a + t1, matB = shader b + t2, matC = shader a
// + t2.
inline std::unique_ptr<Scene>
makeScene(const std::function<void(SceneOptions &)> &configure = {},
          bool withCulling = false) {
  SceneOptions options;
  if (configure) {
    configure(options);
  }
  auto scene = std::make_unique<Scene>();
  scene->world = std::make_unique<World>();
  World &world = *scene->world;

  auto device = std::make_unique<fakerhi::FakeDevice>();
  scene->device = device.get();
  std::unique_ptr<rhi::IDevice> base = std::move(device);
  world.setResource<std::unique_ptr<rhi::IDevice>>(std::move(base));
  world.setResource<AssetManager>(AssetManager(world.getRenderDevice()));
  world.setResource<resources::Fog>(resources::Fog{});
  // Off by default so a test sees each batch drawn once; the depth pre-pass
  // tests turn it on.
  world.setResource<resources::RenderSettings>(
      resources::RenderSettings{.depthPrepass = false});

  world.registerComponents<TransformComponent, MeshComponent, MaterialComponent,
                           ColorComponent, camera::CameraComponent,
                           DirectionalLightComponent, SkyboxComponent>();

  AssetManager &assets = scene->assets();
  // Real shaders, compiled by the real compiler; the fake device only sees
  // their SPIR-V.
  assets.compileShader("a", testVertexSource(options),
                       testFragmentSource(options.extras));
  assets.compileShader("b", testVertexSource(options),
                       testFragmentSource(false));
  const std::string white =
      utils::Utils::getAssetPath("assets/textures/white.png");
  assets.loadTexture(white.c_str(), false, "t1");
  assets.loadTexture(white.c_str(), false, "t2");

  for (auto [name, obj] :
       {std::pair{"tri", kTriangleObj}, std::pair{"quad", kQuadObj}}) {
    auto mesh = std::make_unique<Mesh>();
    mesh->generateFromWavefrontObj(obj);
    assets.addMesh(name, std::move(mesh));
  }

  assets.registerMaterial<DiffuseMaterial>("matA", "a", "t1");
  assets.registerMaterial<DiffuseMaterial>("matB", "b", "t2");
  assets.registerMaterial<DiffuseMaterial>("matC", "a", "t2");

  // Camera with an easily recognised view-projection matrix.
  scene->camera = world.createEntity();
  camera::CameraComponent cam;
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r)
      cam.viewProjectionMatrix[c][r] = float(c * 4 + r) + 0.5f;
  world.addComponents(scene->camera,
                      TransformComponent{.position = Vec3(1, 2, 3),
                                         .rotation = Vec3(0.0f),
                                         .scale = Vec3(1.0f)},
                      camera::CameraComponent(cam));
  world.setResource(camera::MainCamera{scene->camera});

  EntityID sun = world.createEntity();
  world.addComponents(
      sun, DirectionalLightComponent{.direction = Vec3(0.0f, 1.0f, 0.0f),
                                     .color = Vec3(0.5f, 0.25f, 0.125f)});

  if (withCulling) {
    world.registerSystem<CullingSystem>(SystemGroup::Presentation);
  }
  world.registerSystem<BatchingSystem>(SystemGroup::Presentation);
  if (withCulling) {
    world.addSystemDependencies<BatchingSystem, CullingSystem>();
  }
  world.registerSystem<RenderSystem>(SystemGroup::Presentation);
  world.addSystemDependencies<RenderSystem, BatchingSystem>();
  world.createSystems();
  return scene;
}

// 3 x (tri, matA), 2 x (quad, matA), 4 x (tri, matB), 1 x (tri, matC),
// deliberately interleaved.
inline void spawnMixed(Scene &s) {
  s.spawn("tri", "matB", 0);
  s.spawn("tri", "matA", 1);
  s.spawn("tri", "matC", 2);
  s.spawn("quad", "matA", 3);
  s.spawn("tri", "matB", 4);
  s.spawn("tri", "matA", 5);
  s.spawn("quad", "matA", 6);
  s.spawn("tri", "matB", 7);
  s.spawn("tri", "matA", 8);
  s.spawn("tri", "matB", 9);
}

} // namespace rscene
