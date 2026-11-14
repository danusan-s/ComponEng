#pragma once

#include "componeng/ecs/entity.hpp"
#include "componeng/renderer/pipeline/pass_context.hpp"

namespace componeng::ecs {
class World;
}

namespace componeng::renderer {

/**
 * @brief Draws the sky: a full-screen triangle behind everything, from the
 * first SkyboxComponent in the world (none: draws nothing).
 *
 * Run it after the opaque geometry, not first: the sky shader is expensive,
 * and with the depth test on, the pixels the opaque geometry already covers
 * are rejected before it runs. It must still come before anything blended or
 * drawn without a depth test, which has to land on top of it.
 *
 * It binds its own shader, so a BatchDrawer's bound-state memory is stale
 * afterwards.
 */
void drawSkyPass(const PassContext &ctx, ecs::World &world,
                 ecs::EntityID camera);

} // namespace componeng::renderer
