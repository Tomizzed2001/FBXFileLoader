#include "FBXFileLoader.hpp"

#include "gtx/string_cast.hpp"
#include "gtx/hash.hpp"

#include <iostream>
#include <unordered_map> 

// Link the libraries necessary for the execution mode
// The file paths are based on the default location for the 2020.3.7 version of the SDK
#if _DEBUG
#pragma comment (lib, "C:\\Program Files\\Autodesk\\FBX\\FBX SDK\\2020.3.7\\lib\\x64\\debug\\libfbxsdk-md.lib")
#pragma comment (lib, "C:\\Program Files\\Autodesk\\FBX\\FBX SDK\\2020.3.7\\lib\\x64\\debug\\libxml2-md.lib")
#pragma comment (lib, "C:\\Program Files\\Autodesk\\FBX\\FBX SDK\\2020.3.7\\lib\\x64\\debug\\zlib-md.lib")
#else
#pragma comment (lib, "C:\\Program Files\\Autodesk\\FBX\\FBX SDK\\2020.3.7\\lib\\x64\\release\\libfbxsdk-md.lib")
#pragma comment (lib, "C:\\Program Files\\Autodesk\\FBX\\FBX SDK\\2020.3.7\\lib\\x64\\release\\libxml2-md.lib")
#pragma comment (lib, "C:\\Program Files\\Autodesk\\FBX\\FBX SDK\\2020.3.7\\lib\\x64\\release\\zlib-md.lib")
#endif

namespace fbx {

    Scene loadFBXFile(const char* filename) {

        // Create the FBX Memory Manager
        FbxManager* memoryManager = FbxManager::Create();

        // Create the IO settings for the import settings
        FbxIOSettings* ioSettings = FbxIOSettings::Create(memoryManager, IOSROOT);

        // Assign the IO settings to the memory manager
        memoryManager->SetIOSettings(ioSettings);

        // Adjust the import settings in the memory manager so that only the required data is imported
        (*(memoryManager->GetIOSettings())).SetBoolProp(IMP_FBX_LINK, false);
        (*(memoryManager->GetIOSettings())).SetBoolProp(IMP_FBX_SHAPE, false);
        (*(memoryManager->GetIOSettings())).SetBoolProp(IMP_FBX_GOBO, false);
        (*(memoryManager->GetIOSettings())).SetBoolProp(IMP_FBX_ANIMATION, false);

        // Create and initialise the importer using the above settings
        FbxImporter* importer = FbxImporter::Create(memoryManager, "");

        // Use the first argument as the filename for the importer.
        if (!importer->Initialize(filename, -1, memoryManager->GetIOSettings())) {
            throw std::runtime_error("Failed to initialise the importer.");
        }

        // Create a scene object to store the data from the .fbx file
        FbxScene* scene = FbxScene::Create(memoryManager, "Scene");

        // Import the .fbx file and store within the scene object created and destroy the importer.
        importer->Import(scene);
        importer->Destroy();

        // Triangulate the scene
        FbxGeometryConverter converter(memoryManager);
        converter.Triangulate(scene, true);

        // Get the root node of the scene
        FbxNode* rootNode = scene->GetRootNode();

        Scene outputScene;
        std::unordered_map<std::string, std::uint32_t> materialMap;
        getChildren(rootNode, outputScene);

        std::cout << std::endl;
        std::cout << "Number of meshes: " << outputScene.meshes.size() << std::endl;
        std::cout << "Number of materials: " << outputScene.materials.size() << std::endl;
        std::cout << std::endl;

        // Destroy the SDK manager and all the other objects it was handling.
        memoryManager->Destroy();

        return outputScene;
    }

