#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_GTC_quaternion

#include "SceneObjectData.h"
#include "MeshData.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>


    //No scale for now
    Scene::Scene()
    {
        
    }
    void Scene::Update()
    {
        // auto objectRotation = time * glm::radians(90.0f); //TODO JS: pass in from renderr update
        glm::mat4 model;
        
        for (int i = 0; i < translations.size(); i++)
        {
            model = glm::mat4(1.0f);
            
            model = glm::toMat4(rotations[i]);
            model = glm::translate(model, translations[i]); //TODO: These should update at a different rate than camera stuff
            matrices[i] = model;
        }
      
    }

    //So things like, get the index back from this and then index in to these vecs to update them
    //At some point in the future I can replace this with a more sophisticated reference system if I need
    //Even just returning a pointer is probably plenty, then I can sort the lists, prune stuff, etc.
    int Scene::AddObject(MeshData* mesh, glm::vec3 position, glm::quat rotation)
    {
        meshes.push_back(mesh);
        translations.push_back(position);
        rotations.push_back(rotation);
        matrices.push_back(glm::mat4(1.0));
        return ct++;
    }
    