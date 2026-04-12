#include "renderer/resource_manager.hpp"
#include "core/logger.hpp"
#include <fstream>
#include <sstream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

std::unordered_map<std::string, ShaderID> ResourceManager::s_shaders;
std::unordered_map<std::string, TextureID> ResourceManager::s_textures;
std::unordered_map<std::string, MeshID> ResourceManager::s_meshes;

std::unordered_map<ShaderID, std::unique_ptr<Shader>>
    ResourceManager::s_shaderResources;
std::unordered_map<TextureID, std::unique_ptr<Texture2D>>
    ResourceManager::s_textureResources;
std::unordered_map<MeshID, std::unique_ptr<Mesh>>
    ResourceManager::s_meshResources;

uint32_t ResourceManager::nextShaderID = 1;
uint32_t ResourceManager::nextTextureID = 1;
uint32_t ResourceManager::nextMeshID = 1;

void ResourceManager::loadShader(const char *vShaderFile,
                                 const char *fShaderFile,
                                 const char *gShaderFile, std::string name) {
  LOG_INFO("Loading Shader: %s", name.c_str());
  ShaderID id = nextShaderID++;
  s_shaders[name] = id;
  s_shaderResources[id] =
      loadShaderFromFile(vShaderFile, fShaderFile, gShaderFile);
}

const Shader &ResourceManager::getShader(ShaderID id) {
  return *s_shaderResources.at(id);
}

ShaderID ResourceManager::getShaderID(std::string name) {
  return s_shaders.at(name);
}

void ResourceManager::loadTexture(const char *file, bool alpha,
                                  std::string name) {
  LOG_INFO("Loading Texture: %s", name.c_str());
  TextureID id = nextTextureID++;
  s_textures[name] = id;
  s_textureResources[id] = loadTextureFromFile(file, alpha);
}

const Texture2D &ResourceManager::getTexture(TextureID id) {
  return *s_textureResources.at(id);
}

TextureID ResourceManager::getTextureID(std::string name) {
  return s_textures.at(name);
}

bool ResourceManager::textureExists(std::string name) {
  return s_textures.find(name) != s_textures.end();
}

void ResourceManager::addMesh(std::string name, std::unique_ptr<Mesh> mesh) {
  LOG_INFO("Adding Mesh: %s", name.c_str());
  mesh->uploadToGPU();
  MeshID id = nextMeshID++;
  s_meshes[name] = id;
  s_meshResources[id] = std::move(mesh);
}

void ResourceManager::loadMesh(const char *file, std::string name) {
  LOG_INFO("Loading Model: %s", name.c_str());
  MeshID id = nextMeshID++;
  s_meshes[name] = id;
  s_meshResources[id] = loadMeshFromFile(file);
}

const Mesh &ResourceManager::getMesh(MeshID id) {
  return *s_meshResources.at(id);
}

MeshID ResourceManager::getMeshID(std::string name) {
  return s_meshes.at(name);
}

void ResourceManager::clear() {
  LOG_INFO("Deleting loaded resources");
  s_shaders.clear();
  s_textures.clear();
  s_meshes.clear();
}

std::unique_ptr<Shader> ResourceManager::loadShaderFromFile(
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

std::unique_ptr<Texture2D>
ResourceManager::loadTextureFromFile(const char *file, bool alpha) {
  LOG_INFO("Loading texture file: %s", file);
  int width, height, nrChannels;
  unsigned char *data =
      stbi_load(file, &width, &height, &nrChannels, alpha ? 4 : 3);
  auto texture = std::make_unique<Texture2D>();
  texture->generate(width, height, data);
  stbi_image_free(data);
  return texture;
}

std::unique_ptr<Mesh> ResourceManager::loadMeshFromFile(const char *file) {
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
