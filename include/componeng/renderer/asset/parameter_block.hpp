#pragma once

#include "componeng/core/types.hpp"
#include "componeng/renderer/backend/api/rhi/device.hpp"
#include "componeng/renderer/shader/reflection.hpp"

#include <memory>
#include <string>
#include <vector>

namespace componeng::renderer {

using UniformValue = core::UniformMap::mapped_type;

/**
 * @brief The values of one uniform block, kept on the CPU and written to a
 * uniform buffer when they change.
 *
 * The layout comes from shader reflection, so a value is set by member name
 * and lands at the offset the shader expects. Used for a material's
 * parameters and for the sky and post-process passes' own.
 */
class ParameterBlock {
public:
  enum class Result { Ok, UnknownName, TypeMismatch };

  /** Sizes the shadow copy and creates the buffer from `block`. */
  void init(rhi::IDevice &device, const shader::ReflectedResource &block);

  bool valid() const {
    return m_buffer != nullptr;
  }

  /** Writes `value` into the shadow copy. The type must match the member
   *  exactly (float, int, Vec2, Vec3, Vec4 or Mat4); there is no conversion. */
  Result set(const std::string &name, const UniformValue &value);

  /** Uploads the shadow copy if anything changed since the last flush. */
  void flush();

  rhi::IBuffer *buffer() const {
    return m_buffer.get();
  }
  const std::vector<unsigned char> &bytes() const {
    return m_shadow;
  }
  const shader::ReflectedResource &layout() const {
    return m_block;
  }

private:
  shader::ReflectedResource m_block;
  std::vector<unsigned char> m_shadow;
  std::unique_ptr<rhi::IBuffer> m_buffer;
  bool m_dirty = false;
};

} // namespace componeng::renderer
