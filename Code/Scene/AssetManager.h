#pragma once
#include <General/Array.h>
#include <Renderer/rendererGlobals.h>

#include "General/LinearDictionary.h"
#include "Renderer/RendererSharedTypes.h"
#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/AssetManagerTypes.h"
#include "Renderer/MainRenderer/rendererStructs.h"
//
struct positionRadius;

namespace MemoryArena
{
    struct memoryArena;
}

struct textureSetIDs
{
    ID::TextureID diffuseIndex;
    ID::TextureID specIndex;
    ID::TextureID normIndex;
};

struct offsetData
{
    size_t vertex_start;
    size_t index_start;  
};

//Objects have like, transformation info, ref to their mesh, ref to their material
//Not sure on ref to material. Really I only have one shader right now
struct TextureMetaData;
struct MeshData;
struct VkDescriptorImageInfo;
class AssetManager;
void static_AllocateAssetMemory(MemoryArena::memoryArena* arena, AssetManager* assetManager);
class AssetManager
{
public:
    size_t materialTextureCount();


#pragma region scene

#pragma endregion

#pragma region RendererData




    int meshCount = 0;
    Array<Material> materials;
    Array<VkDescriptorImageInfo> textures; //What we need to render the textures
    Array<TextureMetaData> texturesMetaData; //Other info about the texture in parallel array
    Array<MeshData> backing_submeshes;
    Array<offsetData> backing_submeshOffsetData;
    Array<positionRadius> meshBoundingSphereRad;
    Array<Array<ID::SubMeshID>>  subMeshGroups;
    Array<std::span<meshletIndexInfo>>  subMeshMeshletInfo;


    //Returns the index to the object in the vectors
    ID::MaterialID AddMaterial(float roughness, float metallic, glm::vec3 color, textureSetIDs textureindex, uint32_t pipeline);
    uint32_t getIndexCount();
    uint32_t getVertexCount();
    uint32_t getOffsetFromMeshID(int id);
    positionRadius GetBoundingSphere(int idx); //TODO JS: Change to use mesh idx
    //TODO JS: these are temporary
    ID::TextureID AddTexture(TextureData T);
    textureSetIDs AddTextureSet(TextureData D, TextureData S, TextureData N);
    ID::SubMeshID AddBackingMesh(MeshData M);
    std::span<ID::SubMeshID> GetMesh( ID::SubMeshGroupID meshId);
    ID::SubMeshGroupID AddSingleSubmeshMeshMesh(std::span<MeshData> Meshlets, meshletIndexInfo meshletInfo);
    ID::SubMeshGroupID AddMultiSubmeshMeshMesh(std::span<std::span<MeshData>> Submeshes, std::span<meshletIndexInfo> meshletInfo);

    void Update();
    void Cleanup();

#pragma endregion
};
