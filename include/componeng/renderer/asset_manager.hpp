#pragma once

#include "componeng/core/types.hpp"
#include "componeng/renderer/material.hpp"
#include "componeng/renderer/mesh.hpp"
#include "componeng/renderer/shader.hpp"
#include "componeng/renderer/texture.hpp"
#include "componeng/resources/audio_engine.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace componeng::renderer {

/**
 * @brief Asset manager for shaders, textures, and meshes.
 *
 * Loads assets from disk on first request, caches them by string handle,
 * and returns const references on subsequent lookups. All access is through
 * static methods — no public constructor exists.
 */
class AssetManager {
public:
  AssetManager() = default;
  ~AssetManager() = default;

  // Non-copyable, Movable
  AssetManager(const AssetManager &) = delete;
  AssetManager &operator=(const AssetManager &) = delete;

  AssetManager(AssetManager &&) = default;
  AssetManager &operator=(AssetManager &&) = default;

  resources::AudioEngine *m_audioEngine = nullptr;

  void loadShader(const char *vShaderFile, const char *fShaderFile,
                  const char *gShaderFile, std::string name);
  const Shader &getShader(core::HandleID id) const;
  core::HandleID getShaderID(std::string name) const;

  void loadTexture(const char *file, bool alpha, std::string name);
  const Texture2D &getTexture(core::HandleID id) const;
  core::HandleID getTextureID(std::string name) const;
  bool textureExists(std::string name) const;

  void registerMaterial(const char *name, const char *shaderName,
                        const char *textureName);
  core::HandleID getMaterialID(std::string name) const;
  Material &getMaterial(core::HandleID id) const;

  void addMesh(std::string name, std::unique_ptr<Mesh> mesh);
  void loadMesh(const char *file, std::string name);
  const Mesh &getMesh(core::HandleID id) const;
  core::HandleID getMeshID(std::string name) const;

  void loadAudio(const char *file, std::string name);
  core::HandleID getAudioID(std::string name) const;
  const char *getAudio(core::HandleID id) const;
  void clear();

private:
  std::unordered_map<std::string, core::HandleID> m_shaders;
  std::unordered_map<std::string, core::HandleID> m_textures;
  std::unordered_map<std::string, core::HandleID> m_materials;
  std::unordered_map<std::string, core::HandleID> m_meshes;
  std::unordered_map<std::string, core::HandleID> m_audios;

  std::vector<std::unique_ptr<Shader>> m_shaderResources;
  std::vector<std::unique_ptr<Texture2D>> m_textureResources;
  std::vector<std::unique_ptr<Material>> m_materialResources;
  std::vector<std::unique_ptr<Mesh>> m_meshResources;
  std::vector<std::string> m_audioPaths;

  std::unique_ptr<Shader> loadShaderFromFile(const char *vShaderFile,
                                             const char *fShaderFile,
                                             const char *gShaderFile = nullptr);
  std::unique_ptr<Texture2D> loadTextureFromFile(const char *file, bool alpha);
  std::unique_ptr<Mesh> loadMeshFromFile(const char *file);
};

} // namespace componeng::renderer
