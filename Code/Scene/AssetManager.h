#pragma once
#include <General/Array.h>
#include <Renderer/rendererGlobals.h>

#include "Renderer/RendererSharedTypes.h"
#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/AssetManagerTypes.h"
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

//Objects have like, transformation info, ref to their mesh, ref to their material
//Not sure on ref to material. Really I only have one shader right now
struct TextureMetaData;
struct MeshData;
struct VkDescriptorImageInfo;
class AssetManager;
void static_AllocateAssetMemory(MemoryArena::memoryArena* arena, AssetManager* scene);
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
    Array<MeshData> backing_meshes;
    Array<positionRadius> meshBoundingSphereRad;


    //Returns the index to the object in the vectors
    ID::MaterialID AddMaterial(float roughness, float metallic, glm::vec3 color, textureSetIDs textureindex, uint32_t pipeline);
    uint32_t getIndexCount();
    uint32_t getVertexCount();
    uint32_t getOffsetFromMeshID(int id);
    positionRadius GetBoundingSphere(int idx); //TODO JS: Change to use mesh idx
    //TODO JS: these are temporary
    ID::TextureID AddTexture(TextureData T);
    textureSetIDs AddTextureSet(TextureData D, TextureData S, TextureData N);
    ID::MeshID AddBackingMesh(MeshData M);

    void Update();
    void Cleanup();

#pragma endregion
};
