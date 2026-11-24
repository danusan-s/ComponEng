#include "componeng/renderer/asset_manager.hpp"

#include "componeng/core/types.hpp"
#include "componeng/utils/logger.hpp"
#include "componeng/utils/utils.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace componeng::renderer {

void AssetManager::compileShader(core::Name name,
                                 const std::string &vertexSource,
                                 const std::string &fragmentSource) {
  LOG_INFO("Queueing Shader: %s", name.c_str());
  m_pendingShaders.push_back(
      {name, vertexSource, fragmentSource, {}, {}, {}, false});
  m_shaders[name] = m_shaderResources.size() + m_pendingShaders.size();
}

void AssetManager::loadShader(const char *vShaderFile, const char *fShaderFile,
                              const char *gShaderFile, core::Name name) {
  LOG_INFO("Queueing Shader: %s", name.c_str());
  if (gShaderFile)
    LOG_ERROR("Geometry shaders are not supported; ignoring %s", gShaderFile);
  m_pendingShaders.push_back(
      {name, {}, {}, vShaderFile ? vShaderFile : "", fShaderFile ? fShaderFile : "",
       std::filesystem::path(vShaderFile ? vShaderFile : "").stem().string(), true});
  m_shaders[name] = m_shaderResources.size() + m_pendingShaders.size();
}

const Shader &AssetManager::getShader(core::HandleID id) const {
  ensureLoaded();
  if (id == 0 || id > m_shaderResources.size())
    throw std::runtime_error("Shader not found");
  return *m_shaderResources.at(id - 1);
}

core::HandleID AssetManager::getShaderID(core::Name name) const {
  ensureLoaded();
  auto it = m_shaders.find(name);
  if (it == m_shaders.end()) {
    LOG_ERROR("Shader not found: %s", name.c_str());
    throw std::runtime_error("Shader not found");
  }
  return it->second;
}

bool AssetManager::shaderExists(core::Name name) const {
  ensureLoaded();
  return m_shaders.find(name) != m_shaders.end();
}

void AssetManager::loadTexture(const char *file, bool alpha, core::Name name,
                               const api::TextureDesc &desc) {
  LOG_INFO("Queueing Texture: %s", name.c_str());
  m_pendingTextures.push_back(
      {name, file ? file : "", alpha, desc, 0, 0, {}});
  m_textures[name] = m_textureResources.size() + m_pendingTextures.size();
}

const Texture2D &AssetManager::getTexture(core::HandleID id) const {
  ensureLoaded();
  if (id == 0 || id > m_textureResources.size())
    throw std::runtime_error("Texture not found");
  return *m_textureResources.at(id - 1);
}

core::HandleID AssetManager::getTextureID(core::Name name) const {
  ensureLoaded();
  auto it = m_textures.find(name);
  if (it == m_textures.end()) {
    LOG_ERROR("Texture not found: %s", name.c_str());
    throw std::runtime_error("Texture not found");
  }
  return it->second;
}

bool AssetManager::textureExists(core::Name name) const {
  ensureLoaded();
  return m_textures.find(name) != m_textures.end();
}

void AssetManager::createTexture(core::Name name, uint32_t width,
                                 uint32_t height, api::TextureFormat format,
                                 const void *pixels,
                                 const api::TextureDesc &desc) {
  ensureLoaded();
  LOG_INFO("Creating Texture: %s (%ux%u)", name.c_str(), width, height);
  auto texture = std::make_unique<Texture2D>();
  texture->create(*m_device, width, height, format, pixels, desc);
  m_textureResources.push_back(std::move(texture));
  m_textures[name] = m_textureResources.size();
}

bool AssetManager::updateTexture(core::HandleID id, uint32_t x, uint32_t y,
                                 uint32_t width, uint32_t height,
                                 const void *pixels) {
  ensureLoaded();
  return m_textureResources.at(id - 1)->update(x, y, width, height, pixels);
}

