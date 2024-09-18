workspace "FBXFileLoader"
    language "C++"
    cppdialect "C++20"
    platforms { "x64" }
    configurations { "debug", "release" }

    --configurations
    filter "debug"
        defines { "_DEBUG=1" }
        symbols "On"

    filter "release"
        defines { "NDEBUG=1" }
        optimize "On"

    filter "*"

    -- Include files (The default directory)
    includedirs{"C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.7/include", "ExternalLibraries/glm"}

project "FileLoader"
    kind "ConsoleApp"
    location "src"
    files {"src/**.cpp", "src/**.hpp"}