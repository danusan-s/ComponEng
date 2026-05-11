#include "collision_sounds.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/events/collision_event.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/resources/audio_engine.hpp"
#include "componeng/resources/main_camera.hpp"
#include "componeng/utils/logger.hpp"

void CollisionSounds::onUpdate(const componeng::ecs::SystemState &state) {
  auto &collisionEvents =
      state.world->get_events<componeng::events::CollisionEvent>();
  auto &assetManager =
      state.world->get_resource<componeng::renderer::AssetManager>();
  auto &audioEngine =
      state.world->get_resource<componeng::resources::AudioEngine>();

  auto soundDataSource = assetManager.getAudio(assetManager.getAudioID("boop"));

  auto &mainCamera =
      state.world->get_resource<componeng::resources::MainCamera>();
  if (state.world->hasComponent<componeng::components::TransformComponent>(
          mainCamera.entity)) {
    auto &camTransform =
        state.world->getComponent<componeng::components::TransformComponent>(
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
        state.world->getComponent<componeng::components::TransformComponent>(
            event.entityA);
    auto position = a_transform.position -
                    event.info.normal * event.info.penetration * 0.5f;

    audioEngine.playSoundFromDataSource(soundDataSource, position.x, position.y,
                                        position.z, 0.8f, 1.0f, false, 1.0f,
                                        300.0f);

    LOG_INFO("Collision detected between Entity %d and Entity %d at position "
             "(%.2f, %.2f, %.2f)",
             event.entityA, event.entityB, position.x, position.y, position.z);
  }
}
