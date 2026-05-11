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
  auto &audioEngine = state.world->get_resource<resources::AudioEngine>();

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

  auto view =
      state.world
          ->query<components::AudioComponent, components::TransformComponent>();

  view.each([&](components::AudioComponent &audio,
                components::TransformComponent &transform) {
    if (!audio.playOnAwake || audio.isPlaying) {
      return;
    }

    std::unique_ptr<ma_sound> sound =
        audioEngine.decodeSound(assetManager.getAudio(audio.audioID));

    if (audio.is3D) {
      audioEngine.setSoundPosition(sound.get(), transform.position.x,
                                   transform.position.y, transform.position.z);
      audioEngine.setSound3D(sound.get(), audio.minDistance, audio.maxDistance);
    }

    audioEngine.setSoundSettings(sound.get(), audio.volume, audio.pitch,
                                 audio.loop ? MA_TRUE : MA_FALSE);

    if (audioEngine.playSound(sound.get())) {
      audio.isPlaying = true;
    }

    m_activeSounds.push_back(std::move(sound));
  });

  for (size_t i = 0; i < m_activeSounds.size();) {
    auto &sound = m_activeSounds[i];

    if (!ma_sound_is_playing(sound.get())) {
      ma_sound_uninit(sound.get());

      m_activeSounds[i] = std::move(m_activeSounds.back());
      m_activeSounds.pop_back();
    } else {
      ++i;
    }
  }
}

} // namespace componeng::systems
