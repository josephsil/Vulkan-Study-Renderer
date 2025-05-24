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
    // this->meshData.vertices = MemoryArena::AllocSpan<Vertex>(&this->allocator,350000); 
    this->meshData.vertPositions = MemoryArena::AllocSpan<glm::vec4>(&this->allocator,350000); 
    this->meshData.vertData = MemoryArena::AllocSpan<gpuvertex>(&this->allocator,350000);     
    this->meshData.vertIndices = MemoryArena::AllocSpan<uint8_t>(&this->allocator, 950000);
    this->meshData.meshletInfo =  MemoryArena::AllocSpan<MeshletData>(&this->allocator, MESHLET_MAX_PER * ASSET_MAX);
    this->meshData.boundingInfo =  MemoryArena::AllocSpan<positionRadius>(&this->allocator, MESHLET_MAX_PER * ASSET_MAX);
    this->meshData.perSubmeshData = Array(MemoryArena::AllocSpan<PerSubmeshData>(&this->allocator, MESHLET_MAX_PER * ASSET_MAX));
    this->texturesMetaData = Array(MemoryArena::AllocSpan<TextureMetaData>(&this->allocator, ASSET_MAX));
    this->textures = Array(MemoryArena::AllocSpan<VkDescriptorImageInfo>(&this->allocator, ASSET_MAX));
    // this->TODO_UNUSEDbacking_submeshes = Array(MemoryArena::AllocSpan<MeshData>(&this->allocator, MESHLET_MAX_PER * ASSET_MAX));
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
//
uint32_t AssetManager::getOffsetFromMeshID(int submesh, int meshlet)
{
    //TODO remove
    return (uint32_t)meshData.perSubmeshData[submesh].firstMeshletIndex + meshlet;
}

uint32_t AssetManager::getIndexCount()
{
    return (uint32_t)globalIndexCount;
    
}

uint32_t AssetManager::getVertexCount()
{

    return (uint32_t)globalVertexCount;
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

AllocatedMeshData AssetManager::RequestMeshMemory(uint32_t vertCt, uint32_t indexCt)
{

    return
    {
    .vertPositions = meshData.vertPositions.pushUninitializedSpan(vertCt),
    .vertData = meshData.vertData.pushUninitializedSpan(vertCt),
    .vertIndices = meshData.vertIndices.pushUninitializedSpan(indexCt)
    };
    
}

void SetVertexDataFromLoadingMeshVertex(Vertex& input, glm::vec4& outptuPos, gpuvertex& outputVert)
{
    outptuPos = input.pos;
    outputVert =            {
        .texCoord = input.texCoord,
        .normal = input.normal,
        .tangent = input.tangent};
}

//It would be nicer if the assetmanager provided asset importers memory to load their verts into, and all that got passed around was layout/config/offset data
ID::SubMeshID AssetManager::AddMesh(ImportMeshData importMesh)
{

    Array offsets = meshData.meshletInfo.pushUninitializedSpan(importMesh.meshletCount);
    Array boundingInfo = meshData.boundingInfo.pushUninitializedSpan(importMesh.meshletCount);

    auto meshVertexOffset = globalVertexCount;
    for (uint32_t j = 0; j < importMesh.meshletCount; j++)
    {
        boundingInfo.push_back(MeshDataCreation::boundingSphereFromMeshBounds(importMesh.meshletBounds[j]));
        offsets.push_back({
            importMesh.meshletVertexOffsets[j] + meshVertexOffset, globalIndexCount, importMesh.indexCounts[j]
        });
        globalIndexCount += importMesh.indexCounts[j];
    }

     this->meshData.perSubmeshData.push_back() =
        PerSubmeshData{
            .meshletCt =  importMesh.meshletCount,
            .firstMeshletIndex = globalMeshletcount,
        };
    globalMeshletcount += importMesh.meshletCount;
    globalVertexCount += importMesh.vertexct;
    return submeshCount++;
}

ID::SubMeshGroupID AssetManager::AddMultiSubmeshMeshMesh2(std::span<ImportMeshData> Submeshes)
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
