#pragma once
#include <span>
#include <vector>
#include <glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include "Material.h"

namespace MemoryArena
{
    struct memoryArena;
}

//Objects have like, transformation info, ref to their mesh, ref to their material
//Not sure on ref to material. Really I only have one shader right now
struct TextureData;
struct MeshData;
struct VkDescriptorImageInfo;

class Scene
{
    //Should add a like gameobject-y thing that has a mesh and a material
private:
    //No scale for now
public:
    int utilityTextureCount();
    int materialCount();
    int materialTextureCount();

    struct Objects
    {
        int objectsCount = 0;
        //Parallel arrays per-object
        std::span<glm::vec3> translations;
        std::span<glm::quat> rotations;
        std::span<MeshData*> meshes; //todo js: are these redundant?
        std::span<uint32_t> meshOffsets; //todo js: are these redundant?
        std::span<Material> materials;
        std::span<uint32_t> meshVertCounts;
        std::span<glm::mat4> matrices;
    };

    Objects objects;

    // arallel arrays per Light
    int lightCount = 0;
    std::span<glm::vec4> lightposandradius;
    std::span<glm::vec4> lightcolorAndIntensity;

    //Non parallel arrays //TODO JS: Pack together?
    int textureSetCount = 0;
    std::span<TextureData> backing_diffuse_textures;
    std::span<TextureData> backing_specular_textures;
    std::span<TextureData> backing_normal_textures;

    int _utilityTextureCount = 0;
    std::span<TextureData> backing_utility_textures;

    int meshCount = 0;
    std::span<MeshData> backing_meshes;


    Scene(MemoryArena::memoryArena* memoryArena);
    void Update();
    //Returns the index to the object in the vectors
    int AddObject(MeshData* mesh, int textureidx, float material_roughness, bool material_metallic, glm::vec3 position,
                  glm::quat rotation);
    uint32_t getVertexCount();
    int objectsCount();
    uint32_t getOffsetFromMeshID(int id);

    //TODO JS: these are temporary
    int AddUtilityTexture(TextureData T);
    int AddMaterial(TextureData D, TextureData S, TextureData N);
    int AddBackingMesh(MeshData M);
    int AddLight(glm::vec3 position, glm::vec3 color, float radius = 1, float intensity = 1);

    void Cleanup();

    std::pair<std::vector<VkDescriptorImageInfo>, std::vector<VkDescriptorImageInfo>> getBindlessTextureInfos();
};
