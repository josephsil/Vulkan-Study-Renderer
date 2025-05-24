#pragma once
#include <General/Array.h>
#include <Renderer/rendererGlobals.h>

#include "General/LinearDictionary.h"
#include "Renderer/RendererSharedTypes.h"
#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/AssetManagerTypes.h"
#include "Renderer/MainRenderer/rendererStructs.h"
struct boundingBox;
struct gpuvertex;
struct ImportMeshData;
//
struct preMeshletMesh;
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

struct MeshletData
{
    size_t meshletVertexOffset; //Offset for this meshlet's verts within the global vertex buffer
    size_t meshletIndexOffset;//Offset for this meshlet within the global index buffer
    size_t meshletIndexCount;//Index count for this meshlet
};
struct PerSubmeshData
{
    uint32_t meshletCt;
    size_t firstMeshletIndex; //For indexing into global arrays
};

//Objects have like, transformation info, ref to their mesh, ref to their material
//Not sure on ref to material. Really I only have one shader right now
struct TextureMetaData;
struct preMeshletMesh;
struct VkDescriptorImageInfo;
void SetVertexDataFromLoadingMeshVertex(Vertex& input, glm::vec4& outptuPos, gpuvertex& outputVert);

struct AllocatedMeshData
{
    std::span<glm::vec4> vertPositions;
    std::span<gpuvertex> vertData;
    std::span<uint8_t> vertIndices;
};
class AssetManager
{
public:
    size_t materialTextureCount();
    AssetManager();


#pragma region scene

#pragma endregion

#pragma region RendererData

        //vert positions/data 
    struct RendererMeshData
    {
        // Geometry Data
                //TODO JS PERF: Break vertices into separate positions/data like below
                Array<glm::vec4> vertPositions; //glm::vec4
                Array<gpuvertex> vertData; //gpuvertex

        // Objects/Meshlets
        Array<uint8_t> vertIndices;
        Array<MeshletData> meshletInfo;
        Array<positionRadius> boundingSpheres;
        Array<boundingBox> boundingBoxes; 
        Array<PerSubmeshData> perSubmeshData;
    };
    
    uint32_t submeshCount = 0;
    size_t globalVertexCount = 0;
    size_t globalIndexCount = 0;
    size_t globalMeshletcount = 0;
    Array<Material> materials;
    Array<VkDescriptorImageInfo> textures; //What we need to render the textures
    Array<TextureMetaData> texturesMetaData; //Other info about the texture in parallel array
    Array<Array<ID::SubMeshID>>  subMeshGroups;
    RendererMeshData meshData;
    MemoryArena::memoryArena allocator;


    //Returns the index to the object in the vectors
    ID::MaterialID AddMaterial(float roughness, float metallic, glm::vec3 color, textureSetIDs textureindex, uint32_t pipeline);
    uint32_t getIndexCount();
    uint32_t getVertexCount();
    uint32_t getOffsetFromMeshID(int submesh, int meshlet);
    //TODO JS: these are temporary
    ID::TextureID AddTexture(TextureData T);
    textureSetIDs AddTextureSet(TextureData D, TextureData S, TextureData N);
    AllocatedMeshData RequestMeshMemory(uint32_t vertCt, uint32_t indexCt);

    ID::SubMeshID AddMesh(std::span<preMeshletMesh> SubmeshMeshlets);
    ID::SubMeshID AddMesh(ImportMeshData importMesh);
    ID::SubMeshGroupID AddMultiSubmeshMeshMesh(std::span<std::span<preMeshletMesh>> Submeshes);
    ID::SubMeshGroupID AddMultiSubmeshMeshMesh2(std::span<ImportMeshData> Submeshes);
    
    void Update();
    void Cleanup();

#pragma endregion
};
AssetManager* GetGlobalAssetManager();