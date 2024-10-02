#include "FBXFileLoader.hpp"

int main() {
	// Load the FBX file and populate a scene struct that can
	// be used for PBR.
	fbx::Scene newScene = fbx::loadFBXFile("SunTemple/SunTemple.fbx");

	return 1;
}