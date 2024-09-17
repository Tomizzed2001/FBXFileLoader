#include "FBXFileLoader.hpp"

#include <fbxsdk.h>
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

    // Destroy the SDK manager and all the other objects it was handling.
    memoryManager->Destroy();

    Scene outputScene;

    return outputScene;
}