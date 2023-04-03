#pragma once
#include <vector>
#include <glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include "Material.h"

//Objects have like, transformation info, ref to their mesh, ref to their material
//Not sure on ref to material. Really I only have one shader right now
struct MeshData;
class Scene
{
    //Should add a like gameobject-y thing that has a mesh and a material
    private:
    int ct = 0;
    
    //No scale for now
    public:
    //Parallel arrays per-object
    std::vector<glm::vec3> translations;
    std::vector<glm::quat> rotations;
    std::vector<MeshData*> meshes;
    std::vector<Material> materials;
    std::vector<uint32_t> meshOffsets;
    std::vector<uint32_t> meshVertCounts;
    std::vector<glm::mat4> matrices;

    // arallel arrays per Light
    std::vector<glm::vec4> lightposandradius;
    std::vector<glm::vec4> lightcolorAndIntensity;
    int lightCount = 0;

    //Non parallel arrays
    std::vector<TextureData> backing_diffuse_textures;
    std::vector<TextureData> backing_specular_textures;
    std::vector<TextureData> backing_normal_textures;
    std::vector<MeshData> backing_meshes;
    
    Scene();
    void Update();
    void Sort();
    //Returns the index to the object in the vectors
    int AddObject(MeshData* mesh, int textureidx, glm::vec3 position, glm::quat rotation);
    uint32_t getVertexCount();
    uint32_t getOffsetFromMeshID(int id);

    //TODO JS: these are temporary 
    int AddMaterial(TextureData D, TextureData S, TextureData N);
    int AddBackingMesh(MeshData M);
    int AddLight(glm::vec3 position, glm::vec3 color, float radius=1, float intensity=1);

    void Cleanup();

};