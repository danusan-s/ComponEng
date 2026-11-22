#pragma once

#include "componeng/renderer/backend/api/rhi/resources.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace componeng::renderer::shader {

enum class ValueType : uint8_t {
  Unknown,
  Float,
  Vec2,
  Vec3,
  Vec4,
  Int,
  IVec2,
  IVec3,
  IVec4,
  UInt,
  Mat3,
  Mat4,
};

/** One member of a uniform or storage block. Members of nested structs are
 *  flattened with dotted names. */
struct BlockMember {
  std::string name;
  ValueType type = ValueType::Unknown;
  uint32_t offset = 0; // bytes from the start of the block
  uint32_t arraySize = 1;
  uint32_t arrayStride = 0;

  bool operator==(const BlockMember &o) const {
    return name == o.name && type == o.type && offset == o.offset &&
           arraySize == o.arraySize && arrayStride == o.arrayStride;
  }
};

enum class ResourceKind : uint8_t { UniformBlock, StorageBlock, Sampler2D };

struct ReflectedResource {
  std::string name; // the block's type name, or the sampler's name
  ResourceKind kind = ResourceKind::UniformBlock;
  // The logical set and binding (set 0 to 1, binding 0 to 7), whatever the
  // target compiled to; see BIND() in the prelude.
  uint32_t set = 0;
  uint32_t binding = 0;
  uint32_t sizeBytes = 0; // blocks only; uniform blocks rounded up to 16
  rhi::StageFlags stages = rhi::StageFlags::None;
  std::vector<BlockMember> members;

  const BlockMember *member(const std::string &memberName) const;

  bool sameDeclaration(const ReflectedResource &o) const {
    return name == o.name && kind == o.kind && set == o.set &&
           binding == o.binding && sizeBytes == o.sizeBytes &&
           members == o.members;
  }
};

struct VertexInput {
  std::string name;
  uint32_t location = 0;
  ValueType type = ValueType::Unknown;
};

/** What a shader declares, as found by the compiler. Resources are sorted by
 *  (set, binding). */
struct Reflection {
  std::vector<ReflectedResource> resources;
  std::vector<VertexInput> vertexInputs; // vertex stage only

  const ReflectedResource *find(const std::string &name) const;
};

/** Combines the reflections of a program's stages. A resource declared in two
 *  stages must be identical in both (name, set, binding, size, members);
 *  otherwise false with `error` filled in. Stage flags are or-ed. */
bool mergeReflection(const std::vector<const Reflection *> &stages,
                     Reflection &out, std::string &error);

/** The bind group layout of `set` for a merged reflection. Every binding is
 *  visible to all graphics stages, so one group serves every variant of a
 *  shader. */
rhi::BindGroupLayoutDesc setLayout(const Reflection &merged, uint32_t set);

} // namespace componeng::renderer::shader