    void getChildren(FbxNode* node, Scene& outputScene) {
        // Get the number of children in the node
        int numChildren = node->GetChildCount();
        std::cout << "Name: " << node->GetName() << " Number of children: " << numChildren << " Number of materials: " << node->GetMaterialCount() << std::endl;

        // Check for materials
        uint32_t materialIndex = -1;
        if (node->GetMaterialCount() > 0) {
            // Only get the first material for now and apply it to the entire mesh.
            FbxSurfaceMaterial* material = node->GetMaterial(0);
            // Check if the material data has already been made
            for (size_t i = 0; i < outputScene.materials.size(); i++) {
                if (outputScene.materials[i].materialName == material->GetName()) {
                    materialIndex = i;
                    break;
                }
            }
            // If material has not been found create one for it
            if (materialIndex == -1) {
                outputScene.materials.emplace_back(createMaterialData(material, outputScene));
                materialIndex = outputScene.materials.size() - 1;
            }

            std::cout << "Material name: " << outputScene.materials[materialIndex].materialName << " Material index: " << materialIndex << std::endl;

        }
        else {
            std::cout << "Node has no material component." << std::endl;
        }


        // Check if the node has a mesh component
        FbxMesh* nodeMesh = node->GetMesh();
        if (nodeMesh == NULL) {
            std::cout << "Node has no mesh component." << std::endl;
        }
        else {
            // Create the mesh data
            outputScene.meshes.emplace_back(createMeshData(nodeMesh, materialIndex));
        }

        // If there is no children do not recurse
        if (numChildren == 0) {
            return;
        }

        // Visit all the children of the current node
        for (int i = 0; i < numChildren; i++) {
            FbxNode* childNode = node->GetChild(i);
            getChildren(childNode, outputScene);
        }
    }

    Mesh createMeshData(FbxMesh* inMesh, uint32_t materialIndex) {
        Mesh outMesh;

        // Add the material index to the mesh data structure
        outMesh.materialIndex = materialIndex;

        // Get the number of triangles in the mesh and all the triangles
        int numTriangles = inMesh->GetPolygonCount();

        // Get the number of vertices and all vertices
        int numVertices = inMesh->GetControlPointsCount();
        FbxVector4* fbxVertices = inMesh->GetControlPoints();

        // Get the number of indices and all indices
        int numIndices = inMesh->GetPolygonVertexCount();
        int* indices = inMesh->GetPolygonVertices();

        // Get the normals for the mesh
        FbxArray<FbxVector4> normals;
        // Generate the normals (if there is none) and store them
        if (!(inMesh->GenerateNormals() && inMesh->GetPolygonVertexNormals(normals))) {
            throw std::runtime_error("Failed to gather mesh normals");
        }

        // Get the uvs for the mesh 
        // For now use only the first uv set in the mesh
        FbxStringList uvSets;
        inMesh->GetUVSetNames(uvSets);
        FbxArray<FbxVector2> uvs;
        if (!inMesh->GetPolygonVertexUVs(uvSets.GetStringAt(0), uvs)) {
            throw std::runtime_error("Failed to gather mesh texture coordinates.");
        }

        // Check for duplicate vertices and re-index them
        std::vector<int> seenIndex(numIndices, -1);
        std::unordered_map<glm::vec3, std::uint32_t> seenVertices;

        for (int i = 0; i < numIndices; i++) {  // For each index
            // Get the index
            int index = indices[i];
            // If index has already been seen and re-indexed, use that one
            if (seenIndex[index] != -1) {
                outMesh.vertexIndices.emplace_back(seenIndex[index]);
            }
            // Index has not been found so find the vertex and re-index accordingly
            else {
                // Get the vertex position
                glm::vec3 vertex = glm::vec3(fbxVertices[index][0], fbxVertices[index][1], fbxVertices[index][2]);

                // Get the vertex normal
                glm::vec3 normal = glm::vec3(normals[index][0], normals[index][1], normals[index][2]);

                // Get the vertex texture co-ordinate
                glm::vec2 uv = glm::vec2(uvs[index][0], uvs[index][1]);

                // Check if that position has been seen before
                if (seenVertices.find(vertex) == seenVertices.end()) {
                    // Add the new vertex position and normal
                    outMesh.vertexPositions.emplace_back(vertex);
                    outMesh.vertexNormals.emplace_back(normal);
                    outMesh.vertexTextureCoords.emplace_back(uv);

                    // Store the newly assigned index
                    std::uint32_t newIndex = outMesh.vertexPositions.size() - 1;
                    outMesh.vertexIndices.emplace_back(newIndex);

                    // Add it to the map of seen vertices
                    seenVertices[vertex] = newIndex;

                    // Note that the index has been seen and remapped to a new index
                    seenIndex[index] = newIndex;
                }
                else {
                    // Get the already assigned index
                    std::uint32_t newIndex = seenVertices.at(vertex);

                    // Store the index
                    outMesh.vertexIndices.emplace_back(newIndex);

                    // Map the new index to the seen indices
                    seenIndex[index] = newIndex;
                }
            }
        }

        // Calculate the per vertex tangents
        outMesh.vertexTangents = calculateTangents(outMesh.vertexIndices, outMesh.vertexPositions, outMesh.vertexTextureCoords, outMesh.vertexNormals);

        /* DEBUG INFO
        if (numTriangles < 31) {
            std::cout << "Material Index: " << outMesh.materialIndex << std::endl;

            std::cout << "Indices: ";
            for (size_t i = 0; i < outMesh.vertexIndices.size(); i++) {
                std::cout << outMesh.vertexIndices[i] << " ,";
            }
            std::cout << std::endl;

            std::cout << "Positions: ";
            for (size_t i = 0; i < outMesh.vertexPositions.size(); i++) {
                std::cout << glm::to_string(outMesh.vertexPositions[i]) << ", ";
            }
            std::cout << std::endl;

            std::cout << "Normals: ";
            for (size_t i = 0; i < outMesh.vertexNormals.size(); i++) {
                std::cout << glm::to_string(outMesh.vertexNormals[i]) << ", ";
            }
            std::cout << std::endl;

            std::cout << "Texture Coords: ";
            for (size_t i = 0; i < outMesh.vertexTextureCoords.size(); i++) {
                std::cout << glm::to_string(outMesh.vertexTextureCoords[i]) << ", ";
            }
            std::cout << std::endl;
        }
        */

        return outMesh;
    }

