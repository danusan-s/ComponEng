#include "componeng/renderer/asset_manager.hpp"

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
  core::HandleID id = m_nextShaderID++;
  m_shaders[name] = id;
  m_shaderResources[id] =
      loadShaderFromFile(vShaderFile, fShaderFile, gShaderFile);
}

const Shader &AssetManager::getShader(core::HandleID id) const {
  return *m_shaderResources.at(id);
}

core::HandleID AssetManager::getShaderID(std::string name) const {
  if (m_shaders.find(name) == m_shaders.end()) {
    LOG_ERROR("Shader not found: %s", name.c_str());
    throw std::runtime_error("Shader not found");
  }
  return m_shaders.at(name);
}

void AssetManager::loadTexture(const char *file, bool alpha, std::string name) {
  LOG_INFO("Loading Texture: %s", name.c_str());
  core::HandleID id = m_nextTextureID++;
  m_textures[name] = id;
  m_textureResources[id] = loadTextureFromFile(file, alpha);
}

const Texture2D &AssetManager::getTexture(core::HandleID id) const {
  return *m_textureResources.at(id);
}

core::HandleID AssetManager::getTextureID(std::string name) const {
  if (m_textures.find(name) == m_textures.end()) {
    LOG_ERROR("Texture not found: %s", name.c_str());
    throw std::runtime_error("Texture not found");
  }
  return m_textures.at(name);
}

bool AssetManager::textureExists(std::string name) const {
  return m_textures.find(name) != m_textures.end();
}

void AssetManager::addMesh(std::string name, std::unique_ptr<Mesh> mesh) {
  LOG_INFO("Adding Mesh: %s", name.c_str());
  mesh->uploadToGPU();
  core::HandleID id = m_nextMeshID++;
  m_meshes[name] = id;
  m_meshResources[id] = std::move(mesh);
}

void AssetManager::loadMesh(const char *file, std::string name) {
  LOG_INFO("Loading Model: %s", name.c_str());
  core::HandleID id = m_nextMeshID++;
  m_meshes[name] = id;
  m_meshResources[id] = loadMeshFromFile(file);
}

const Mesh &AssetManager::getMesh(core::HandleID id) const {
  return *m_meshResources.at(id);
}

core::HandleID AssetManager::getMeshID(std::string name) const {
  if (m_meshes.find(name) == m_meshes.end()) {
    LOG_ERROR("Mesh not found: %s", name.c_str());
    throw std::runtime_error("Mesh not found");
  }
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

void AssetManager::loadAudio(const char *file, std::string name) {
  LOG_INFO("Loading Audio: %s", name.c_str());

  core::HandleID id = m_nextAudioID++;
  m_audios[name] = id;
  m_audioPaths[id] = file;
  LOG_INFO("Audio loaded successfully: %s (ID: %u)", name.c_str(), id);
}

core::HandleID AssetManager::getAudioID(std::string name) const {
  return m_audios.at(name);
}

const char *AssetManager::getAudio(core::HandleID id) const {
  return m_audioPaths.at(id).c_str();
}

void AssetManager::clear() {
  LOG_INFO("Deleting loaded resources");

  m_audios.clear();
  m_audioPaths.clear();
  m_shaders.clear();
  m_shaderResources.clear();
  m_textures.clear();
  m_textureResources.clear();
  m_meshes.clear();
  m_meshResources.clear();
}

} // namespace componeng::renderer
