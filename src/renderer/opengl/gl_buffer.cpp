#include "core/logger.hpp"
#include "renderer/opengl/gl_buffer.hpp"

GLBuffer::GLBuffer() {
  glGenBuffers(1, &m_id);
}

GLBuffer::~GLBuffer() {
  release();
}

void GLBuffer::setData(const void* data, size_t sizeBytes) {
  m_size = sizeBytes;
  glBindBuffer(GL_ARRAY_BUFFER, m_id);
  glBufferData(GL_ARRAY_BUFFER, sizeBytes, data, GL_DYNAMIC_DRAW);
}

void GLBuffer::setSubData(size_t offset, const void* data, size_t sizeBytes) {
  glBindBuffer(GL_ARRAY_BUFFER, m_id);
  glBufferSubData(GL_ARRAY_BUFFER, offset, sizeBytes, data);
}

void GLBuffer::bind() {
  glBindBuffer(GL_ARRAY_BUFFER, m_id);
}

void GLBuffer::unbind() {
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLBuffer::release() {
  if (m_id != 0) {
    glDeleteBuffers(1, &m_id);
    m_id = 0;
  }
}
