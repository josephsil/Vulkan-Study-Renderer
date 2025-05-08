#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_GTC_quaternion
#include "glm/glm.hpp"
#include "AssetManager.h"
#include <Renderer/MeshData.h> // TODO JS: I want to separate the backing data from the scene 
#include <Renderer/TextureData.h> // TODO JS: I want to separate the backing data from the scene 
#include <General/MemoryArena.h>
#include "engineGlobals.h"
#include "General/Array.h"

//No scale for now
void static_AllocateAssetMemory(MemoryArena::memoryArena* arena, AssetManager* scene)
{
    // arallel arrays per Light
    scene->texturesMetaData = Array(MemoryArena::AllocSpan<TextureMetaData>(arena, ASSET_MAX));
    scene->textures = Array(MemoryArena::AllocSpan<VkDescriptorImageInfo>(arena, ASSET_MAX));
    scene->backing_meshes = Array(MemoryArena::AllocSpan<MeshData>(arena, ASSET_MAX));
    scene->meshBoundingSphereRad = Array(MemoryArena::AllocSpan<positionRadius>(arena, ASSET_MAX));

    scene->materials = Array(MemoryArena::AllocSpan<Material>(arena, OBJECT_MAX));
}


//TODO JS PRIMS:
//TODO JS Objects are currently what's drawn -- per object there's a mesh, materal, and translatio ninfo
//TODO JS: Easiest thing is to add the prims as new objects, and then later break the transform data apart from the object data
//TODO JS: not every objcet neeeds a transform -- prims should use a praent transform.
//TODO JS: So I'll do it in two steps -- first I'll add prims as objects, then ill split "drawables" ("models"?) and "transformablse" ("objects"?)
//So things like, get the index back from this and then index in to these vecs to update them
//At some point in the future I can replace this with a more sophisticated reference system if I need
//Even just returning a pointer is probably plenty, then I can sort the lists, prune stuff, etc.
size_t AssetManager::AddMaterial(float roughness, float metallic, glm::vec3 color, textureSetIDs textureindex,
                              uint32_t pipeline)
{
    materials.push_back(Material{
        .shaderGroupIndex = pipeline, .diffuseIndex = static_cast<uint32_t>(textureindex.diffuseIndex), .specIndex =  static_cast<uint32_t>(textureindex.specIndex),
        .normalIndex =  static_cast<uint32_t>(textureindex.normIndex), .metallic = metallic, .roughness = roughness, .color = color
    });
    return materials.size() - 1;
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
        indexcount += static_cast<uint32_t>(backing_meshes[i].indices.size());
    }
    return indexcount;
}

uint32_t AssetManager::getIndexCount()
{
    uint32_t indexcount = 0;
    for (int i = 0; i < meshCount; i++)
    {
        indexcount += static_cast<uint32_t>(backing_meshes[i].indices.size());
    }
    return indexcount;
}

uint32_t AssetManager::getVertexCount()
{
    uint32_t vertexCount = 0;
    for (int i = 0; i < meshCount; i++)
    {
        vertexCount += static_cast<uint32_t>(backing_meshes[i].vertices.size());
    }
    return vertexCount;
}


size_t AssetManager::materialTextureCount()
{
    return textures.size();
}


size_t AssetManager::AddTexture(TextureData T)
{
    //What we need to render the texture. 
    textures.push_back(T.vkImageInfo);
    texturesMetaData.push_back(T.metaData);
    return textures.size() - 1;
}

textureSetIDs AssetManager::AddTextureSet(TextureData D, TextureData S, TextureData N)
{
    auto dI = AddTexture(D);
    auto sI = AddTexture(S);
    auto nI = AddTexture(N);
    return {static_cast<uint32_t>(dI), static_cast<uint32_t>(sI), static_cast<uint32_t>(nI)};
}

size_t AssetManager::AddBackingMesh(MeshData M)
{
    backing_meshes.push_back(M);
    meshBoundingSphereRad.push_back(boundingSphereFromMeshBounds(M.boundsCorners));
    return meshCount++;
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
