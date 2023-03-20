#pragma once
#include <vector>
#include <glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

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
    std::vector<glm::mat4> matrices;
    
    Scene();
    void Update();
    //Returns the index to the object in the vectors
    int AddObject(MeshData* mesh, glm::vec3 position, glm::quat rotation);

};