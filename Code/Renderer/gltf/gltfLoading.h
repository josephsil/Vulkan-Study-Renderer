#pragma once
#include <span>

#include "../RendererContext.h"
#include "../../../tinygltf/tiny_gltf.h"

struct TextureData;
struct MeshData;

struct gltfdata
{
    std::span<MeshData> meshes;
    std::span<TextureData> textures;
    std::span<tinygltf::Light> lights;
    std::span<tinygltf::Camera> cameras;
};

gltfdata GltfLoad(RendererContext handles, std::string gltfpath)
{
    //TODO JS: idea here is parse out te gltf to formats I can use, then pass it to the scene somehow
    //Currently using the gltf native version of light and camera, but will probably refactor to use my own so I can pass the struct out elsewhere
    //Ideally nowhere outside of here should have a tinygltf dependency 
    gltfdata output = {};


    return output;
    
}