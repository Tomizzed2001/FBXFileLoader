#pragma once
#include <vector>
#include <functional>

#include <glm.hpp>
#include <fbxsdk.h>

/// A set of structs used to hold the information from the FBX file.
namespace fbx {
	/// <summary>
	/// Data for the texture of a material
	/// </summary>
	struct Texture
	{
		std::string	filePath;

		bool isEmpty = false;
	};

	/// <summary>
	/// Data for the material of a mesh
	/// </summary>
	struct Material
	{
		std::string materialName;	// Mostly for DEBUG

		std::uint32_t diffuseTextureID;
		std::uint32_t specularTextureID;
		std::uint32_t normalTextureID;
		std::uint32_t emissiveTextureID;

		bool isAlphaMapped = false;

	};

	/// <summary>
	/// Data for a mesh within a scene
	/// </summary>
	struct Mesh
	{
		// Per mesh variables
		std::vector<uint32_t> materials;
		
		// Per vertex variables
		std::vector<glm::vec3> vertexPositions;		
		std::vector<glm::vec2> vertexTextureCoords;		
		std::vector<glm::vec3> vertexNormals;
		std::vector<glm::vec4> vertexTangents;
		std::vector<uint32_t> vertexMaterialIDs;

		// Per polygon variables
		// VertexMaterialIDs could go here for later optimisation

		// Per index variables
		std::vector<uint32_t> vertexIndices;
	};

	/// <summary>
	/// Data for a light within the scene
	/// </summary>
	struct Light
	{
		bool isPointLight;

		glm::vec3 location;
		glm::vec3 colour;
		glm::mat4 direction;
	};

	/// <summary>
	/// Data contained in a scene
	/// </summary>
	struct Scene
	{
		std::vector<Mesh> meshes;
		std::vector<Material> materials;
		std::vector<Texture> diffuseTextures;
		std::vector<Texture> specularTextures;
		std::vector<Texture> normalTextures;
		std::vector<Texture> emissiveTextures;
		std::vector<Light> lights;
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
	/// <param name="materialIndices">The material indices from the node</param>
	/// <param name="transform">The node transform matrix</param>
	/// <returns>A mesh data structure</returns>
	Mesh createMeshData(FbxMesh* inMesh, std::vector<uint32_t>& materialIndices, glm::mat4 transform);

	/// <summary>
	/// Creates and populates a material data structure given an Fbx material
	/// </summary>
	/// <param name="inMaterial">Fbx Material</param>
	/// <param name="outputScene">The output data for the program</param>
	/// <returns>Material data structure</returns>
	Material createMaterialData(FbxSurfaceMaterial* inMaterial, Scene& outputScene);

	/// <summary>
	/// Creates a texture and adds it to the output scene if it does not already exist
	/// </summary>
	/// <param name="texture">The texture to look for / add</param>
	/// <param name="textureSet">The output texture set to use</param>
	/// <returns>The ID of the texture in the output scene</returns>
	std::uint32_t createTexture(FbxFileTexture* texture, std::vector<Texture>& textureSet);

	/// <summary>
	/// Creates and populates a light data structure given an fbx light
	/// </summary>
	/// <param name="inLight">Fbx light</param>
	/// <param name="transform">The node transform matrix</param>
	/// <returns></returns>
	Light createLightData(FbxLight* inLight, glm::mat4 transform);

	/// <summary>
	/// Calculates the vertex tangents for a given mesh
	/// </summary>
	/// <param name="indices">The indices of the mesh</param>
	/// <param name="positions">The vertex positions</param>
	/// <param name="uvs">The vertex texture coordinates</param>
	/// <param name="normals">The per vertex normals</param>
	/// <returns>A set of tangents with the 4th float containing the handedness of the bitangent</returns>
	std::vector<glm::vec4> calculateTangents(
		std::vector<std::uint32_t>& indices,
		std::vector<glm::vec3>& positions,
		std::vector<glm::vec2>& uvs,
		std::vector<glm::vec3>& normals);
}