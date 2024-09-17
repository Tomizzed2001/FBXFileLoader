#pragma once

/// A set of structs used to hold the information from the FBX file.

struct Texture
{

};

struct Material
{

};

struct Mesh
{

};

struct Scene
{

};

/// <summary>
/// Loads a given FBX file and creates a set of data that can be used for 
/// PBR.
/// </summary>
/// <param name="filename">The .fbx file path</param>
/// <returns>A Scene structure</returns>
Scene loadFBXFile(const char* filename);
