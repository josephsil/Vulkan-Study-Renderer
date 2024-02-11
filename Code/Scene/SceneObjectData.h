#pragma once

#include <span>

#include <vector>
#include <glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include <General/Array.h>
#include <Renderer/rendererGlobals.h>

#include "Transforms.h"
//
struct positionRadius;

namespace MemoryArena
{
    struct memoryArena;
}

//Objects have like, transformation info, ref to their mesh, ref to their material
//Not sure on ref to material. Really I only have one shader right now
struct TextureData;
struct MeshData;
struct VkDescriptorImageInfo;
class Scene;

const int OBJECT_MAX = 3000; 
const int LIGHT_MAX = 3000; 
const int ASSET_MAX = 300;

void InitializeScene(MemoryArena::memoryArena* arena, Scene* scene);
class Scene
{
    //Should add a like gameobject-y thing that has a mesh and a material
private:
  
    //No scale for now
public:
    int utilityTextureCount();
    int materialCount();
    int materialTextureCount();

    void OrderedObjectIndices(::MemoryArena::memoryArena* allocator, glm::vec3 eyePos, std::span<int> indices, bool invert);

   
    struct Objects
    {
        int objectsCount = 0;
        //Parallel arrays per-object
        Array<glm::vec3> translations;
        Array<glm::quat> rotations;
        Array<glm::vec3> scales;
        Array<uint32_t> meshIndices;
        Array<Material> materials;
        Array<uint64_t> transformIDs;

      
    };

    Objects objects;
    objectTransforms transforms;

    // arallel arrays per Light
    int lightCount = 0;


    Array<uint32_t> lightshadowMapCount;
    Array<glm::vec4> lightposandradius;
    Array<glm::vec4> lightcolorAndIntensity;
    Array<glm::vec4> lightDir;
    Array<glm::float32> lightTypes;
    

    //Non parallel arrays //TODO JS: Pack together?
    int textureSetCount = 0;
    Array<TextureData> backing_diffuse_textures;
    Array<TextureData> backing_specular_textures;
    Array<TextureData> backing_normal_textures;

    int _utilityTextureCount = 0;
    Array<TextureData> backing_utility_textures;

    int meshCount = 0;
    Array<MeshData> backing_meshes;
    Array<positionRadius> meshBoundingSphereRad;


    Scene(MemoryArena::memoryArena* memoryArena);
    void Update();
    //Returns the index to the object in the vectors
    int AddObject(MeshData* mesh, int textureidx, float material_roughness, bool material_metallic, glm::vec3 position,
                  glm::quat rotation, glm::vec3 scale = glm::vec3(1), localTransform* parent = nullptr);
    uint32_t getVertexCount();
    int objectsCount();
    uint32_t getOffsetFromMeshID(int id);

    //TODO JS: these are temporary
    int AddUtilityTexture(TextureData T);
    int AddMaterial(TextureData D, TextureData S, TextureData N);
    int AddBackingMesh(MeshData M);
    positionRadius GetBoundingSphere(int idx);

    int AddDirLight(glm::vec3 position, glm::vec3 color,float intensity);
    int AddSpotLight(glm::vec3 position, glm::vec3 dir, glm::vec3 color, float radius, float intensity);
    int AddPointLight(glm::vec3 position, glm::vec3 color,  float intensity);
    int getShadowDataIndex(int i);
    int shadowCasterCount();
    uint32_t shadowmapCount;

    void Cleanup();

    std::pair<std::vector<VkDescriptorImageInfo>, std::vector<VkDescriptorImageInfo>> getBindlessTextureInfos(MemoryArena::memoryArena* allocator);
private:
    int AddLight(glm::vec3 position, glm::vec3 dir = glm::vec3(-1), glm::vec3 color = glm::vec3(1), float radius = 1, float intensity = 1, lightType type = lightType::LIGHT_POINT);
    void lightSort();
};
