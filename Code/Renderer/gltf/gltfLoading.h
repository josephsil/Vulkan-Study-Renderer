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

gltfdata GltfLoadMeshes(RendererContext handles, const char* gltfpath);
