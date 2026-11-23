#include "componeng/renderer/draw/material_bindings.hpp"

#include "componeng/renderer/asset/material.hpp"
#include "componeng/renderer/asset/shader.hpp"
#include "componeng/renderer/asset/texture.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/utils/logger.hpp"

namespace componeng::renderer {

namespace {

constexpr uint64_t kZeroBufferBytes = 16384;

}

MaterialBindings::MaterialBindings() = default;
MaterialBindings::~MaterialBindings() = default;

void MaterialBindings::clear() {
  m_groups.clear();
}

void MaterialBindings::report(const std::string &key,
                              const std::string &message) {
  if (m_reported.insert(key).second) {
    LOG_ERROR("%s", message.c_str());
  }
}

void MaterialBindings::ensureFallbacks(rhi::IDevice &device) {
  if (m_whiteImage) {
    return;
  }
  rhi::ImageDesc id;
  id.extent = {1, 1};
  id.format = rhi::Format::RGBA8;
  id.usage = rhi::ImageUsage::Sampled | rhi::ImageUsage::TransferDst;
  id.mipLevels = 1;
  m_whiteImage = device.createImage(id);
  const unsigned char white[4] = {255, 255, 255, 255};
  device.uploadImage(*m_whiteImage, {0, 0, 1, 1}, white);
  m_whiteSampler = device.createSampler({});
  m_zeroBuffer = device.createBuffer(
      {kZeroBufferBytes, rhi::BufferUsage::Uniform | rhi::BufferUsage::Storage,
       rhi::MemoryKind::Upload, "zeros"});
  const std::vector<unsigned char> zeros(kZeroBufferBytes, 0);
  m_zeroBuffer->write(0, zeros.data(), zeros.size());
}

rhi::IBindGroup *MaterialBindings::groupFor(rhi::IDevice &device,
                                            const AssetManager &assets,
                                            IMaterial &material,
                                            const Shader &shader) {
  if (!shader.valid()) {
    return nullptr;
  }
  material.params().flush();

  Entry &entry = m_groups[&material];
  if (entry.group && entry.version == material.bindingsVersion()) {
    return entry.group.get();
  }
  ensureFallbacks(device);

  const auto findBuffer =
      [](const std::vector<IMaterial::BufferBinding> &list,
         const std::string &block) -> const IMaterial::BufferBinding * {
    for (const auto &b : list) {
      if (b.block == block) {
        return &b;
      }
    }
    return nullptr;
  };
  const auto findTexture =
      [&](const std::string &sampler) -> const IMaterial::TextureBinding * {
    for (const auto &b : material.getTextureBindings()) {
      if (b.sampler == sampler) {
        return &b;
      }
    }
    return nullptr;
  };

  rhi::BindGroupDesc desc;
  desc.layout = shader.materialLayout();
  for (const shader::ReflectedResource &r : shader.reflection().resources) {
    if (r.set != 1) {
      continue;
    }
    rhi::BindGroupEntry e;
    e.binding = r.binding;
    switch (r.kind) {
      case shader::ResourceKind::UniformBlock: {
        if (r.binding == 0 && material.params().valid()) {
          e.buffer = material.params().buffer();
        } else if (const auto *b =
                       findBuffer(material.getUniformBlockBindings(), r.name)) {
          e.buffer = &assets.getBuffer(b->buffer);
        } else {
          report("block:" + r.name, "No buffer was given for uniform block '" +
                                        r.name + "'; it reads zeros");
          e.buffer = m_zeroBuffer.get();
        }
        break;
      }
      case shader::ResourceKind::StorageBlock: {
        if (const auto *b =
                findBuffer(material.getStorageBlockBindings(), r.name)) {
          e.buffer = &assets.getBuffer(b->buffer);
        } else {
          report("storage:" + r.name,
                 "No buffer was given for storage block '" + r.name +
                     "'; it reads zeros");
          e.buffer = m_zeroBuffer.get();
        }
        break;
      }
      case shader::ResourceKind::Sampler2D: {
        core::HandleID id = 0;
        if (const auto *b = findTexture(r.name)) {
          id = b->texture;
        } else if (r.name == "albedoMap") {
          id = material.getTextureID();
        } else if (r.name == "normalMap") {
          id = material.getNormalMapID();
        } else if (r.name == "metallicMap") {
          id = material.getMetallicMapID();
        } else if (r.name == "roughnessMap") {
          id = material.getRoughnessMapID();
        } else if (r.name == "aoMap") {
          id = material.getAOMapID();
        } else {
          report("sampler:" + r.name, "No texture was given for sampler '" +
                                          r.name + "'; it reads white");
        }
        // A PBR map the material has none of reads as the albedo.
        const bool pbrName = r.name == "normalMap" || r.name == "metallicMap" ||
                             r.name == "roughnessMap" || r.name == "aoMap";
        if (id == 0 && pbrName) {
          id = material.getTextureID();
        }
        if (id != 0) {
          const Texture2D &t = assets.getTexture(id);
          e.image = t.image();
          e.sampler = t.sampler();
        }
        if (!e.image) {
          e.image = m_whiteImage.get();
          e.sampler = m_whiteSampler.get();
        }
        break;
      }
    }
    desc.entries.push_back(e);
  }

  entry.group = device.createBindGroup(desc);
  entry.version = material.bindingsVersion();
  ++m_built;
  return entry.group.get();
}

} // namespace componeng::renderer
