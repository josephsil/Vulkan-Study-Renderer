#include <General/GLM_impl.h>
#include "AssetManager.h"
#include <Renderer/MeshCreation/MeshData.h> // TODO JS: I want to separate the backing data from the scene 
#include <Renderer/TextureCreation/TextureData.h> // TODO JS: I want to separate the backing data from the scene 
#include <General/MemoryArena.h>
#include "engineGlobals.h"
#include "General/Array.h"



//No scale for now
AssetManager::AssetManager()
{

    MemoryArena::initialize(&this->allocator, 1000000 * 500); //500mb
    this->meshData.vertPositions = MemoryArena::AllocSpan<glm::vec4>(&this->allocator,350000); 
    this->meshData.vertData = MemoryArena::AllocSpan<GPU_VertexData>(&this->allocator,350000);     
    this->meshData.vertIndices = MemoryArena::AllocSpan<uint8_t>(&this->allocator, 950000);
    this->meshData.meshletInfo =  MemoryArena::AllocSpan<MeshletData>(&this->allocator, MESHLET_MAX_PER * ASSET_MAX);
    this->meshData.boundingSpheres =  MemoryArena::AllocSpan<GPU_BoundingSphere>(&this->allocator, MESHLET_MAX_PER * ASSET_MAX);
    this->meshData.GPU_Boundses =  MemoryArena::AllocSpan<GPU_Bounds>(&this->allocator, MESHLET_MAX_PER * ASSET_MAX);
    this->meshData.perSubmeshData = Array(MemoryArena::AllocSpan<PerSubmeshData>(&this->allocator, MESHLET_MAX_PER * ASSET_MAX));
    this->texturesMetaData = Array(MemoryArena::AllocSpan<TextureMetaData>(&this->allocator, ASSET_MAX));
    this->textures = Array(MemoryArena::AllocSpan<VkDescriptorImageInfo>(&this->allocator, ASSET_MAX));
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

ID::MaterialID AssetManager::AddMaterial(float roughness, float metallic, glm::vec3 color, textureSetIDs textureindex,
										 uint32_t pipeline)
{
	materials.push_back(Material {
		.shaderGroupIndex = pipeline, .diffuseIndex = static_cast<uint32_t>(textureindex.diffuseIndex), .specIndex = static_cast<uint32_t>(textureindex.specIndex),
		.normalIndex = static_cast<uint32_t>(textureindex.normIndex), .metallic = metallic, .roughness = roughness, .color = color
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

void SetVertexDataFromLoadingMeshVertex(Vertex& input, glm::vec4& outptuPos, GPU_VertexData& outputVert)
{
    outptuPos = input.pos;
    outputVert =            {
        .uv0 = input.texCoord,
        .normal = input.normal,
        .Tangent =  input.tangent
       };
}

ID::SubMeshID AssetManager::AddMesh(ImportMeshData importMesh)
{

    Array offsets = meshData.meshletInfo.pushUninitializedSpan(importMesh.meshletCount);
    Array spheres = meshData.boundingSpheres.pushUninitializedSpan(importMesh.meshletCount);
    Array GPU_Boundses = meshData.GPU_Boundses.pushUninitializedSpan(importMesh.meshletCount);

    auto meshVertexOffset = globalVertexCount;
    for (uint32_t j = 0; j < importMesh.meshletCount; j++)
    {
        spheres.push_back(importMesh.meshletBounds[j]);
        auto& vecCenter = importMesh.meshletBounds[j].center;
         GPU_Boundses.push_back({ vecCenter+ (glm::vec4(-1) * ( importMesh.meshletBounds[j].radius)),
        vecCenter+ (glm::vec4(1) * ( importMesh.meshletBounds[j].radius))
             });
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


struct sortData
{
    glm::float32_t* data;
    uint32_t ct;
};


void AssetManager::Cleanup()
{
}
