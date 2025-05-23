#pragma once
#include <General/Array.h>
#include <Renderer/rendererGlobals.h>

#include "General/LinearDictionary.h"
#include "Renderer/RendererSharedTypes.h"
#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/AssetManagerTypes.h"
#include "Renderer/MainRenderer/rendererStructs.h"
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

//todo js to remove
struct MeshletData
{
    size_t meshletVertexOffset; //Offset for this meshlet's verts within the global vertex buffer
    size_t meshletIndexOffset;//Offset for this meshlet within the global index buffer
    size_t meshletIndexCount;//Index count for this meshlet
    // size_t first_meshlet_mesh_index;//ID for the object
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
class AssetManager
{
public:
    size_t materialTextureCount();
    AssetManager();


#pragma region scene

#pragma endregion

#pragma region RendererData



    //TODO JS: Next steps:
    //Phased plan for meshlet -> mesh import revamp:
    //Phase 1: separate "submesh" and "meshlet" concept
        //Submesh indices point to parallel arrays of material and submesh data
        //In phase 1, submesh data is span of spans of meshdata
        //Plumb this through relevant rendering/buffer upload code. Maybe separated as functions to be easy to replace
        //perdrawdata still needs to get set per submesh
    //Phase2: 1 vertex buffer per submesh
        //Basically same as phase 2, but don't duplicate verts onto submeshes. Update vertex buffer binding code and various plumbing appropriately
        //Also need to update meshlet generation -- according to docs meshlet vertices are the correct thing to use as indices for the original buffer.
    //Phase3, 4: Contiguous memory for meshes
        //1. Move all vertex and index allocations onto RendererMeshData struct, update gpu upload code to loop over those buffers directly
        //2. Move away from spans of spans to a size and stride approach for meshlets -- should be able to compactly represent meshlets as "start + count" for indices, and a reference to the vertex buffer
        //3. Update assetmanager representation of vertices to match gpu representation in layout so I can just directly copy them
        
        //vert positions/data 
    struct RendererMeshData
    {
        // Array<Vertex> vertices;
                //TODO JS PERF: Break vertices into separate positions/data like below
                Array<glm::vec4> vertPositions; //glm::vec4
                Array<gpuvertex> vertData; //gpuvertex
        Array<uint8_t> vertIndices; //uint32_t
        Array<MeshletData> meshletInfo;
        Array<positionRadius> boundingInfo;
        Array<PerSubmeshData> perSubmeshData;
    };
    
    uint32_t submeshCount = 0;
    size_t vertexCount = 0;
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
    ID::SubMeshID AddMesh(std::span<preMeshletMesh> SubmeshMeshlets);
    ID::SubMeshID UploadMesh(ImportMeshData importMesh);
    ID::SubMeshGroupID AddMultiSubmeshMeshMesh(std::span<std::span<preMeshletMesh>> Submeshes);
    ID::SubMeshGroupID AddMultiSubmeshMeshMesh2(std::span<ImportMeshData> Submeshes);
    
    void Update();
    void Cleanup();

#pragma endregion
};
AssetManager* GetGlobalAssetManager();