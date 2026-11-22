#pragma once

#include "componeng/renderer/backend/api/rhi/device.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace componeng::renderer {

class AssetManager;
class IMaterial;
class Shader;

/**
 * @brief Builds and keeps the set-1 bind group of each material: its
 * parameter block, its albedo and PBR textures, and the extra textures and
 * buffers it declared (IMaterial::setTexture and friends).
 *
 * A group is built the first time a material is drawn and rebuilt when
 * IMaterial::bindingsVersion changes. A sampler or block the material gives
 * nothing for is reported once and gets a white texture or a zeroed buffer,
 * so the draw still happens.
 */
class MaterialBindings {
public:
  MaterialBindings();
  ~MaterialBindings();

  /** The group for `material` drawn with `shader` (whose layout it matches).
   *  Flushes the material's parameters first. Null if the shader is not
   *  valid or a resource could not be bound. */
  rhi::IBindGroup *groupFor(rhi::IDevice &device, const AssetManager &assets,
                            IMaterial &material, const Shader &shader);

  /** Drops every group; call before the assets they point into go. */
  void clear();

  /** Number of groups built so far, for tests. */
  std::size_t built() const {
    return m_built;
  }

private:
  struct Entry {
    std::unique_ptr<rhi::IBindGroup> group;
    uint64_t version = 0;
  };

  void ensureFallbacks(rhi::IDevice &device);
  void report(const std::string &key, const std::string &message);

  std::unordered_map<const IMaterial *, Entry> m_groups;
  std::unordered_set<std::string> m_reported;
  std::unique_ptr<rhi::IImage> m_whiteImage;
  std::unique_ptr<rhi::ISampler> m_whiteSampler;
  std::unique_ptr<rhi::IBuffer> m_zeroBuffer;
  std::size_t m_built = 0;
};

} // namespace componeng::renderer
