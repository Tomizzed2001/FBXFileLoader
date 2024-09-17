#include "FBXFileLoader.hpp"

int main() {
	// Load the FBX file and populate a scene struct that can
	// be used for PBR.
	Scene newScene = loadFBXFile("SunTemple/SunTemple.fbx");

	return 1;
}