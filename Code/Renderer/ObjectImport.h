#pragma once
#include <cstdint>
#include <General/GLM_impl.h>

#include "AssetManagerTypes.h"
#include "General/MemoryArena.h"
#include "MainRenderer/rendererStructs.h"


struct ImportMeshData;
struct TextureData;
class AssetManager;
struct Scene;
struct preMeshletMesh;

// This is used to take 'object like' imported files (gltfs,)
// and add them to the scene.
// A little janky, some weirdness spread between here and gltf import
// This functionality is duplicated with the direct "scene->AddObject"
// calls which are used for other objects -- eventually I want to unify them
namespace ObjectImport
{
    struct DefaultTextures
    {
        ID::TextureID defaultTexture;
        ID::TextureID defaultSPec;
        ID::TextureID defaultNormal;
    };

    struct MeshObject
    {
        int meshidx;
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
    };
    
    
    struct Mesh
    {
        std::span<ImportMeshData> submeshes;
        std::span<uint32_t> submeshMaterialIndices;
    };

    
    struct ImportedObjectData
    {
        std::span<Mesh> meshes;
        std::span<std::span<meshletIndexInfo>> meshletInfo;
        std::span<TextureData> textures;
        std::span<Material> materials;	
        std::span<MeshObject> objects;
    };

	void CreateObjectAssetsAndAddToScene(ArenaAllocator& arena, Scene& scene, 
							AssetManager& assetManager, ImportedObjectData& gltf, 
							DefaultTextures defaults);


    
};
