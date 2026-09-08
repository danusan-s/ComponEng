#include "orbiting_sound.hpp"

#include "componeng/audio/audio_component.hpp"
#include "componeng/audio/audio_engine.hpp"
#include "componeng/camera/main_camera.hpp"
#include "componeng/core/transform_component.hpp"
#include "componeng/ecs/world.hpp"

void OrbitingSound::onUpdate(const componeng::ecs::SystemState &state) {
  auto mainCamEntity =
      state.world->getResource<componeng::camera::MainCamera>().entity;
  auto &camTransform =
      state.world->getComponent<componeng::core::TransformComponent>(
          mainCamEntity);
  auto &audioEngine = state.world->getResource<componeng::audio::AudioEngine>();

  double time = state.world->getTime();
  state.world
      ->query<componeng::core::TransformComponent,
              componeng::audio::AudioComponent>()
      .each([&](componeng::core::TransformComponent &transform,
                componeng::audio::AudioComponent &audio) {
        float radius = 40.0f;
        float speed = 1.0f;
        float heightRange = 10.0f;
        transform.position.x =
            camTransform.position.x + radius * cos(speed * time);
        transform.position.y =
            camTransform.position.y + heightRange * sin(speed * time * 5.0f);
        transform.position.z =
            camTransform.position.z + radius * sin(speed * time);
      });
}
