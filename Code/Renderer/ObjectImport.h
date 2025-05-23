#pragma once
#include <cstdint>
#include <General/GLM_impl.h>

#include "AssetManagerTypes.h"
#include "General/MemoryArena.h"
#include "MainRenderer/rendererStructs.h"


struct MeshData2;
struct TextureData;
class AssetManager;
struct Scene;


struct MeshData;

namespace ObjectImport
{


    //TODO JS: should have assetmanger return this
    struct DefaultTextures
    {
        ID::TextureID defaultTexture;
        ID::TextureID defaultSPec;
        ID::TextureID defaultNormal;
    };

    struct MeshObject
    {
        int meshidx;
        // glm::mat4 placeholder;
        std::span<int> children;
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
    };


    struct Material
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
    
    
    struct Mesh
    {
        std::span<MeshData2> submeshes;
        std::span<uint32_t> submeshMaterialIndices;
    };

    
    struct ImportedObjectData
    {
        //TODO JS: to span of spans for submeshes 
        std::span<Mesh> meshes;
        std::span<std::span<meshletIndexInfo>> meshletInfo;
        std::span<TextureData> textures;
        std::span<Material> materials;
        std::span<MeshObject> objects;
        //std::span<tinygltf::Light> lights;
    };

    void CreateObjectAssets(ArenaAllocator& arena, Scene& scene, AssetManager& assetManager, ImportedObjectData& gltf, DefaultTextures defaults);
    void AddObjectToScene(ArenaAllocator& arena, Scene& scene, ImportedObjectData& gltf);


    
};