    Material createMaterialData(FbxSurfaceMaterial* inMaterial, Scene& outputScene) {
        Material outMaterial;

        // Get the material name and place it in the struct
        outMaterial.materialName = inMaterial->GetName();

        if (inMaterial->GetClassId().Is(FbxSurfacePhong::ClassId)) {
            /* DEBUG LINE */
            std::cout << "Phong available" << std::endl;

            // Get the material as a phong material
            FbxSurfacePhong* phongMaterial = ((FbxSurfacePhong*)inMaterial);

            // Check for diffuse texture
            if (phongMaterial->Diffuse.GetSrcObject(0)) {
                // There is a diffuse texture
                FbxFileTexture* diffuseTexture = ((FbxFileTexture*)phongMaterial->Diffuse.GetSrcObject(0));

                // Add the index to the material
                outMaterial.diffuseTextureID = createTexture(diffuseTexture, outputScene);

                // Check if the diffuse texture is alpha mapped
                FbxTexture* alphaTexture = ((FbxTexture*)phongMaterial->Diffuse.GetSrcObject(0));
                if (alphaTexture->Alpha < 1) {
                    outMaterial.isAlphaMapped = true;
                }
            }
            else {
                outMaterial.diffuseTextureID = 0xffffffff;
            }

            // NOTE: The specular is the roughness and metalness it seems
            // Check for specular texture
            if (phongMaterial->Specular.GetSrcObject(0)) {
                // There is a specular texture
                FbxFileTexture* specularTexture = ((FbxFileTexture*)phongMaterial->Specular.GetSrcObject(0));

                // Add the index to the material
                outMaterial.specularTextureID = createTexture(specularTexture, outputScene);
            }
            else {
                outMaterial.specularTextureID = 0xffffffff;
            }

            // Check for normal texture
            if (phongMaterial->NormalMap.GetSrcObject(0)) {
                // There is a specular texture
                FbxFileTexture* normalTexture = ((FbxFileTexture*)phongMaterial->NormalMap.GetSrcObject(0));

                // Add the index to the material
                outMaterial.normalTextureID = createTexture(normalTexture, outputScene);
            }
            else {
                outMaterial.normalTextureID = 0xffffffff;
            }

            // Check for emissive texture
            if (phongMaterial->Emissive.GetSrcObject(0)) {
                // There is a specular texture
                FbxFileTexture* emissiveTexture = ((FbxFileTexture*)phongMaterial->Emissive.GetSrcObject(0));

                // Add the index to the material
                outMaterial.emissiveTextureID = createTexture(emissiveTexture, outputScene);
            }
            else {
                outMaterial.emissiveTextureID = 0xffffffff;
            }

        }
        else if (inMaterial->GetClassId().Is(FbxSurfaceLambert::ClassId)) {
            std::cout << "Lambertian available" << std::endl;
        }
        else {
            std::cout << "What is this" << std::endl;
        }

        return outMaterial;
    }

