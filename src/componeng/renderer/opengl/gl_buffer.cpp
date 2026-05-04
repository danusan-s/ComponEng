#include "componeng/core/logger.hpp"
#include "componeng/renderer/opengl/gl_buffer.hpp"

GLBuffer::GLBuffer() {
  glGenBuffers(1, &m_id);
}

GLBuffer::~GLBuffer() {
  release();
}

void GLBuffer::setData(const void* data, size_t sizeBytes, IBuffer::Usage usage) {
  m_size = sizeBytes;
  glBindBuffer(GL_ARRAY_BUFFER, m_id);
  GLenum glUsage = (usage == IBuffer::Usage::Static) ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;
  glBufferData(GL_ARRAY_BUFFER, sizeBytes, data, glUsage);
}

void GLBuffer::setSubData(size_t offset, const void* data, size_t sizeBytes) {
  glBindBuffer(GL_ARRAY_BUFFER, m_id);
  glBufferSubData(GL_ARRAY_BUFFER, offset, sizeBytes, data);
}

void GLBuffer::release() {
  if (m_id != 0) {
    glDeleteBuffers(1, &m_id);
    m_id = 0;
  }
}
