#include "componeng/renderer/backend/opengl/gl_stub_device.hpp"

#include "componeng/utils/logger.hpp"

#include <cstdlib>

namespace componeng::renderer::opengl {

void GLStubDevice::init(void *) {
  LOG_ERROR("The OpenGL backend is being ported to the new renderer API and "
            "cannot run yet. Build with -DRENDERER_BACKEND=vulkan.");
  std::exit(1);
}

} // namespace componeng::renderer::opengl
