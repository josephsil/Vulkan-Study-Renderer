#include <General/GLM_IMPL.h>
#include "AssetManager.h"
#include <Renderer/MeshCreation/meshData.h> // TODO JS: I want to separate the backing data from the scene 
#include <Renderer/TextureCreation/TextureData.h> // TODO JS: I want to separate the backing data from the scene 
#include <General/MemoryArena.h>
#include "engineGlobals.h"
#include "General/Array.h"

//No scale for now
void static_AllocateAssetMemory(MemoryArena::memoryArena* arena, AssetManager* assetManager)
{
    // arallel arrays per Light
    assetManager->texturesMetaData = Array(MemoryArena::AllocSpan<TextureMetaData>(arena, ASSET_MAX));
    assetManager->textures = Array(MemoryArena::AllocSpan<VkDescriptorImageInfo>(arena, ASSET_MAX));
    assetManager->backing_submeshes = Array(MemoryArena::AllocSpan<MeshData>(arena, ASSET_MAX));
    assetManager->subMeshGroups = Array(MemoryArena::AllocSpan<Array<uint32_t>>(arena, ASSET_MAX));
    for (auto& g : assetManager->subMeshGroups.getSubSpanToCapacity())
    {
        g = MemoryArena::AllocSpan<ID::SubMeshID>(arena, UINT8_MAX);
    }
    assetManager->meshBoundingSphereRad = Array(MemoryArena::AllocSpan<positionRadius>(arena, ASSET_MAX));
    assetManager->materials = Array(MemoryArena::AllocSpan<Material>(arena, OBJECT_MAX));
}


//TODO JS PRIMS:
//TODO JS Objects are currently what's drawn -- per object there's a mesh, materal, and translatio ninfo
//TODO JS: Easiest thing is to add the prims as new objects, and then later break the transform data apart from the object data
//TODO JS: not every objcet neeeds a transform -- prims should use a praent transform.
//TODO JS: So I'll do it in two steps -- first I'll add prims as objects, then ill split "drawables" ("models"?) and "transformablse" ("objects"?)
//So things like, get the index back from this and then index in to these vecs to update them
//At some point in the future I can replace this with a more sophisticated reference system if I need
//Even just returning a pointer is probably plenty, then I can sort the lists, prune stuff, etc.
ID::MaterialID AssetManager::AddMaterial(float roughness, float metallic, glm::vec3 color, textureSetIDs textureindex,
                              uint32_t pipeline)
{
    materials.push_back(Material{
        .shaderGroupIndex = pipeline, .diffuseIndex = static_cast<uint32_t>(textureindex.diffuseIndex), .specIndex =  static_cast<uint32_t>(textureindex.specIndex),
        .normalIndex =  static_cast<uint32_t>(textureindex.normIndex), .metallic = metallic, .roughness = roughness, .color = color
    });
    return (uint32_t)materials.size() - 1;
}

void AssetManager::Update()
{
    //noop
}

uint32_t AssetManager::getOffsetFromMeshID(int id)
{
    uint32_t indexcount = 0;
    for (int i = 0; i < id; i++)
    {
        indexcount += static_cast<uint32_t>(backing_submeshes[i].indices.size());
    }
    return indexcount;
}

uint32_t AssetManager::getIndexCount()
{
    uint32_t indexcount = 0;
    for (int i = 0; i < meshCount; i++)
    {
        indexcount += static_cast<uint32_t>(backing_submeshes[i].indices.size());
    }
    return indexcount;
}

uint32_t AssetManager::getVertexCount()
{
    uint32_t vertexCount = 0;
    for (int i = 0; i < meshCount; i++)
    {
        vertexCount += static_cast<uint32_t>(backing_submeshes[i].vertices.size());
    }
    return vertexCount;
}


size_t AssetManager::materialTextureCount()
{
    return textures.size();
}


ID::TextureID AssetManager::AddTexture(TextureData T)
{
    //What we need to render the texture. 
    textures.push_back(T.vkImageInfo);
    texturesMetaData.push_back(T.metaData);
    return static_cast<uint32_t>(textures.size()) - 1;
}

textureSetIDs AssetManager::AddTextureSet(TextureData D, TextureData S, TextureData N)
{
    auto dI = AddTexture(D);
    auto sI = AddTexture(S);
    auto nI = AddTexture(N);
    return {static_cast<uint32_t>(dI), static_cast<uint32_t>(sI), static_cast<uint32_t>(nI)};
}

ID::SubMeshID AssetManager::AddBackingMesh(MeshData M)
{
    backing_submeshes.push_back(M);
    meshBoundingSphereRad.push_back(MeshDataCreation::boundingSphereFromMeshBounds(M.boundsCorners));
    return meshCount++;
}

std::span<ID::SubMeshID> AssetManager::GetMesh(ID::SubMeshGroupID meshId)
{
    return subMeshGroups[meshId].getSpan();
}

ID::SubMeshGroupID AssetManager::AddSingleSubmeshMeshMesh(MeshData Ms)
{
    return AddMultiSubmeshMeshMesh({&Ms, 1});
}


ID::SubMeshGroupID AssetManager::AddMultiSubmeshMeshMesh(std::span<MeshData> Ms)
{
    auto& _span = subMeshGroups.push_back();

    auto start = backing_submeshes.back();
    for (auto m : Ms)
    {
        _span.push_back(AddBackingMesh(m));
        
    }
    return (uint32_t)(subMeshGroups.ct -1);
}

//TODO JS: 
positionRadius AssetManager::GetBoundingSphere(int idx)
{
    printf("NOT IMPLEMENTED: NEED TO USE CORRECT INDEX FOR GETBOUNDINGSPHERE\n");
    return meshBoundingSphereRad[idx];
}

struct sortData
{
    glm::float32_t* data;
    uint32_t ct;
};


void AssetManager::Cleanup()
{
}
