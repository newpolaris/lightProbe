#pragma once

#include <Types.h>

class BaseMesh
{
public:
	BaseMesh()
	{
	}

	virtual ~BaseMesh()
	{
		destroy();
	}

	void initialize();
	void destroy();
	void render();

    uint32_t m_NumIndices = 0;
	int32_t m_MaterialIndex = -1;
	BaseMaterialPtr m_Material;
};
