#pragma once

#include <string>

namespace componeng::ecs {

class World;

/**
 * @brief Persists/restores a World's entities and components as JSON.
 *
 * Walks every archetype's live entities, serializing each component via its
 * registered ComponentInfo::serializer/deserializer (see ComponentRegistry),
 * and separately tracks which entity (if any) is the scene's main camera.
 */
class SceneSerializer {
public:
  static void save(World &world, const std::string &filename);
  static void load(World &world, const std::string &filename);
};

} // namespace componeng::ecs
