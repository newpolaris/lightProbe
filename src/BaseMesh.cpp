#include "BaseMesh.h"
#include <tools/gltools.hpp>

void BaseMesh::initialize()
{
}

void BaseMesh::destroy()
{
	m_Material = nullptr;
}

void BaseMesh::render()
{
	if (m_Material)
	{
	}
	glDrawElements(GL_TRIANGLES, m_NumIndices, GL_UNSIGNED_INT, 0);

	CHECKGLERROR();
}

