#pragma once
#include <General/MemoryArena.h>
#include <General/Array.h>
#include <Renderer/rendererGlobals.h>
#include "Renderer/RendererSharedTypes.h"
#include "Renderer/AssetManagerTypes.h"
#include "Renderer/MainRenderer/rendererStructs.h"
#include <Renderer/TextureTypes.h>
struct GPU_Bounds;
struct GPU_Bounds;
struct GPU_VertexData;
struct ImportMeshData;
struct preMeshletMesh;
struct GPU_BoundingSphere;


struct TextureMetaData;
struct preMeshletMesh;
struct VkDescriptorImageInfo;

struct AllocatedMeshData
{
    std::span<glm::vec4> vertPositions;
    std::span<GPU_VertexData> vertData;
    std::span<uint8_t> vertIndices;
};
// AssetManager contains all data (vertices, indices, meshlet descriptions, 
// material descriptions, etc) for rendering assets. This primary interacts with: 
// -The renderer, which copies data for binding and reads information about 
//  meshlets and textures throughout the frame update 
// -Asset Importers, which write data to the AssetManager. 
// Manager is a bit of a misnomer, this class mostly only *holds* data at the moment.
class AssetManager
{
public:
    size_t GetTextureCount();
    AssetManager();

    struct RendererMeshData
    {
        // Geometry Data
		Array<glm::vec4> vertPositions; 
		Array<GPU_VertexData> vertData; 
		

        // Objects/Meshlets
        Array<uint8_t> vertIndices;
        Array<MeshletData> meshletInfo;
        Array<GPU_BoundingSphere> boundingSpheres;
        Array<GPU_Bounds> GPU_Boundses; 
        Array<PerSubmeshData> perSubmeshData;
    };
    
	MemoryArena::memoryArena allocator;
    uint32_t submeshCount = 0;
    size_t globalVertexCount = 0;
    size_t globalIndexCount = 0;
    size_t globalMeshletcount = 0;

    Array<Material> materials;
    Array<VkDescriptorImageInfo> textures; //What we need to render the textures
    Array<TextureMetaData> texturesMetaData; //Other info about the textures -- parallel array
    Array<Array<ID::SubMeshID>>  subMeshGroups;
    RendererMeshData meshData;


    ID::MaterialID AddMaterial(float roughness, float metallic, glm::vec3 color, textureSetIDs textureindex, uint32_t pipeline);
    uint32_t GetIndexCount();
    uint32_t GetVertexCount();
    uint32_t GetOffsetFromMeshID(int submesh, int meshlet);

    ID::TextureID AddTexture(TextureData T);
    textureSetIDs AddTextureSet(TextureData D, TextureData S, TextureData N);

    AllocatedMeshData RequestMeshMemory(uint32_t vertCt, uint32_t indexCt);
    ID::SubMeshID AddMesh(std::span<preMeshletMesh> SubmeshMeshlets);
    ID::SubMeshID AddMesh(ImportMeshData importMesh);
    ID::SubMeshGroupID AddMultiSubmeshMesh(std::span<ImportMeshData> Submeshes);
    
    void Cleanup();

};
AssetManager* GetGlobalAssetManager();
void GpuVertexFromImportVertex(Vertex& input, glm::vec4& outptuPos, GPU_VertexData& outputVert);
