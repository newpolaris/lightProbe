#pragma once

#include <vector>
#include <GL/glew.h>

class IndexBuffer
{
public:

	IndexBuffer()
	{
	}

	void initialize();
	void destroy();

	void create(std::vector<unsigned int>& indices);
	void bind();
	void unbind();
	void enable();
	void disable();

	GLuint m_ebo = 0;
	unsigned int m_NumIndices = 0;
};

