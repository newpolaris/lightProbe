#include <ModelAssImp.h>
#include <tools/gltools.hpp>
#include <GL/glew.h>

// GLM for matrix transformation
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp> 

// assimp
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing fla

void BaseMaterial::destroy()
{
}

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
	GL_ASSERT(glDrawElements(GL_TRIANGLES, m_NumIndices, GL_UNSIGNED_INT, 0));

	CHECKGLERROR();
}

void ModelAssImp::destroy()
{
	m_Meshes.clear();
	m_Materials.clear();
}

bool ModelAssImp::loadFromFile(const std::string& filename)
{
	Assimp::Importer Importer;

	int postprocess = aiProcess_Triangulate 
		| aiProcess_GenSmoothNormals 
		| aiProcess_SplitLargeMeshes
		| aiProcess_SortByPType
		| aiProcess_OptimizeMeshes
		| aiProcess_CalcTangentSpace
		| aiProcess_JoinIdenticalVertices
		;

	const aiScene* pScene = Importer.ReadFile(filename.c_str(), postprocess);
	assert(pScene != nullptr);
	if (!pScene) return false;

	// material
	for (uint32_t i = 0; i < pScene->mNumMaterials; i++)
	{

	}

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(3, m_vbo);
    glGenBuffers(1, &m_ibo);

    glBindVertexArray( m_vao );

	for (uint32_t meshIdx = 0; meshIdx < pScene->mNumMeshes; meshIdx++)
	{
		const aiMesh* paiMesh = pScene->mMeshes[meshIdx];
		BaseMeshPtr mesh = std::make_shared<BaseMesh>();
		mesh->m_MaterialIndex = paiMesh->mMaterialIndex;
		if (mesh->m_MaterialIndex < m_Materials.size()) 
			mesh->m_Material = m_Materials[mesh->m_MaterialIndex];

		// vertex buffer
        std::vector<GLuint> indices;
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texcoords;
            
        GLsizeiptr m_positionSize;
        GLsizeiptr m_normalSize;
        GLsizeiptr m_texcoordSize;

		const int NumVertices = paiMesh->mNumVertices;
		positions.resize(NumVertices);
		normals.resize(NumVertices);
		texcoords.resize(NumVertices);

		bool bHasTex = paiMesh->HasTextureCoords(0);
		const aiVector3D zero(0.f, 0.f, 0.f);
		for (int i = 0; i < NumVertices; i++)
		{
			const aiVector3D& pos = paiMesh->mVertices[i];
			const aiVector3D& nor = paiMesh->mNormals[i];
			const aiVector3D& tex = bHasTex ? paiMesh->mTextureCoords[0][i] : zero;

			positions[i] = glm::vec3(pos.x, pos.y, pos.z);
			normals[i] = glm::vec3(nor.x, nor.y, nor.z);
			texcoords[i] = glm::vec2(tex.x, tex.y);
		}

		// index buffer 
		const int NumFaces = paiMesh->mNumFaces;
        indices.clear();
		indices.reserve(NumFaces * 3);
		for (unsigned int i = 0; i < NumFaces; i++) {
			const aiFace& face = paiMesh->mFaces[i];
			indices.push_back(face.mIndices[0]); 
			indices.push_back(face.mIndices[1]); 
			indices.push_back(face.mIndices[2]); 
		}
        mesh->m_NumIndices = indices.size();

		m_Meshes.push_back(mesh);

        m_positionSize = m_normalSize = m_texcoordSize = 0;  
        auto& m_position = positions;
        auto& m_normal = normals;
        auto& m_texcoord = texcoords;
        auto& m_index = indices;

        if (!m_position.empty()) m_positionSize = m_position.size() * sizeof(glm::vec3);
        if (!m_normal.empty()) m_normalSize = m_normal.size() * sizeof(glm::vec3);
        if (!m_texcoord.empty()) m_texcoordSize  = m_texcoord.size() * sizeof(glm::vec2);  

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

    glBindVertexArray( 0u );

	return true;
}

void ModelAssImp::render()
{
    glBindVertexArray( m_vao );
	for (auto& mesh : m_Meshes)
		mesh->render();
    glBindVertexArray( 0u );
}

