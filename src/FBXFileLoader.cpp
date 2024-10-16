#include "FBXFileLoader.hpp"

#include "gtx/quaternion.hpp"
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

#define DEBUG_OUTPUTS false

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

        std::cout << "Loading " << filename << std::endl;

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
        
        if (DEBUG_OUTPUTS) {
            std::cout << std::endl;
            std::cout << "Number of meshes: " << outputScene.meshes.size() << std::endl;
            std::cout << "Number of materials: " << outputScene.materials.size() << std::endl;
            std::cout << std::endl;
        }        

        // Destroy the SDK manager and all the other objects it was handling.
        memoryManager->Destroy();

        std::cout << "Finished loading " << filename << std::endl;

        return outputScene;
    }

    void getChildren(FbxNode* node, Scene& outputScene) {
        // Get the number of children in the node
        int numChildren = node->GetChildCount();

        if(DEBUG_OUTPUTS)
            std::cout << "Name: " << node->GetName() << " Number of children: " << numChildren << " Number of materials: " << node->GetMaterialCount() << std::endl;

        // Get the global transform matrix of the node
        FbxAMatrix nodeTransform = node->EvaluateGlobalTransform();

        // Set the scale of the transform
        //FbxVector4 fbxScale = nodeTransform.GetS() * 0.01;
        FbxVector4 fbxScale = nodeTransform.GetS();
        nodeTransform.SetS(fbxScale);

        // Convert it to GLM
        glm::mat4 transformMatrix;
        FbxVector4 col0 = nodeTransform.GetRow(0);
        transformMatrix[0] = glm::vec4(col0[0], col0[1], col0[2], col0[3]);
        FbxVector4 col1 = nodeTransform.GetRow(1);
        transformMatrix[1] = glm::vec4(col1[0], col1[1], col1[2], col1[3]);
        FbxVector4 col2 = nodeTransform.GetRow(2);
        transformMatrix[2] = glm::vec4(col2[0], col2[1], col2[2], col2[3]);
        FbxVector4 col3 = nodeTransform.GetRow(3);
        transformMatrix[3] = glm::vec4(col3[0], col3[1], col3[2], col3[3]);

        // Reset the translation component so it is unaffected by scale
        //FbxVector4 fbxTranslation = nodeTransform.GetT() * 0.01;
        FbxVector4 fbxTranslation = nodeTransform.GetT();
        glm::vec4 translation = glm::vec4(fbxTranslation[0], fbxTranslation[1], fbxTranslation[2], 1);
        transformMatrix[3] = translation;

        // Check for materials
        std::vector<uint32_t> materialIndices;
        if (node->GetMaterialCount() > 0) {
            for (int i = 0; i < node->GetMaterialCount(); i++) {
                // Reset material index
                uint32_t materialIndex = -1;
                
                // Only get the first material for now and apply it to the entire mesh.
                FbxSurfaceMaterial* material = node->GetMaterial(i);

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

                // Add to the material indices
                materialIndices.emplace_back(materialIndex);

                if (DEBUG_OUTPUTS)
                    std::cout << "Material name: " << outputScene.materials[materialIndex].materialName << " Material index: " << materialIndex << std::endl;
            }
        }
        else {
            if (DEBUG_OUTPUTS)
                std::cout << "Node has no material component." << std::endl;
        }


        // Check if the node has a mesh component
        FbxMesh* nodeMesh = node->GetMesh();
        if (nodeMesh == NULL) {
            if (DEBUG_OUTPUTS) {
                std::cout << "Node has no mesh component." << std::endl;
            }
            FbxLight* light = node->GetLight();
            if (light != NULL) {
                outputScene.lights.emplace_back(createLightData(light, transformMatrix));
            }
        }
        else {
            // Create the mesh data
            outputScene.meshes.emplace_back(createMeshData(nodeMesh, materialIndices, transformMatrix));
            outputScene.meshes.back().materials = materialIndices;
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

    Mesh createMeshData(FbxMesh* inMesh, std::vector<uint32_t>& materialIndices, glm::mat4 transform) {
        Mesh outMesh;

        // Get the number of triangles in the mesh and all the triangles
        int numTriangles = inMesh->GetPolygonCount();

        // Get the number of vertices and all vertices
        int numVertices = inMesh->GetControlPointsCount();
        FbxVector4* fbxVertices = inMesh->GetControlPoints();

        // Get the number of indices and all indices
        int numIndices = inMesh->GetPolygonVertexCount();
        int* fbxIndices = inMesh->GetPolygonVertices();

        // Get the normals for the mesh
        FbxArray<FbxVector4> fbxNormals;
        // Generate the normals (if there is none) and store them
        if (!(inMesh->GenerateNormals() && inMesh->GetPolygonVertexNormals(fbxNormals))) {
            throw std::runtime_error("Failed to gather mesh normals");
        }

        // Get the uvs for the mesh 
        // For now use only the first uv set in the mesh
        FbxStringList uvSets;
        inMesh->GetUVSetNames(uvSets);
        FbxArray<FbxVector2> fbxUVs;
        if (!inMesh->GetPolygonVertexUVs(uvSets.GetStringAt(0), fbxUVs)) {
            throw std::runtime_error("Failed to gather mesh texture coordinates.");
        }

        // Remove the translation component for the  
        glm::mat4 normalTransform = transform;
        normalTransform[3] = glm::vec4(0, 0, 0, 1);

        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec3> normals;
        std::vector<uint32_t> materialIDs;
        std::vector<uint32_t> indices;

        // For each index
        for (int i = 0; i < numIndices; i++) {  
            // Get the index
            int index = fbxIndices[i];

            // Get the vertex position
            glm::vec3 vertex = glm::vec3(fbxVertices[index][0], fbxVertices[index][1], fbxVertices[index][2]);

            // Get the vertex normal
            glm::vec3 normal = glm::vec3(fbxNormals[i][0], fbxNormals[i][1], fbxNormals[i][2]);

            // Get the vertex texture co-ordinate
            glm::vec2 uv = glm::vec2(fbxUVs[i][0], fbxUVs[i][1]);

            // Add the new vertex position and normal
            positions.emplace_back(transform * glm::vec4(vertex, 1));
            normals.emplace_back(normalTransform * glm::vec4(normal, 1));
            uvs.emplace_back(uv);

            // Store the newly assigned index
            indices.emplace_back(i);
        }

        // Calculate the per polygon material ids
        FbxLayerElementMaterial* materialElement = inMesh->GetElementMaterial();
        for (int i = 0; i < inMesh->GetPolygonCount(); i++) {
            // Material index for the current polygon
            int materialIndex = materialElement->GetIndexArray().GetAt(i);
            materialIDs.emplace_back(materialIndices[materialIndex]);
            materialIDs.emplace_back(materialIndices[materialIndex]);
            materialIDs.emplace_back(materialIndices[materialIndex]);
        }

        // Check if the mesh will benefit from redoing the indices
        // Check for duplicate vertices and re-index them

        
        std::vector<int> seenIndex(indices.size(), -1);
        std::unordered_map<glm::vec3, std::pair<uint32_t, uint32_t>> seenVertices;
        std::vector<std::vector<std::uint32_t>> samePositionsArray;

        for (size_t i = 0; i < indices.size(); i++) {
            uint32_t index = indices[i];

            // If index has already been seen and re-indexed, use that one
            if (seenIndex[index] != -1) {
                outMesh.vertexIndices.emplace_back(seenIndex[index]);
            }
            // Index has not been found so find the vertex and re-index accordingly
            else {
                // Check if that position has been seen before
                if (seenVertices.find(positions[index]) == seenVertices.end()) {
                    // New position found
                    // Add the new vertex position normal and uv
                    outMesh.vertexPositions.emplace_back(glm::vec4(positions[index], 1));
                    outMesh.vertexNormals.emplace_back(glm::vec4(normals[index], 1));
                    outMesh.vertexTextureCoords.emplace_back(uvs[index]);
                    outMesh.vertexMaterialIDs.emplace_back(materialIDs[index]);

                    // Store the newly assigned index
                    std::uint32_t newIndex = outMesh.vertexPositions.size() - 1;
                    outMesh.vertexIndices.emplace_back(newIndex);

                    // Add it to the map of seen vertices
                    samePositionsArray.emplace_back(std::vector<std::uint32_t>());
                    samePositionsArray[samePositionsArray.size() - 1].emplace_back(newIndex);
                    
                    seenVertices[positions[index]] = std::pair<uint32_t, uint32_t>(newIndex, samePositionsArray.size() - 1);

                    // Note that the index has been seen and remapped to a new index
                    seenIndex[index] = newIndex;
                }
                else {
                    // Position already exists
                    // Get the already assigned index
                    std::uint32_t newIndex = seenVertices.at(positions[index]).first;
                    std::uint32_t vertexID = seenVertices.at(positions[index]).second;

                    // Check that the uvs, normals and materials match up as well
                    if ((outMesh.vertexTextureCoords[newIndex] != uvs[index]) || 
                        outMesh.vertexNormals[newIndex] != normals[index] ||
                        outMesh.vertexMaterialIDs[newIndex] != materialIDs[index]) {
                        // If uvs or normals do not match
                        // Add to index array as if its a new vertex
                        outMesh.vertexPositions.emplace_back(glm::vec4(positions[index], 1));
                        outMesh.vertexNormals.emplace_back(glm::vec4(normals[index], 1));
                        outMesh.vertexTextureCoords.emplace_back(uvs[index]);
                        outMesh.vertexMaterialIDs.emplace_back(materialIDs[index]);

                        // Store the newly assigned index
                        std::uint32_t oldIndex = newIndex;
                        newIndex = outMesh.vertexPositions.size() - 1;
                        outMesh.vertexIndices.emplace_back(newIndex);

                        // Map the new index to the vertex
                        samePositionsArray[vertexID].emplace_back(newIndex);

                    }
                    else {
                        // Identical index so remap and store the index only
                        // Store the index
                        outMesh.vertexIndices.emplace_back(newIndex);

                        // Map the new index to the seen indices
                        seenIndex[index] = newIndex;
                    }                    
                }
            }
        }
        
        // Calculate the per vertex tangents
        outMesh.vertexTangents = calculateTangents(outMesh.vertexIndices, outMesh.vertexPositions, outMesh.vertexTextureCoords, outMesh.vertexNormals);

        return outMesh;
    }

    Material createMaterialData(FbxSurfaceMaterial* inMaterial, Scene& outputScene) {
        Material outMaterial;

        // Get the material name and place it in the struct
        outMaterial.materialName = inMaterial->GetName();

        if (inMaterial->GetClassId().Is(FbxSurfacePhong::ClassId)) {
            /* DEBUG LINE */
            if (DEBUG_OUTPUTS)
                std::cout << "Phong available" << std::endl;

            // Get the material as a phong material
            FbxSurfacePhong* phongMaterial = ((FbxSurfacePhong*)inMaterial);

            // Check for diffuse texture
            if (phongMaterial->Diffuse.GetSrcObject(0)) {
                // There is a diffuse texture
                FbxFileTexture* diffuseTexture = ((FbxFileTexture*)phongMaterial->Diffuse.GetSrcObject(0));

                // Add the index to the material
                outMaterial.diffuseTextureID = createTexture(diffuseTexture, outputScene.diffuseTextures);

                // Check if the diffuse texture is alpha mapped
                FbxTexture* alphaTexture = ((FbxTexture*)phongMaterial->Diffuse.GetSrcObject(0));
                if (alphaTexture->Alpha < 1) {
                    outMaterial.isAlphaMapped = true;
                }
            }
            else {
                // Place an empty texture in the array so it is aligned with material index
                Texture emptyTexture;
                emptyTexture.isEmpty = true;
                outputScene.diffuseTextures.emplace_back(emptyTexture);

                outMaterial.diffuseTextureID = 0xffffffff;
            }

            // NOTE: The specular is the roughness and metalness
            // Check for specular texture
            if (phongMaterial->Specular.GetSrcObject(0)) {
                // There is a specular texture
                FbxFileTexture* specularTexture = ((FbxFileTexture*)phongMaterial->Specular.GetSrcObject(0));

                // Add the index to the material
                outMaterial.specularTextureID = createTexture(specularTexture, outputScene.specularTextures);
            }
            else {
                // Place an empty texture in the array so it is aligned with material index
                Texture emptyTexture;
                emptyTexture.isEmpty = true;
                outputScene.specularTextures.emplace_back(emptyTexture);

                outMaterial.specularTextureID = 0xffffffff;
            }

            // Check for normal texture
            if (phongMaterial->NormalMap.GetSrcObject(0)) {
                // There is a specular texture
                FbxFileTexture* normalTexture = ((FbxFileTexture*)phongMaterial->NormalMap.GetSrcObject(0));

                // Add the index to the material
                outMaterial.normalTextureID = createTexture(normalTexture, outputScene.normalTextures);
            }
            else {
                // Place an empty texture in the array so it is aligned with material index
                Texture emptyTexture;
                emptyTexture.isEmpty = true;
                outputScene.normalTextures.emplace_back(emptyTexture);

                outMaterial.normalTextureID = 0xffffffff;
            }

            // Check for emissive texture
            if (phongMaterial->Emissive.GetSrcObject(0)) {
                // There is a specular texture
                FbxFileTexture* emissiveTexture = ((FbxFileTexture*)phongMaterial->Emissive.GetSrcObject(0));

                // Add the index to the material
                outMaterial.emissiveTextureID = createTexture(emissiveTexture, outputScene.emissiveTextures);
            }
            else {
                // Place an empty texture in the array so it is aligned with material index
                Texture emptyTexture;
                emptyTexture.isEmpty = true;
                outputScene.emissiveTextures.emplace_back(emptyTexture);

                outMaterial.emissiveTextureID = 0xffffffff;
            }

        }
        else if (inMaterial->GetClassId().Is(FbxSurfaceLambert::ClassId)) {
            if (DEBUG_OUTPUTS)
                std::cout << "Lambertian available" << std::endl;
        }
        else {
            if (DEBUG_OUTPUTS)
                std::cout << "What is this" << std::endl;
        }

        return outMaterial;
    }

    std::uint32_t createTexture(FbxFileTexture* texture, std::vector<Texture>& textureSet) {
        /* DEBUG LINE */
        if (DEBUG_OUTPUTS)
            std::cout << texture->GetFileName() << std::endl;

        // Check if the texture already exists in the output scene
        int textureIndex = -1;
        for (size_t i = 0; i < textureSet.size(); i++) {
            if (textureSet[i].filePath == texture->GetFileName()) {
                return i;
            }
        }

        // If texture index has not been found create texture and add it to the output
        if (textureIndex == -1) {
            Texture newTexture;
            newTexture.filePath = texture->GetFileName();
            textureSet.emplace_back(newTexture);
            // Remember the ID
            textureIndex = textureSet.size() - 1;
        }

        return textureIndex;
    }

    Light createLightData(FbxLight* inLight, glm::mat4 transform) {
        Light outLight;

        // The location of the light is simply defined by the transform matrix
        outLight.location = transform[3];

        // Get the light colour
        FbxDouble3 colour = inLight->Color.Get();
        outLight.colour = glm::vec3(colour[0], colour[1], colour[2]);

        // Get the direction of the light if its a directional light
        if (inLight->LightType.Get() == FbxLight::eSpot ||
            inLight->LightType.Get() == FbxLight::eDirectional) {
            // It is not a point light
            outLight.isPointLight = false;

            // Get rotation only from the transform
            transform[3] = glm::vec4(0, 0, 0, 1);
            outLight.direction = transform;
        }
        else {
            // It is a point light
            outLight.isPointLight = true;

            // No direction so just set as the identity
            outLight.direction = glm::mat4(1);
        }


        return outLight;
    }

    std::vector<glm::vec4> calculateTangents(
        std::vector<std::uint32_t>& indices,
        std::vector<glm::vec3>& positions,
        std::vector<glm::vec2>& uvs,
        std::vector<glm::vec3>& normals) {

        // Initialise an array to store all the different tangents for each vertex
        //std::vector<std::vector<glm::vec3>> vertexTriangleTangents(positions.size(), std::vector<glm::vec3>());
        //std::vector<std::vector<glm::vec3>> vertexTriangleBitangents(positions.size(), std::vector<glm::vec3>());

        std::vector<glm::vec3> vTangents(positions.size());
        std::vector<glm::vec3> vBitangents(positions.size());

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
            //glm::vec3 tangent = (1 / ((uvAB.x * uvAC.y) - (uvAC.x - uvAB.y))) * ((uvAC.y * AB) + (-uvAB.y * AC));
            //glm::vec3 bitangent = (1 / ((uvAB.x * uvAC.y) - (uvAC.x - uvAB.y))) * ((-uvAC.x * AB) + (uvAB.x * AC));

            float determinant = 1.0f / (uvAB.x * uvAC.y - uvAC.x * uvAB.y);
            glm::vec3 tangent = determinant * (uvAC.y * AB - uvAB.y * AC);
            glm::vec3 bitangent = determinant * (-uvAC.x * AB + uvAB.x * AC);

            // Tangent is the same for all vertices of the triangle so add to all of them
            vTangents[indices[i]] += tangent;
            vTangents[indices[i + 1]] += tangent;
            vTangents[indices[i + 2]] += tangent;

            vBitangents[indices[i]] += bitangent;
            vBitangents[indices[i + 1]] += bitangent;
            vBitangents[indices[i + 2]] += bitangent;
        }

        // Average the tangents for all vertices
        std::vector<glm::vec4> tangents;
        for (size_t i = 0; i < vTangents.size(); i++) {
            glm::vec3 normal = normals[i];

            glm::vec3 tangent = vTangents[i];
            glm::vec3 bitangent = vBitangents[i];
            // Orthogonalize the tangent and normalise the output
            tangent = glm::normalize(tangent - normal * glm::dot(normal, tangent));

            // Calculate the handedness of the bitangent
            int handedness;
            if (glm::dot(glm::cross(normal, tangent), bitangent) < 0) {
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