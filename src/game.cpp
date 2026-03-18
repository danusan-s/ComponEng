#include "game.hpp"
#include "resource_manager.hpp"
#include "utils.hpp"
#include <GLFW/glfw3.h>
#include <random>

#include "components/material.hpp"
#include "components/mesh.hpp"
#include "components/transform.hpp"
#include "systems/render_system.hpp"

void Game::Init() {
  world.Init();

  ResourceManager::LoadShader(
      Utils::GetAssetPath("shaders/diffuse.vert").c_str(),
      Utils::GetAssetPath("shaders/diffuse.frag").c_str(), nullptr, "default");

  ResourceManager::LoadModel(Utils::GetAssetPath("models/cube.obj").c_str(),
                             "cube");

  world.RegisterComponent<Transform>();
  world.RegisterComponent<Mesh>();
  world.RegisterComponent<Material>();
}

void InitObjects(World &world) {
  std::vector<EntityID> entities(MAX_ENTITIES);

  std::default_random_engine generator;
  std::uniform_real_distribution<float> randPosition(-100.0f, 100.0f);
  std::uniform_real_distribution<float> randRotation(0.0f, 3.0f);
  std::uniform_real_distribution<float> randScale(3.0f, 5.0f);
  std::uniform_real_distribution<float> randGravity(-10.0f, -1.0f);

  float scale = randScale(generator);

  const Model &cubeModel = ResourceManager::GetModel("cube");
  for (auto &entity : entities) {
    entity = world.CreateEntity();

    world.AddComponent(
        entity,
        Mesh{.VAO = cubeModel.VAO, .VBO = cubeModel.VBO, .EBO = cubeModel.EBO});

    gCoordinator.AddComponent(
        entity, RigidBody{.velocity = Vec3(0.0f, 0.0f, 0.0f),
                          .acceleration = Vec3(0.0f, 0.0f, 0.0f)});

    gCoordinator.AddComponent(
        entity,
        Transform{
            .position = Vec3(randPosition(generator), randPosition(generator),
                             randPosition(generator)),
            .rotation = Vec3(randRotation(generator), randRotation(generator),
                             randRotation(generator)),
            .scale = Vec3(scale, scale, scale)});
  }
}

void Game::Run() {
  // deltaTime variables
  // -------------------
  float deltaTime = 0.0f;
  float lastFrame = 0.0f;
  float accumulatedTime = 0.0f;

  //  240 fps simulation
  const float timeStep = 1 / 60.0f;

  auto window = glfwGetCurrentContext();
  auto renderSystem = world.RegisterSystem<OpenGLRenderSystem>();
  Signature renderSignature;
  renderSignature.set(world.GetComponentType<Transform>());
  renderSignature.set(world.GetComponentType<Mesh>());
  renderSignature.set(world.GetComponentType<Material>());
  world.SetSystemSignature<OpenGLRenderSystem>(renderSignature);

  while (!glfwWindowShouldClose(window)) {
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    accumulatedTime += deltaTime;

    while (accumulatedTime >= timeStep) {
      // Update game logic here (e.g., physics, input handling)
      accumulatedTime -= timeStep;
    }

    // Render the scene
    renderSystem->Render();

    // Swap buffers and poll events
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
}
