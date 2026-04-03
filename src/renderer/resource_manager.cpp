#include "core/logger.hpp"
#include "renderer/resource_manager.hpp"
#include <fstream>
#include <sstream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

std::unordered_map<std::string, std::unique_ptr<Shader>> ResourceManager::s_shaders;
std::unordered_map<std::string, std::unique_ptr<Texture2D>> ResourceManager::s_textures;
std::unordered_map<std::string, std::unique_ptr<Mesh>> ResourceManager::s_meshes;

void ResourceManager::loadShader(const char* vShaderFile,
                                 const char* fShaderFile,
                                 const char* gShaderFile, std::string name) {
  LOG_INFO("Loading Shader: %s", name.c_str());
  s_shaders[name] = loadShaderFromFile(vShaderFile, fShaderFile, gShaderFile);
}

const Shader& ResourceManager::getShader(std::string name) {
  return *s_shaders.at(name);
}

void ResourceManager::loadTexture(const char* file, bool alpha,
                                  std::string name) {
  LOG_INFO("Loading Texture: %s", name.c_str());
  s_textures[name] = loadTextureFromFile(file, alpha);
}

const Texture2D& ResourceManager::getTexture(std::string name) {
  return *s_textures.at(name);
}

bool ResourceManager::textureExists(std::string name) {
  return s_textures.find(name) != s_textures.end();
}

void ResourceManager::addMesh(std::string name, std::unique_ptr<Mesh> mesh) {
  LOG_INFO("Adding Mesh: %s", name.c_str());
  mesh->uploadToGPU();
  s_meshes[name] = std::move(mesh);
}

void ResourceManager::loadMesh(const char* file, std::string name) {
  LOG_INFO("Loading Model: %s", name.c_str());
  s_meshes[name] = loadMeshFromFile(file);
}

const Mesh& ResourceManager::getMesh(std::string name) {
  return *s_meshes.at(name);
}

void ResourceManager::clear() {
  LOG_INFO("Deleting loaded resources");
  s_shaders.clear();
  s_textures.clear();
  s_meshes.clear();
}

std::unique_ptr<Shader> ResourceManager::loadShaderFromFile(const char* vShaderFile,
                                                            const char* fShaderFile,
                                                            const char* gShaderFile) {
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
  } catch (const std::exception& e) {
    LOG_ERROR("ERROR::SHADER: Failed to read shader files");
  }
  auto shader = std::make_unique<Shader>();
  shader->compile(vertexCode.c_str(), fragmentCode.c_str(),
                  gShaderFile != nullptr ? geometryCode.c_str() : nullptr);
  return shader;
}

std::unique_ptr<Texture2D> ResourceManager::loadTextureFromFile(const char* file, bool alpha) {
  LOG_INFO("Loading texture file: %s", file);
  int width, height, nrChannels;
  unsigned char* data =
      stbi_load(file, &width, &height, &nrChannels, alpha ? 4 : 3);
  auto texture = std::make_unique<Texture2D>();
  texture->generate(width, height, data);
  stbi_image_free(data);
  return texture;
}

std::unique_ptr<Mesh> ResourceManager::loadMeshFromFile(const char* file) {
  std::string modelData;
  try {
    std::ifstream modelWavefrontObjFile(file);
    std::stringstream wavefrontObjStream;
    wavefrontObjStream << modelWavefrontObjFile.rdbuf();
    modelWavefrontObjFile.close();
    modelData = wavefrontObjStream.str();
  } catch (const std::exception& e) {
    LOG_ERROR("ERROR::MODEL: Failed to read model file");
  }

  auto mesh = std::make_unique<Mesh>();
  mesh->generateFromWavefrontObj(modelData);
  mesh->uploadToGPU();
  return mesh;
}
