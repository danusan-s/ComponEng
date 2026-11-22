#include "componeng/renderer/shader/reflection.hpp"

#include <algorithm>

namespace componeng::renderer::shader {

const BlockMember *ReflectedResource::member(const std::string &n) const {
  for (const BlockMember &m : members) {
    if (m.name == n) {
      return &m;
    }
  }
  return nullptr;
}

const ReflectedResource *Reflection::find(const std::string &n) const {
  for (const ReflectedResource &r : resources) {
    if (r.name == n) {
      return &r;
    }
  }
  return nullptr;
}

bool mergeReflection(const std::vector<const Reflection *> &stages,
                     Reflection &out, std::string &error) {
  out = Reflection{};
  for (const Reflection *stage : stages) {
    for (const ReflectedResource &r : stage->resources) {
      ReflectedResource *same = nullptr;
      for (ReflectedResource &have : out.resources) {
        const bool sameSlot = have.set == r.set && have.binding == r.binding;
        if (have.name == r.name || sameSlot) {
          if (!have.sameDeclaration(r)) {
            error = "'" + r.name + "' (set " + std::to_string(r.set) +
                    ", binding " + std::to_string(r.binding) +
                    ") is declared differently in two stages (or clashes "
                    "with '" +
                    have.name + "')";
            return false;
          }
          same = &have;
          break;
        }
      }
      if (same) {
        same->stages = same->stages | r.stages;
      } else {
        out.resources.push_back(r);
      }
    }
    if (!stage->vertexInputs.empty()) {
      out.vertexInputs = stage->vertexInputs;
    }
  }
  std::sort(out.resources.begin(), out.resources.end(),
            [](const ReflectedResource &a, const ReflectedResource &b) {
              return a.set != b.set ? a.set < b.set : a.binding < b.binding;
            });
  return true;
}

rhi::BindGroupLayoutDesc setLayout(const Reflection &merged, uint32_t set) {
  rhi::BindGroupLayoutDesc layout;
  for (const ReflectedResource &r : merged.resources) {
    if (r.set != set) {
      continue;
    }
    rhi::BindingDesc b;
    b.binding = r.binding;
    b.stages = rhi::StageFlags::AllGraphics;
    switch (r.kind) {
      case ResourceKind::UniformBlock:
        b.type = rhi::BindingType::UniformBuffer;
        break;
      case ResourceKind::StorageBlock:
        b.type = rhi::BindingType::StorageBuffer;
        break;
      case ResourceKind::Sampler2D:
        b.type = rhi::BindingType::CombinedImageSampler;
        break;
    }
    layout.bindings.push_back(b);
  }
  return layout;
}

} // namespace componeng::renderer::shader
