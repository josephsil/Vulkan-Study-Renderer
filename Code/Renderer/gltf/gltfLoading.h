#pragma once
#include <span>
#include <General/GLM_IMPL.h>

#include "Renderer/TextureCreation/TextureData.h"


struct PerThreadRenderContext;
struct MeshData;
struct TextureMetaData;

struct gltfMesh
{
    std::span<MeshData> submeshes;
    std::span<uint32_t> materialIndices;
};

struct gltfNode
{
    int meshidx;

    // glm::mat4 placeholder;
    std::span<int> children;
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
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

gltfdata GltfLoadMeshes(PerThreadRenderContext handles, const char* gltfpath);
