#include <memory>
#include <GLType/IndexBuffer.h>
#include <GLType/VertexBuffer.h>

typedef std::shared_ptr<class BaseMaterial> BaseMaterialPtr;
typedef std::shared_ptr<class BaseMesh> BaseMeshPtr;
typedef std::shared_ptr<class ModelAssImp> ModelPtr;
typedef std::vector<BaseMaterialPtr> BaseMaterialList;
typedef std::vector<BaseMeshPtr> BaseMeshList;

class BaseMaterial
{
public:
	BaseMaterial()
	{
	}

	virtual ~BaseMaterial()
	{
		destroy();
	}

	void initialize();
	void destroy();
};


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
    VertexBuffer m_VertexBuffer;
	IndexBuffer m_IndexBuffer;
	BaseMaterialPtr m_Material;
};

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

	BaseMeshList m_Meshes;
	BaseMaterialList m_Materials;
};