void AssetManager::createBuffer(core::Name name, const void *data,
                                size_t sizeBytes) {
  ensureLoaded();
  LOG_INFO("Creating Buffer: %s (%zu bytes)", name.c_str(), sizeBytes);
  auto buffer = m_device->createBuffer(
      {sizeBytes, rhi::BufferUsage::Uniform | rhi::BufferUsage::Storage,
       rhi::MemoryKind::Upload, name.c_str()});
  if (data && sizeBytes)
    buffer->write(0, data, sizeBytes);
  m_bufferResources.push_back(std::move(buffer));
  m_buffers[name] = m_bufferResources.size();
}

bool AssetManager::updateBuffer(core::HandleID id, size_t offset,
                                const void *data, size_t sizeBytes) {
  rhi::IBuffer &buffer = *m_bufferResources.at(id - 1);
  if (offset > buffer.size() || sizeBytes > buffer.size() - offset)
    return false;
  buffer.write(offset, data, sizeBytes);
  return true;
}

rhi::IBuffer &AssetManager::getBuffer(core::HandleID id) const {
  ensureLoaded();
  return *m_bufferResources.at(id - 1);
}

core::HandleID AssetManager::getBufferID(core::Name name) const {
  ensureLoaded();
  auto it = m_buffers.find(name);
  if (it == m_buffers.end())
    throw std::runtime_error("Buffer not found");
  return it->second;
}

bool AssetManager::bufferExists(core::Name name) const {
  return m_buffers.find(name) != m_buffers.end();
}

core::HandleID AssetManager::getMaterialID(core::Name name) const {
  ensureLoaded();
  auto it = m_materials.find(name);
  if (it == m_materials.end())
    throw std::runtime_error("Material not found");
  return it->second;
}

IMaterial &AssetManager::getMaterial(core::HandleID id) const {
  ensureLoaded();
  return *m_materialResources.at(id - 1);
}

void AssetManager::addMesh(core::Name name, std::unique_ptr<Mesh> mesh) {
  LOG_INFO("Queueing Mesh: %s", name.c_str());
  m_pendingMeshes.push_back({name, {}, std::move(mesh)});
  m_meshes[name] = m_meshResources.size() + m_pendingMeshes.size();
}

void AssetManager::loadMesh(const char *file, core::Name name) {
  LOG_INFO("Queueing Model: %s", name.c_str());
  m_pendingMeshes.push_back({name, file ? file : "", nullptr});
  m_meshes[name] = m_meshResources.size() + m_pendingMeshes.size();
}

const Mesh &AssetManager::getMesh(core::HandleID id) const {
  ensureLoaded();
  if (id == 0 || id > m_meshResources.size())
    throw std::runtime_error("Mesh not found");
  return *m_meshResources.at(id - 1);
}

core::HandleID AssetManager::getMeshID(core::Name name) const {
  ensureLoaded();
  auto it = m_meshes.find(name);
  if (it == m_meshes.end())
    throw std::runtime_error("Mesh not found");
  return it->second;
}

void AssetManager::ensureLoaded() const {
  const_cast<AssetManager *>(this)->loadAll();
}

void AssetManager::loadAll(ecs::ThreadPool &pool) {
  m_threadPool = &pool;
  loadAll();
}

