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
	m_VertexBuffer.destroy();
	m_IndexBuffer.destroy();
	m_Material = nullptr;
}

void BaseMesh::render()
{
	if (m_Material)
	{
	}
	m_VertexBuffer.enable();  
	m_IndexBuffer.enable();  
	const unsigned int numIndices = m_IndexBuffer.m_NumIndices;
	GL_ASSERT(glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0));
	m_VertexBuffer.disable();
	m_IndexBuffer.disable();  

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

	for (uint32_t meshIdx = 0; meshIdx < pScene->mNumMeshes; meshIdx++)
	{
		const aiMesh* paiMesh = pScene->mMeshes[meshIdx];
		BaseMeshPtr mesh = std::make_shared<BaseMesh>();
		mesh->m_MaterialIndex = paiMesh->mMaterialIndex;
		if (mesh->m_MaterialIndex < m_Materials.size()) 
			mesh->m_Material = m_Materials[mesh->m_MaterialIndex];

		// vertex buffer
		VertexBuffer& vb = mesh->m_VertexBuffer;
		std::vector<glm::vec3>& positions = vb.getPosition();
		std::vector<glm::vec3>& normals = vb.getNormal();
		std::vector<glm::vec2>& texcoords = vb.getTexcoord();

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

		// Generate buffer's id
		vb.initialize();  

		// Send data to the GPU
		vb.complete( GL_STATIC_DRAW );

		// Remove data from the CPU [optional]
		vb.cleanData();

		// index buffer 
		const int NumFaces = paiMesh->mNumFaces;
		std::vector<unsigned int> indices;
		indices.reserve(NumFaces * 3);
		for (unsigned int i = 0; i < NumFaces; i++) {
			const aiFace& face = paiMesh->mFaces[i];
			indices.push_back(face.mIndices[0]); 
			indices.push_back(face.mIndices[1]); 
			indices.push_back(face.mIndices[2]); 
		}
		mesh->m_IndexBuffer.create(indices);	

		m_Meshes.push_back(mesh);
	}

	return true;
}

void ModelAssImp::render()
{
	for (auto& mesh : m_Meshes)
		mesh->render();
}

