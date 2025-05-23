#include <General/GLM_IMPL.h>
#include "AssetManager.h"
#include <Renderer/MeshCreation/meshData.h> // TODO JS: I want to separate the backing data from the scene 
#include <Renderer/TextureCreation/TextureData.h> // TODO JS: I want to separate the backing data from the scene 
#include <General/MemoryArena.h>
#include "engineGlobals.h"
#include "General/Array.h"



//No scale for now
AssetManager::AssetManager()
{

    MemoryArena::initialize(&this->allocator, 1000000 * 500); //500mb
    // arallel arrays per Light
    //1000000
    // this->meshData.vertices = MemoryArena::AllocSpan<Vertex>(&this->allocator,VERTEX_MAX); 
    // this->meshData.vertPositions = MemoryArena::AllocSpan<glm::vec4>(&this->allocator,VERTEX_MAX); 
    // this->meshData.vertData = MemoryArena::AllocSpan<gpuvertex>(&this->allocator,VERTEX_MAX);     
    // this->meshData.vertIndices = MemoryArena::AllocSpan<uint32_t>(&this->allocator, INDEX_MAX); 
    this->texturesMetaData = Array(MemoryArena::AllocSpan<TextureMetaData>(&this->allocator, ASSET_MAX));
    this->textures = Array(MemoryArena::AllocSpan<VkDescriptorImageInfo>(&this->allocator, ASSET_MAX));
    // this->TODO_UNUSEDbacking_submeshes = Array(MemoryArena::AllocSpan<MeshData>(&this->allocator, MESHLET_MAX_PER * ASSET_MAX));
    this->perSubmeshData = Array(MemoryArena::AllocSpan<PerSubmeshData>(&this->allocator, MESHLET_MAX_PER * ASSET_MAX));
    this->subMeshGroups = Array(MemoryArena::AllocSpan<Array<uint32_t>>(&this->allocator, ASSET_MAX));


    for (auto& g : this->subMeshGroups.getSubSpanToCapacity())
    {
        g = MemoryArena::AllocSpan<ID::SubMeshID>(&this->allocator, UINT8_MAX);
    }
    this->materials = Array(MemoryArena::AllocSpan<Material>(&this->allocator, OBJECT_MAX));
}

AssetManager GlobalAssetManager;

AssetManager* GetGlobalAssetManager()
{
    return &GlobalAssetManager;
}

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

uint32_t AssetManager::getOffsetFromMeshID(int submesh, int meshlet)
{
    //TODO MESHLET: This function should be able to 
    return (uint32_t)perSubmeshData[submesh].meshletOffsets[meshlet].index_start;
}

uint32_t AssetManager::getIndexCount()
{
    uint32_t indexcount = 0;
    for (uint32_t i = 0; i < submeshCount; i++)
    {
    for(size_t j = 0; j < perSubmeshData[i].meshlets.size(); j++)
       {
       
        indexcount += static_cast<uint32_t>( perSubmeshData[i].meshlets[j].indices.size());
    }
    }
    return indexcount;
}

uint32_t AssetManager::getVertexCount()
{

    return (uint32_t)vertexCount;
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

ID::SubMeshID AssetManager::AddMesh(std::span<MeshData> SubmeshMeshlets)
{

    Array offsets = MemoryArena::AllocSpan<offsetData>(&allocator, SubmeshMeshlets.size());
    Array meshDatas = MemoryArena::AllocSpan<MeshData>(&allocator, SubmeshMeshlets.size());
    Array boundingInfo = MemoryArena::AllocSpan<positionRadius>(&allocator, SubmeshMeshlets.size());

    for(int i =0; i < SubmeshMeshlets.size(); i++)
    {
        auto& M = SubmeshMeshlets[i];
        meshDatas.push_back(M);
        offsets.push_back({vertexCount, indexCount, meshletCount++});
        boundingInfo.push_back(MeshDataCreation::boundingSphereFromMeshBounds(M.boundsCorners));
        meshletCount++;
        indexCount +=  M.indices.size();
        vertexCount += M.vertices.size();
    }
     perSubmeshData.push_back() =
       PerSubmeshData {
        .meshlets = meshDatas.getSpan(),
        .meshletOffsets = offsets.getSpan(),
        .boundingInfo = boundingInfo.getSpan(),
        };
    
    return submeshCount++;
}

ID::SubMeshGroupID AssetManager::AddMultiSubmeshMeshMesh(std::span<std::span<MeshData>> Submeshes)
{
    auto& _span = subMeshGroups.push_back();

    int i = 0;
    for(auto& submesh: Submeshes)
    {
        _span.push_back(AddMesh(submesh));

    }
    return (uint32_t)(subMeshGroups.ct -1);
}


//TODO JS: 
// positionRadius AssetManager::GetBoundingSphere(int idx)
// {
//     printf("NOT IMPLEMENTED: NEED TO USE CORRECT INDEX FOR GETBOUNDINGSPHERE\n");
//     return TODO_MOVET_TO_MESHDATA_meshBoundingSphereRad[idx];
// }

struct sortData
{
    glm::float32_t* data;
    uint32_t ct;
};


void AssetManager::Cleanup()
{
}
