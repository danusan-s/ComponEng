#include "collision_sounds.hpp"

#include "componeng/audio/audio_engine.hpp"
#include "componeng/camera/main_camera.hpp"
#include "componeng/core/transform_component.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/events/collision_event.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/utils/logger.hpp"

void CollisionSounds::onUpdate(const componeng::ecs::SystemState &state) {
  auto &collisionEvents =
      state.world->getEvents<componeng::events::CollisionEvent>();
  auto &assetManager =
      state.world->getResource<componeng::renderer::AssetManager>();
  auto &audioEngine = state.world->getResource<componeng::audio::AudioEngine>();

  auto soundPath = assetManager.getAudio(assetManager.getAudioID("boop"));

  auto &mainCamera = state.world->getResource<componeng::camera::MainCamera>();
  if (state.world->hasComponent<componeng::core::TransformComponent>(
          mainCamera.entity)) {
    auto &camTransform =
        state.world->getComponent<componeng::core::TransformComponent>(
            mainCamera.entity);
    audioEngine.setListenerPosition(camTransform.position.x,
                                    camTransform.position.y,
                                    camTransform.position.z);
  }

  for (const auto &event : collisionEvents) {
    // For simplicity, we just play a sound for every collision event.
    // In a real implementation, you would want to check the collision impulse,
    // materials involved, etc. to determine which sound to play and at what
    // volume.
    auto &a_transform =
        state.world->getComponent<componeng::core::TransformComponent>(
            event.entityA);
    auto position = a_transform.position -
                    event.info.normal * event.info.penetration * 0.5f;

    // LOG_INFO("Collision detected between Entity %d and Entity %d at position
    // "
    //          "(%.2f, %.2f, %.2f)",
    //          event.entityA, event.entityB, position.x, position.y,
    //          position.z);
  }
}
