/**
 * 
 *    \file VertexBuffer.cpp  
 * 
 */
 

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

#include "VertexBuffer.h"


    
void VertexBuffer::initialize()
{
  if (!m_vao) glGenVertexArrays( 1, &m_vao);
  glGenBuffers( 3, m_vbo);
  if (!m_ibo) glGenBuffers( 1, &m_ibo);
}

void VertexBuffer::destroy()
{
  if (m_vao) glDeleteVertexArrays( 1, &m_vao);
  glDeleteBuffers( 3, m_vbo);
  if (m_ibo) glDeleteBuffers( 1, &m_ibo);
  
  cleanData();
  m_vao = 0;
  m_ibo = 0;
}

void VertexBuffer::cleanData()
{
  m_position.clear();
  m_normal.clear();
  m_texcoord.clear();
  m_index.clear();
}

void VertexBuffer::complete(GLenum usage)
{
  assert( m_vao && m_vbo );
  
  m_positionSize = m_normalSize = m_texcoordSize = 0;  
  if (!m_position.empty()) m_positionSize = m_position.size() * sizeof(glm::vec3);
  if (!m_normal.empty()) m_normalSize = m_normal.size() * sizeof(glm::vec3);
  if (!m_texcoord.empty()) m_texcoordSize  = m_texcoord.size() * sizeof(glm::vec2);  
  
  GLsizeiptr bufferSize = m_positionSize + m_normalSize + m_texcoordSize;
    
  
  bind();
  {    
    m_offset = 0;
    
    if (!m_position.empty())
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo[0]);
        glBufferData(GL_ARRAY_BUFFER, m_positionSize, m_position.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }
    
    if (!m_normal.empty())
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo[1]);
        glBufferData(GL_ARRAY_BUFFER, m_normalSize, m_normal.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

    }
    
    if (!m_texcoord.empty())
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo[2]);
        glBufferData(GL_ARRAY_BUFFER, m_texcoordSize, m_texcoord.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }

    if (!m_index.empty())
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*m_index.size(), m_index.data(), GL_STATIC_DRAW);
    }
  }
  unbind();
}


void VertexBuffer::bind() const
{
  glBindVertexArray( m_vao );
}

void VertexBuffer::unbind()
{
  glBindVertexArray( 0u );
}

void VertexBuffer::enable() const
{  
  bind();
}

void VertexBuffer::disable()
{    
  unbind();
}
