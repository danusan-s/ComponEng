#include "core/logger.hpp"
#include "renderer/resource_manager.hpp"
#include <fstream>
#include <sstream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

std::unordered_map<std::string, Texture2D> ResourceManager::s_textures;
std::unordered_map<std::string, Shader> ResourceManager::s_shaders;
std::unordered_map<std::string, Mesh> ResourceManager::s_meshes;

void ResourceManager::loadShader(const char* vShaderFile,
                                 const char* fShaderFile,
                                 const char* gShaderFile, std::string name) {
  LOG_INFO("Loading Shader: %s", name.c_str());
  s_shaders[name] = loadShaderFromFile(vShaderFile, fShaderFile, gShaderFile);
}

const Shader& ResourceManager::getShader(std::string name) {
  return s_shaders.at(name);
}

void ResourceManager::loadTexture(const char* file, bool alpha,
                                  std::string name) {
  LOG_INFO("Loading Texture: %s", name.c_str());
  s_textures[name] = loadTextureFromFile(file, alpha);
}

const Texture2D& ResourceManager::getTexture(std::string name) {
  return s_textures.at(name);
}

bool ResourceManager::textureExists(std::string name) {
  return s_textures.find(name) != s_textures.end();
}

void ResourceManager::addMesh(std::string name, Mesh& mesh) {
  LOG_INFO("Adding Mesh: %s", name.c_str());
  mesh.initializeBuffers();
  s_meshes[name] = mesh;
}

void ResourceManager::loadMesh(const char* file, std::string name) {
  LOG_INFO("Loading Model: %s", name.c_str());
  Mesh mesh = loadMeshFromFile(file);
  addMesh(name, mesh);
}

const Mesh& ResourceManager::getMesh(std::string name) {
  return s_meshes.at(name);
}

void ResourceManager::clear() {
  LOG_INFO("Deleting loaded resources");

  LOG_INFO("Attempting to delete shaders");
  for (auto& iter : s_shaders)
    glDeleteProgram(iter.second.m_id);
  LOG_INFO("Shaders deleted successfully");

  LOG_INFO("Attempting to delete textures");
  for (auto& iter : s_textures)
    glDeleteTextures(1, &iter.second.m_id);

  LOG_INFO("Textures deleted successfully");

  LOG_INFO("Attempting to delete models");
  for (auto& iter : s_meshes) {
    glDeleteVertexArrays(1, &iter.second.m_vao);
    glDeleteBuffers(1, &iter.second.m_vbo);
    glDeleteBuffers(1, &iter.second.m_ebo);
  }
  LOG_INFO("Models deleted successfully");
}

Shader ResourceManager::loadShaderFromFile(const char* vShaderFile,
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
  const char* vShaderCode = vertexCode.c_str();
  const char* fShaderCode = fragmentCode.c_str();
  const char* gShaderCode = geometryCode.c_str();
  Shader shader;
  shader.compile(vShaderCode, fShaderCode,
                 gShaderFile != nullptr ? gShaderCode : nullptr);
  return shader;
}

Texture2D ResourceManager::loadTextureFromFile(const char* file, bool alpha) {
  Texture2D texture;
  if (alpha) {
    texture.m_internalFormat = GL_RGBA;
    texture.m_imageFormat = GL_RGBA;
  }
  LOG_INFO("Loading texture file: %s", file);
  int width, height, nrChannels;
  unsigned char* data =
      stbi_load(file, &width, &height, &nrChannels, alpha ? 4 : 3);
  texture.generate(width, height, data);
  stbi_image_free(data);
  return texture;
}

Mesh ResourceManager::loadMeshFromFile(const char* file) {
  std::string modelData;
  try {
    std::ifstream modelWavefrontObjFile(file);
    std::stringstream wavefrontObjStream;
    wavefrontObjStream << modelWavefrontObjFile.rdbuf();
    modelWavefrontObjFile.close();
    modelData = wavefrontObjStream.str();
  } catch (const std::exception& e) {
    LOG_ERROR("ERROR::MODEL: Failed to read model files");
  }

  Mesh model;
  model.generateFromWavefrontObj(modelData);
  return model;
}
