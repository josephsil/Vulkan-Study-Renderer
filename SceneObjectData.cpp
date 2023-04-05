#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_GTC_quaternion

#include "SceneObjectData.h"

#include <algorithm>

#include "MeshData.h"
#include "TextureData.h"
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
            glm::mat4 objectLocalRotation = glm::toMat4(rotations[i]);
            model = glm::translate(model, translations[i]); //TODO: These should update at a different rate than camera stuff
            // model *= objectLocalRotation;
            //TODO: Don't fake apply scale here
            model = glm::scale(model, glm::vec3(0.5));
            matrices[i] = model;
        }
      
    }

    //So things like, get the index back from this and then index in to these vecs to update them
    //At some point in the future I can replace this with a more sophisticated reference system if I need
    //Even just returning a pointer is probably plenty, then I can sort the lists, prune stuff, etc.
    int Scene::AddObject(MeshData* mesh, int textureidx, glm::vec3 position, glm::quat rotation)
    {
        //TODD JS: version that can add 
        meshes.push_back(mesh);
        materials.push_back(Material{.backingTextureidx = textureidx});
        translations.push_back(position);
        rotations.push_back(rotation);
        matrices.push_back(glm::mat4(1.0));
        meshOffsets.push_back(getOffsetFromMeshID(mesh->id));
        meshVertCounts.push_back(mesh->vertcount);
        return ct++;
    }


uint32_t Scene::getOffsetFromMeshID(int id)
    {
        int indexcount = 0;
        for (int i = 0; i < id; i++)
        {
            indexcount += backing_meshes[i].indices.size();
        }
        return indexcount;
    }

uint32_t Scene::getVertexCount()
{
        int indexcount = 0;
        for (int i = 0; i < backing_meshes.size(); i++)
        {
            indexcount += backing_meshes[i].indices.size();
        }
        return indexcount;
}


//TODO JS: we should probably CREATE from here at some point?
    int Scene::AddMaterial(TextureData D, TextureData S, TextureData N)
    {
        backing_diffuse_textures.push_back(D);
        backing_specular_textures.push_back(S);
        backing_normal_textures.push_back(N);
        return backing_diffuse_textures.size() -1;
    }

    int Scene::AddBackingMesh(MeshData M)
    {
        backing_meshes.push_back(M);
        return backing_meshes.size() -1;
    }

int Scene::AddLight(glm::vec3 position, glm::vec3 color, float radius, float intensity)
    {

        lightposandradius.push_back(glm::vec4(position.x,position.y,position.z,radius));
        lightcolorAndIntensity.push_back(glm::vec4(color.x,color.y,color.z,intensity));
        lightCount ++;
        return -1; //NOT IMPLEMENTED
    }

void Scene::Cleanup()
{
        for(int i = 0; i < backing_meshes.size(); i++)
        {
            backing_meshes[i].cleanup();
        }
        for(int i = 0; i < backing_diffuse_textures.size(); i++)
        {
            backing_diffuse_textures[i].cleanup();
        }
        
}
void Scene::Sort()
    {
        std::sort(meshes.begin(), meshes.end(), [](MeshData* a, MeshData* b) {
       return a->id < b->id;
   });
    }

    