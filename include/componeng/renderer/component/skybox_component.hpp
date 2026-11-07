#pragma once

#include "componeng/core/types.hpp"

namespace componeng::renderer {

/**
 * @brief Marks an entity as the skybox. The RenderSystem renders it before
 * the main scene with depth writing disabled so it serves as background.
 */
struct SkyboxComponent {
  static constexpr const char *component_name = "SkyboxComponent";
  // Tint applied to the procedural sky gradient.
  core::Vec3 color = core::Vec3(0.5f, 0.7f, 1.0f);
};

} // namespace componeng::renderer
