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
    std::vector<glm::vec3> translations;
    std::vector<glm::quat> rotations;
    std::vector<MeshData*> meshes;
    std::vector<Material> materials;
    std::vector<MeshData> backing_meshes;
    std::vector<TextureData> backing_textures;
    std::vector<glm::mat4> matrices;
    
    Scene();
    void Update();
    void Sort();
    //Returns the index to the object in the vectors
    int AddObject(MeshData* mesh, TextureData* texture, glm::vec3 position, glm::quat rotation);
    uint32_t getVertexCount();
    uint32_t getOffsetFromMeshID(int id);

    //TODO JS: these are temporary 
    int AddBackingTexture(TextureData T);
    int AddBackingMesh(MeshData M);

    void Cleanup();

};