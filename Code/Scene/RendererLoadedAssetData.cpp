#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_GTC_quaternion
#include "glm/glm.hpp"
#include "RendererLoadedAssetData.h"
#include <Renderer/MeshData.h> // TODO JS: I want to separate the backing data from the scene 
#include <Renderer/TextureData.h> // TODO JS: I want to separate the backing data from the scene 
#include <General/MemoryArena.h>
#include "engineGlobals.h"
#include "General/Array.h"

//No scale for now
void InitializeRendererSceneData(MemoryArena::memoryArena* arena, RendererLoadedAssetData* scene)
{

    // arallel arrays per Light
    scene->backing_diffuse_textures = Array(MemoryArena::AllocSpan<TextureData>(arena, ASSET_MAX));
    scene->backing_meshes =  Array(MemoryArena::AllocSpan<MeshData>(arena, ASSET_MAX));
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
int RendererLoadedAssetData::AddMaterial(float roughness, float metallic, glm::vec3 color, textureSetIDs textureindex, uint32_t pipeline)
{
  materials.push_back(Material{
        .pipelineidx = pipeline, .diffuseIndex = textureindex.diffuseIndex, .specIndex   = textureindex.specIndex, .normalIndex = textureindex.normIndex, .metallic = metallic, .roughness = roughness, .color = color
    });
    return materials.size() -1;
}
void RendererLoadedAssetData::Update()
{
    //noop
}

uint32_t RendererLoadedAssetData::getOffsetFromMeshID(int id)
{
    uint32_t indexcount = 0;
    for (int i = 0; i < id; i++)
    {
        indexcount += (uint32_t)backing_meshes[i].indices.size();
    }
    return indexcount;
}

uint32_t RendererLoadedAssetData::getIndexCount()
{
    uint32_t indexcount = 0;
    for (int i = 0; i < meshCount; i++)
    {
        indexcount += (uint32_t)backing_meshes[i].indices.size();
    }
    return indexcount;
}

uint32_t RendererLoadedAssetData::getVertexCount()
{
    uint32_t vertexCount = 0;
    for (int i = 0; i < meshCount; i++)
    {
        vertexCount += (uint32_t)backing_meshes[i].vertices.size();
    }
    return vertexCount;
}





int RendererLoadedAssetData::materialTextureCount()
{
    return  backing_diffuse_textures.size();
}


int RendererLoadedAssetData::AddTexture(TextureData T)
{
    backing_diffuse_textures.push_back(T);
    return backing_diffuse_textures.size() -1;
}
textureSetIDs RendererLoadedAssetData::AddTextureSet(TextureData D, TextureData S, TextureData N)
{
    backing_diffuse_textures.push_back(D);
    backing_diffuse_textures.push_back(S);
    backing_diffuse_textures.push_back(N);
    return {(uint32_t)backing_diffuse_textures.size() -3, (uint32_t)backing_diffuse_textures.size() -2, (uint32_t)backing_diffuse_textures.size() -1};
}

int RendererLoadedAssetData::AddBackingMesh(MeshData M)
{
    backing_meshes.push_back(M);
    meshBoundingSphereRad.push_back(boundingSphereFromMeshBounds(M.boundsCorners));
    return meshCount ++;
}

//TODO JS: 
positionRadius RendererLoadedAssetData::GetBoundingSphere(int idx)
{
    printf("NOT IMPLEMENTED: NEED TO USE CORRECT INDEX FOR GETBOUNDINGSPHERE\n");
    return meshBoundingSphereRad[idx];
}

struct sortData
{
    glm::float32_t* data;
    uint32_t ct;
};


void RendererLoadedAssetData::Cleanup()
{
}
