#pragma once
#include "glm/glm.hpp"
#include <General/Array.h>
#include <Renderer/rendererGlobals.h>

#include "Renderer/TextureData.h"
//
struct positionRadius;

namespace MemoryArena
{
    struct memoryArena;
}

struct textureSetIDs
{
    uint32_t diffuseIndex;
    uint32_t specIndex;
    uint32_t normIndex;
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
    int materialTextureCount();


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
    int AddMaterial(float roughness, float metallic, glm::vec3 color, textureSetIDs textureindex, uint32_t pipeline);
    uint32_t getIndexCount();
    uint32_t getVertexCount();
    uint32_t getOffsetFromMeshID(int id);
    positionRadius GetBoundingSphere(int idx); //TODO JS: Change to use mesh idx
    //TODO JS: these are temporary
    int AddTexture(TextureData T);
    textureSetIDs AddTextureSet(TextureData D, TextureData S, TextureData N);
    int AddBackingMesh(MeshData M);

    void Update();
    void Cleanup();

#pragma endregion

private:

};
