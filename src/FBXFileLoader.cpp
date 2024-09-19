#include "FBXFileLoader.hpp"

#include "gtx/string_cast.hpp"
#include "gtx/hash.hpp"

#include <filesystem>
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

Scene loadFBXFile(const char* filename){

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
    getChildren(rootNode, outputScene);

    // Destroy the SDK manager and all the other objects it was handling.
    memoryManager->Destroy();

    return outputScene;
}

void getChildren(FbxNode* node, Scene& outputScene) {
    // Get the number of children in the node
    int numChildren = node->GetChildCount();
    //std::cout << "Name: " << node->GetName() << " Number of children: " << numChildren << std::endl;

    // Check if the node has a mesh component
    FbxMesh* nodeMesh = node->GetMesh();
    if (nodeMesh == NULL) {
        //std::cout << "Node has no mesh component." << std::endl;
    }
    else {
        // Create the mesh data
        outputScene.meshes.emplace_back(createMeshData(nodeMesh));
        
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

Mesh createMeshData(FbxMesh* inMesh) {
    Mesh outMesh;

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

            // Check if that position has been seen before
            if (seenVertices.find(vertex) == seenVertices.end()) {
                // Add the new vertex position and normal
                outMesh.vertexPositions.emplace_back(vertex);
                outMesh.vertexNormals.emplace_back(normal);
            
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


    if (numTriangles < 31) {
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
    }

    return outMesh;
}