    std::uint32_t createTexture(FbxFileTexture* texture, Scene& outputScene) {
        /* DEBUG LINE */
        std::cout << texture->GetFileName() << std::endl;

        // Check if the diffuse texture already exists in the output scene
        int textureIndex = -1;
        for (size_t i = 0; i < outputScene.textures.size(); i++) {
            if (outputScene.textures[i].filePath == texture->GetFileName()) {
                return i;
            }
        }

        // If texture index has not been found create texture and add it to the output
        if (textureIndex == -1) {
            Texture newTexture;
            newTexture.filePath = texture->GetFileName();
            outputScene.textures.emplace_back(newTexture);
            // Remember the ID
            textureIndex = outputScene.textures.size() - 1;
        }

        return textureIndex;
    }

    std::vector<glm::vec4> calculateTangents(
        std::vector<std::uint32_t>& indices,
        std::vector<glm::vec3>& positions,
        std::vector<glm::vec2>& uvs,
        std::vector<glm::vec3>& normals) {

        // Initialise an array to store all the different tangents for each vertex
        std::vector<std::vector<glm::vec3>> vertexTriangleTangents(positions.size(), std::vector<glm::vec3>());
        std::vector<std::vector<glm::vec3>> vertexTriangleBitangents(positions.size(), std::vector<glm::vec3>());

        // Visit each triangle in the mesh
        for (size_t i = 0; i < indices.size(); i += 3) {
            // Get the positions of all points in the triangle v0, v1, v2 
            glm::vec3 v0, v1, v2;
            v0 = positions[indices[i]];
            v1 = positions[indices[i + 1]];
            v2 = positions[indices[i + 2]];

            // and get the corresponding texture coordinates
            glm::vec2 uv0, uv1, uv2;
            uv0 = uvs[indices[i]];
            uv1 = uvs[indices[i + 1]];
            uv2 = uvs[indices[i + 2]];

            // Get the coordinates in terms of v0
            glm::vec3 AB, AC;
            AB = v1 - v0;
            AC = v2 - v0;

            // again with the texture coordinates
            glm::vec2 uvAB, uvAC;
            uvAB = uv1 - uv0;
            uvAC = uv2 - uv0;

            // Solve to find the unnormalized tangent vector of the triangle
            glm::vec3 tangent = (1 / ((uvAB.x * uvAC.y) - (uvAC.x - uvAB.y))) * ((uvAC.y * AB) + (-uvAB.y * AC));
            glm::vec3 bitangent = (1 / ((uvAB.x * uvAC.y) - (uvAC.x - uvAB.y))) * ((-uvAC.x * AB) + (uvAB.x * AC));


            // Tangent is the same for all vertices of the triangle so add to all of them
            vertexTriangleTangents[indices[i]].emplace_back(tangent);
            vertexTriangleTangents[indices[i + 1]].emplace_back(tangent);
            vertexTriangleTangents[indices[i + 2]].emplace_back(tangent);

            vertexTriangleBitangents[indices[i]].emplace_back(bitangent);
            vertexTriangleBitangents[indices[i + 1]].emplace_back(bitangent);
            vertexTriangleBitangents[indices[i + 2]].emplace_back(bitangent);
        }

        // Average the tangents for all vertices
        std::vector<glm::vec4> tangents;
        for (size_t i = 0; i < vertexTriangleTangents.size(); i++) {
            glm::vec3 tangent = glm::vec3(0, 0, 0);
            glm::vec3 biTangent = glm::vec3(0, 0, 0);
            for (size_t j = 0; j < vertexTriangleTangents[i].size(); j++) {
                tangent = tangent + vertexTriangleTangents[i][j];
                biTangent = biTangent + vertexTriangleBitangents[i][j];
            }

            // Orthogonalize the tangent and normalise the output
            tangent = glm::normalize(tangent - normals[i] * glm::dot(normals[i], tangent));

            // Calculate the handedness of the bitangent
            int handedness;
            if (glm::dot(glm::cross(normals[i], tangent), biTangent) < 0) {
                handedness = -1;
            }
            else {
                handedness = 1;
            }

            tangents.emplace_back(glm::vec4(tangent, handedness));
        }

        return tangents;

    }

}