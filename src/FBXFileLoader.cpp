#include "FBXFileLoader.hpp"

#include "gtx/string_cast.hpp"

#include <filesystem>
#include <iostream>

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
    std::cout << "Name: " << node->GetName() << " Number of children: " << numChildren << std::endl;

    // Check if the node has a mesh component
    FbxMesh* nodeMesh = node->GetMesh();
    if (nodeMesh == NULL) {
        std::cout << "Node has no mesh component." << std::endl;
    }
    else {
        // Create the mesh data
        outputScene.meshes.emplace_back(createMesh(nodeMesh));
        
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

Mesh createMesh(FbxMesh* inMesh) {
    Mesh outMesh;

    // Get the number of triangles in the mesh and all the triangles
    int numTriangles = inMesh->GetPolygonCount();
    std::cout << "Number of triangles: " << numTriangles << std::endl;

    // Get the number of vertices and all vertices
    int numVertices = inMesh->GetControlPointsCount();
    FbxVector4* fbxVertices = inMesh->GetControlPoints();
    std::cout << "Number of vertices: " << numVertices << std::endl;

    // Get the number of indices and all inddices
    int numIndices = inMesh->GetPolygonVertexCount();
    int* indices = inMesh->GetPolygonVertices();
    std::cout << "Number of indices: " << numIndices << std::endl;

    // Place each of the indices into the mesh data structure
    for (int i = 0; i < numIndices; i++) {
        // Place the indices in the array (-1 so as it starts at 1 not 0)
        outMesh.vertexIndices.emplace_back(indices[i] - 1);
        /*  DEBUG LINE
        if (numTriangles < 20) {
            std::cout << indices[i] << ", ";
        }
        */
    }
    //std::cout << std::endl;

    // For each of the vertices (control points in the mesh)
    for (int i = 0; i < numVertices; i++) {
        // Get the vertex position and add it to the array
        outMesh.vertexPositions.emplace_back(glm::vec3(fbxVertices[i][0], fbxVertices[i][1], fbxVertices[i][2]));
        // For each of the normals in the mesh
        int numNormals = inMesh->GetElementNormalCount();
        for (int j = 0; j < numNormals; j++) {
            // Get the vertex normal(s)
            FbxGeometryElementNormal* normals = inMesh->GetElementNormal(j);
            // If normals are defined per vertex / control point
            if (normals->GetMappingMode() == FbxGeometryElement::eByControlPoint)
            {
                // If normals are indexed the same as vertex indices
                if (normals->GetReferenceMode() == FbxGeometryElement::eDirect) {
                    // Finally get the normal
                    FbxVector4 normal = normals->GetDirectArray().GetAt(i);
                    // Push the normal into the output mesh
                    outMesh.vertexNormals.emplace_back(glm::vec3(normal[0], normal[1], normal[2]));
                    /* DEBUG LINE
                    if (numTriangles < 20) {
                        std::cout << glm::to_string(glm::vec3(normal[0], normal[1], normal[2])) << ", ";
                    }
                    */
                }
            }
        }
        /*  DEBUG LINE
        if (numTriangles < 20) {
            std::cout << glm::to_string(vertexPosition) << ", ";
        }
        */
    }
    //std::cout << std::endl;

    return outMesh;
}

