#include "componeng/systems/audio_system.hpp"
#include "componeng/components/audio_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/resources/main_camera.hpp"

namespace componeng::systems {

void AudioSystem::onUpdate(const ecs::SystemState &state) {
  auto &assetManager = state.world->get_resource<renderer::AssetManager>();

  auto &mainCamera =
      state.world->get_resource<componeng::resources::MainCamera>();
  if (state.world->hasComponent<componeng::components::TransformComponent>(
          mainCamera.entity)) {
    auto &camTransform =
        state.world->getComponent<componeng::components::TransformComponent>(
            mainCamera.entity);
    ma_engine_listener_set_position(
        assetManager.m_audioEngine, 0, camTransform.position.x,
        camTransform.position.y, camTransform.position.z);
  }

  auto view =
      state.world
          ->query<components::AudioComponent, components::TransformComponent>();

  view.each([&](components::AudioComponent &audio,
                components::TransformComponent &transform) {
    ma_sound *sound = assetManager.getAudio(audio.audioID);

    if (audio.is3D) {
      ma_sound_set_position(sound, transform.position.x, transform.position.y,
                            transform.position.z);
      ma_sound_set_min_distance(sound, audio.minDistance);
      ma_sound_set_max_distance(sound, audio.maxDistance);
      ma_sound_set_attenuation_model(sound, ma_attenuation_model_linear);
    }

    ma_sound_set_volume(sound, audio.volume);
    ma_sound_set_pitch(sound, audio.pitch);
    ma_sound_set_looping(sound, audio.loop ? MA_TRUE : MA_FALSE);

    if (audio.playOnAwake && !audio.isPlaying) {
      ma_result result = ma_sound_start(sound);
      if (result == MA_SUCCESS) {
        audio.isPlaying = true;
      }
    }
  });
}

} // namespace componeng::systems
