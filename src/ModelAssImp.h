#include <memory>
#include <vector>
#include <GL/glew.h>
#include "Types.h"

class ModelAssImp
{
public:
	ModelAssImp() 
	{
	}

	void create(const std::string& filename);
	void destroy();
	void render();

	bool loadFromFile(const std::string& filename);

    GLuint m_vao;
    GLuint m_vbo[3];    
    GLuint m_ibo;

	BaseMeshList m_Meshes;
	BaseMaterialList m_Materials;
};

