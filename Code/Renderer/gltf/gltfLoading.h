#pragma once
#include <span>
#include <glm/fwd.hpp>

#include <Renderer/RendererContext.h>
#include <tiny_gltf.h>
#include <glm/mat4x4.hpp>

struct TextureData;
struct MeshData;
struct gltfNode
{
    int meshidx;
    int matidx;
    glm::mat4 tform;
};

struct material
{
    glm::vec3 baseColorFactor;  
    glm::float32_t metallicFactor;   
    glm::float32_t roughnessFactor;
    glm::float32_t normalStrength;
    glm::float32_t occlusionStrength;
    int diffIndex;
    int specIndex;
    int normIndex;
    int occlusionIndex;
    //texture indices?
    //roughness/metal?
};
struct gltfdata
{
    std::span<MeshData> meshes;
    std::span<TextureData> textures;
    std::span<material> materials;
    std::span<gltfNode> objects;
    //std::span<tinygltf::Light> lights;
};

gltfdata GltfLoadMeshes(RendererContext handles, const char* gltfpath);
