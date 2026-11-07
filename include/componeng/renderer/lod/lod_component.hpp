#pragma once

#include "componeng/core/types.hpp"

namespace componeng::renderer {

/**
 * @brief Distance-based level of detail for an entity's mesh.
 *
 * LodSystem picks the first level whose mesh exists and whose range still
 * covers the entity's distance to the main camera, writes it to
 * MeshComponent::meshID, and hides the entity (MeshComponent::visible = false)
 * once it is beyond every level. Give the entity a MeshComponent as usual;
 * its meshID is overwritten by the system.
 *
 * `mesh[k]` is a mesh handle from AssetManager::getMeshID, or 0 for "no mesh
 * at this level" (e.g. small props that simply disappear far away, or parts
 * that only exist up close). `distance[k]` is the range of level k in metres;
 * level 0 is the most detailed. Levels must be listed nearest first.
 *
 * Entities that make up one object (a tree's trunk and leaves) switch
 * together as long as they share a position and ranges.
 */
struct LodComponent {
  static constexpr const char *component_name = "LodComponent";
  static constexpr int kMaxLevels = 3;

  core::HandleID mesh[kMaxLevels] = {0, 0, 0};
  float distance[kMaxLevels] = {0.0f, 0.0f, 0.0f};

  // Runtime: the level chosen last time, so unchanged entities cost one
  // compare.
  int level = -1;
};

} // namespace componeng::renderer
