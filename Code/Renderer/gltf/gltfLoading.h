#pragma once
#include <span>
#include <glm/fwd.hpp>

#include <Renderer/RendererContext.h>
#include <tiny_gltf.h>
#include <glm/mat4x4.hpp>

struct TextureData;
struct MeshData;

struct gltfMesh
{
     std::span<MeshData> submeshes;
     std::span<uint32_t> materialIndices;
};
struct gltfNode
{
    int meshidx;
    glm::mat4 tform;
    std::span<int> children;
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
    //TODO JS: to span of spans for submeshes 
    std::span<gltfMesh> meshes;

    std::span<TextureData> textures;
    std::span<material> materials;
    std::span<gltfNode> objects;
    //std::span<tinygltf::Light> lights;
};

gltfdata GltfLoadMeshes(RendererContext handles, const char* gltfpath);