void AssetManager::loadAll() {
  if (m_pendingShaders.empty() && m_pendingTextures.empty() &&
      m_pendingMeshes.empty())
    return;

  if (!m_threadPool) {
    static ecs::ThreadPool pool;
    m_threadPool = &pool;
  }

  std::vector<std::future<void>> tasks;
  for (PendingShader &pending : m_pendingShaders) {
    if (!pending.fromFile)
      continue;
    tasks.push_back(m_threadPool->submit([&pending] {
      auto readFile = [](const std::string &path) {
        std::ifstream file(path);
        if (!file) {
          LOG_ERROR("Failed to read shader file: %s", path.c_str());
          return std::string{};
        }
        std::stringstream stream;
        stream << file.rdbuf();
        return stream.str();
      };
      pending.vertexSource = readFile(pending.vertexFile);
      pending.fragmentSource = readFile(pending.fragmentFile);
    }));
  }

  for (PendingTexture &pending : m_pendingTextures) {
    tasks.push_back(m_threadPool->submit([&pending] {
      unsigned char *data =
          stbi_load(pending.file.c_str(), &pending.width, &pending.height,
                    nullptr, pending.alpha ? 4 : 3);
      if (!data) {
        pending.width = pending.height = 1;
        pending.pixels = pending.alpha
                             ? std::vector<unsigned char>{255, 0, 255, 255}
                             : std::vector<unsigned char>{255, 0, 255};
        LOG_ERROR("Failed to load texture '%s': %s", pending.file.c_str(),
                  stbi_failure_reason());
        return;
      }
      pending.pixels.assign(data, data + pending.width * pending.height *
                                           (pending.alpha ? 4 : 3));
      stbi_image_free(data);
    }));
  }

  for (PendingMesh &pending : m_pendingMeshes) {
    if (!pending.file.empty()) {
      tasks.push_back(m_threadPool->submit([&pending] {
        std::ifstream file(pending.file);
        if (!file) {
          pending.mesh = std::make_unique<Mesh>();
          LOG_ERROR("Failed to read model file: %s", pending.file.c_str());
          return;
        }
        std::stringstream stream;
        stream << file.rdbuf();
        pending.mesh = std::make_unique<Mesh>();
        pending.mesh->generateFromWavefrontObj(stream.str());
      }));
    }
  }
  for (auto &task : tasks)
    task.get();

  for (PendingShader &pending : m_pendingShaders) {
    auto shader = std::make_unique<Shader>(*m_device);
    std::string name =
        pending.fromFile ? pending.nameFromFile : std::string(pending.name.c_str());
    shader->compile(pending.vertexSource, pending.fragmentSource, name,
                    {utils::Utils::getAssetPath("assets/shaders")});
    m_shaderResources.push_back(std::move(shader));
  }
  for (PendingTexture &pending : m_pendingTextures) {
    auto texture = std::make_unique<Texture2D>();
    texture->generate(*m_device, pending.width, pending.height,
                      pending.pixels.data(), pending.alpha, pending.desc);
    m_textureResources.push_back(std::move(texture));
  }
  for (PendingMesh &pending : m_pendingMeshes) {
    if (pending.mesh) {
      pending.mesh->uploadToGPU(*m_device);
      m_meshResources.push_back(std::move(pending.mesh));
    }
  }

  for (size_t i = 0; i < m_pendingShaders.size(); ++i)
    m_shaders[m_pendingShaders[i].name] = m_shaderResources.size() - m_pendingShaders.size() + i + 1;
  for (size_t i = 0; i < m_pendingTextures.size(); ++i)
    m_textures[m_pendingTextures[i].name] = m_textureResources.size() - m_pendingTextures.size() + i + 1;
  for (size_t i = 0; i < m_pendingMeshes.size(); ++i)
    m_meshes[m_pendingMeshes[i].name] = m_meshResources.size() - m_pendingMeshes.size() + i + 1;

  m_pendingShaders.clear();
  m_pendingTextures.clear();
  m_pendingMeshes.clear();
  LOG_INFO("Finished loading asset batch");
}

void AssetManager::clear() {
  m_pendingShaders.clear();
  m_pendingTextures.clear();
  m_pendingMeshes.clear();
  m_shaders.clear();
  m_shaderResources.clear();
  m_textures.clear();
  m_textureResources.clear();
  m_meshes.clear();
  m_meshResources.clear();
  m_buffers.clear();
  m_bufferResources.clear();
  m_materials.clear();
  m_materialResources.clear();
}

} // namespace componeng::renderer
