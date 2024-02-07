#pragma once
#include <span>
#include <glm/fwd.hpp>

#include "../RendererContext.h"
#include "../../../tinygltf/tiny_gltf.h"
#include <glm/mat4x4.hpp>

struct TextureData;
struct MeshData;
struct gltfNode
{
    uint32_t meshidx;
    uint32_t matidx;
    glm::mat4 tform;
};
struct material
{
    glm::vec3 baseColorFactor;  
    glm::float32_t metallicFactor;   
    glm::float32_t roughnessFactor;
    glm::float32_t normalStrength;
    glm::float32_t occlusionStrength;
    uint32_t diffIndex;
    uint32_t specIndex;
    uint32_t normIndex;
    uint32_t occlusionIndex;
    //texture indices?
    //roughness/metal?
};
struct gltfdata
{
    std::span<MeshData> meshes;
    std::span<TextureData> textures;
    std::span<tinygltf::Light> lights;
    std::span<gltfNode> objects;
};

gltfdata GltfLoadMeshes(RendererContext handles, const char* gltfpath);
