#include "componeng/renderer/asset_manager.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "componeng/utils/logger.hpp"
#include <fstream>
#include <sstream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace componeng::renderer {

void AssetManager::loadShader(const char *vShaderFile, const char *fShaderFile,
                              const char *gShaderFile, std::string name) {
  LOG_INFO("Loading Shader: %s", name.c_str());
  ShaderID id = m_nextShaderID++;
  m_shaders[name] = id;
  m_shaderResources[id] =
      loadShaderFromFile(vShaderFile, fShaderFile, gShaderFile);
}

const Shader &AssetManager::getShader(ShaderID id) const {
  return *m_shaderResources.at(id);
}

ShaderID AssetManager::getShaderID(std::string name) const {
  return m_shaders.at(name);
}

void AssetManager::loadTexture(const char *file, bool alpha, std::string name) {
  LOG_INFO("Loading Texture: %s", name.c_str());
  TextureID id = m_nextTextureID++;
  m_textures[name] = id;
  m_textureResources[id] = loadTextureFromFile(file, alpha);
}

const Texture2D &AssetManager::getTexture(TextureID id) const {
  return *m_textureResources.at(id);
}

TextureID AssetManager::getTextureID(std::string name) const {
  return m_textures.at(name);
}

bool AssetManager::textureExists(std::string name) const {
  return m_textures.find(name) != m_textures.end();
}

void AssetManager::addMesh(std::string name, std::unique_ptr<Mesh> mesh) {
  LOG_INFO("Adding Mesh: %s", name.c_str());
  mesh->uploadToGPU();
  MeshID id = m_nextMeshID++;
  m_meshes[name] = id;
  m_meshResources[id] = std::move(mesh);
}

void AssetManager::loadMesh(const char *file, std::string name) {
  LOG_INFO("Loading Model: %s", name.c_str());
  MeshID id = m_nextMeshID++;
  m_meshes[name] = id;
  m_meshResources[id] = loadMeshFromFile(file);
}

const Mesh &AssetManager::getMesh(MeshID id) const {
  return *m_meshResources.at(id);
}

MeshID AssetManager::getMeshID(std::string name) const {
  return m_meshes.at(name);
}

std::unique_ptr<Shader> AssetManager::loadShaderFromFile(
    const char *vShaderFile, const char *fShaderFile, const char *gShaderFile) {
  std::string vertexCode;
  std::string fragmentCode;
  std::string geometryCode;
  try {
    std::ifstream vertexShaderFile(vShaderFile);
    std::ifstream fragmentShaderFile(fShaderFile);
    std::stringstream vShaderStream, fShaderStream;
    vShaderStream << vertexShaderFile.rdbuf();
    fShaderStream << fragmentShaderFile.rdbuf();
    vertexShaderFile.close();
    fragmentShaderFile.close();
    vertexCode = vShaderStream.str();
    fragmentCode = fShaderStream.str();
    if (gShaderFile != nullptr) {
      std::ifstream geometryShaderFile(gShaderFile);
      std::stringstream gShaderStream;
      gShaderStream << geometryShaderFile.rdbuf();
      geometryShaderFile.close();
      geometryCode = gShaderStream.str();
    }
  } catch (const std::exception &e) {
    LOG_ERROR("ERROR::SHADER: Failed to read shader files");
  }
  auto shader = std::make_unique<Shader>();
  shader->compile(vertexCode.c_str(), fragmentCode.c_str(),
                  gShaderFile != nullptr ? geometryCode.c_str() : nullptr);
  return shader;
}

std::unique_ptr<Texture2D> AssetManager::loadTextureFromFile(const char *file,
                                                             bool alpha) {
  LOG_INFO("Loading texture file: %s", file);
  int width, height, nrChannels;
  unsigned char *data =
      stbi_load(file, &width, &height, &nrChannels, alpha ? 4 : 3);
  auto texture = std::make_unique<Texture2D>();
  texture->generate(width, height, data);
  stbi_image_free(data);
  return texture;
}

std::unique_ptr<Mesh> AssetManager::loadMeshFromFile(const char *file) {
  std::string modelData;
  try {
    std::ifstream modelWavefrontObjFile(file);
    std::stringstream wavefrontObjStream;
    wavefrontObjStream << modelWavefrontObjFile.rdbuf();
    modelWavefrontObjFile.close();
    modelData = wavefrontObjStream.str();
  } catch (const std::exception &e) {
    LOG_ERROR("ERROR::MODEL: Failed to read model file");
  }

  auto mesh = std::make_unique<Mesh>();
  mesh->generateFromWavefrontObj(modelData);
  mesh->uploadToGPU();
  return mesh;
}

void AssetManager::setAudioEngine(core::AudioEngine &audioEngine) {
  m_audioEngine = &audioEngine.getEngine();
  LOG_INFO("Audio engine initialized");
}

void AssetManager::loadAudio(const char *file, std::string name) {
  LOG_INFO("Loading Audio: %s", name.c_str());

  auto sound = std::make_unique<ma_sound>();
  ma_result result = ma_sound_init_from_file(m_audioEngine, file, 0, nullptr,
                                             nullptr, sound.get());
  if (result != MA_SUCCESS) {
    LOG_ERROR("Failed to load audio file: %s", file);
    return;
  }

  AudioID id = m_nextAudioID++;
  m_audioClips[name] = id;
  m_audioResources[id] = std::move(sound);
  LOG_INFO("Audio loaded successfully: %s (ID: %u)", name.c_str(), id);
}

AudioID AssetManager::getAudioID(std::string name) const {
  return m_audioClips.at(name);
}

ma_sound *AssetManager::getAudio(AudioID id) const {
  return m_audioResources.at(id).get();
}

void AssetManager::clear() {
  LOG_INFO("Deleting loaded resources");

  for (auto &pair : m_audioResources) {
    ma_sound_uninit(pair.second.get());
  }
  m_audioResources.clear();
  m_audioClips.clear();
  m_shaders.clear();
  m_shaderResources.clear();
  m_textures.clear();
  m_textureResources.clear();
  m_meshes.clear();
  m_meshResources.clear();
}

} // namespace componeng::renderer
