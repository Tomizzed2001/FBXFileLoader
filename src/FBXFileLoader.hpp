#pragma once
#include <vector>
#include <functional>

#include <glm.hpp>
#include <fbxsdk.h>

/// A set of structs used to hold the information from the FBX file.

/// <summary>
/// Data for the texture of a material
/// </summary>
struct Texture
{

};

/// <summary>
/// Data for the material of a mesh
/// </summary>
struct Material
{
	std::string materialName;	// Mostly for DEBUG
};

/// <summary>
/// Data for a mesh within a scene
/// </summary>
struct Mesh
{
	std::uint32_t materialIndex;	// Relates to material
		
	std::vector<glm::vec3> vertexPositions;		
	std::vector<glm::vec2> vertexTextureCoords;		
	std::vector<glm::vec3> vertexNormals;

	std::vector<std::uint32_t> vertexIndices;
};

/// <summary>
/// Data contained in a scene
/// </summary>
struct Scene
{
	std::vector<Mesh> meshes;
	std::vector<Material> materials;
	std::vector<Texture> textures;
};

/// <summary>
/// Loads a given FBX file and creates a set of data that can be used for 
/// PBR.
/// </summary>
/// <param name="filename">The .fbx file path</param>
/// <returns>A Scene structure</returns>
Scene loadFBXFile(const char* filename);

/// <summary>
/// Gets the children of a given node
/// </summary>
/// <param name="node">A node in an FbxScene</param>
void getChildren(FbxNode* node, Scene& outputScene);

/// <summary>
/// Creates and populates a mesh data structure given an Fbx mesh
/// </summary>
/// <param name="inMesh">An FbxMesh</param>
/// <returns>A mesh data structure</returns>
Mesh createMeshData(FbxMesh* inMesh, uint32_t materialIndex);

/// <summary>
/// Creates and populates a material data structure given an Fbx material
/// </summary>
/// <param name="inMaterial">Fbx Material</param>
/// <returns>Material data structure</returns>
Material createMaterialData(FbxSurfaceMaterial* inMaterial);