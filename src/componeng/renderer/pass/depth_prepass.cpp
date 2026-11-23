#include "componeng/renderer/pass/depth_prepass.hpp"

#include "componeng/renderer/batching/render_queue.hpp"
#include "componeng/renderer/draw/batch_drawer.hpp"

namespace componeng::renderer {

void DepthPrepass::execute(const PassContext &ctx) {
  BatchDrawer drawer(ctx);
  for (std::size_t i = 0; i < ctx.queue.opaqueCount(); ++i) {
    const RenderBatch &batch = ctx.queue.sorted(i);
    if (!batch.depthPrepass) {
      continue;
    }
    drawer.draw(batch, this->id());
  }
}

} // namespace componeng::renderer
