#include "componeng/renderer/asset/parameter_block.hpp"

#include <cstring>
#include <type_traits>
#include <variant>

namespace componeng::renderer {

void ParameterBlock::init(rhi::IDevice &device,
                          const shader::ReflectedResource &block) {
  m_block = block;
  m_shadow.assign(block.sizeBytes, 0);
  m_buffer = device.createBuffer({block.sizeBytes, rhi::BufferUsage::Uniform,
                                  rhi::MemoryKind::Upload, "parameters"});
  m_dirty = true;
}

ParameterBlock::Result ParameterBlock::set(const std::string &name,
                                           const UniformValue &value) {
  const shader::BlockMember *member = m_block.member(name);
  if (!member) {
    return Result::UnknownName;
  }
  using shader::ValueType;
  const void *src = nullptr;
  size_t bytes = 0;
  ValueType want = ValueType::Unknown;
  std::visit(
      [&](auto &&v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, float>) {
          want = ValueType::Float;
          src = &v;
          bytes = sizeof(float);
        } else if constexpr (std::is_same_v<T, int>) {
          want = ValueType::Int;
          src = &v;
          bytes = sizeof(int);
        } else if constexpr (std::is_same_v<T, core::Vec2>) {
          want = ValueType::Vec2;
          src = core::value_ptr(v);
          bytes = 2 * sizeof(float);
        } else if constexpr (std::is_same_v<T, core::Vec3>) {
          want = ValueType::Vec3;
          src = core::value_ptr(v);
          bytes = 3 * sizeof(float);
        } else if constexpr (std::is_same_v<T, core::Vec4>) {
          want = ValueType::Vec4;
          src = core::value_ptr(v);
          bytes = 4 * sizeof(float);
        } else if constexpr (std::is_same_v<T, core::Mat4>) {
          want = ValueType::Mat4;
          src = core::value_ptr(v);
          bytes = 16 * sizeof(float);
        }
      },
      value);
  if (member->type != want || member->arraySize != 1 ||
      member->offset + bytes > m_shadow.size()) {
    return Result::TypeMismatch;
  }
  if (std::memcmp(m_shadow.data() + member->offset, src, bytes) != 0) {
    std::memcpy(m_shadow.data() + member->offset, src, bytes);
    m_dirty = true;
  }
  return Result::Ok;
}

void ParameterBlock::flush() {
  if (m_dirty && m_buffer) {
    m_buffer->write(0, m_shadow.data(), m_shadow.size());
    m_dirty = false;
  }
}

} // namespace componeng::renderer
