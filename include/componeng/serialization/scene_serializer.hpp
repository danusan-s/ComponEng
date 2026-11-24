#pragma once

#include <string>

namespace componeng::ecs {
class World;
}

namespace componeng::serialization {

class SceneSerializer {
public:
  static bool save(ecs::World &world, const std::string &filename);
  static bool load(ecs::World &world, const std::string &filename);
};

} // namespace componeng::serialization
