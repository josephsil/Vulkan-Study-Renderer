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

#include "Memory.h"
#include "Vulkan_Includes.h" 
#include "vulkan-utilities.h"
const int OBJECT_MAX = 3000; //TODO JS: grow
const int LIGHT_MAX = 3000; //TODO JS: grow
const int ASSET_MAX = 300; //TODO JS: grow

//No scale for now
Scene::Scene(MemoryArena::memoryArena* arena)
{
    //Parallel arrays per-object
    objects = {};
    objects.objectsCount = 0;
    objects.translations = MemoryArena::AllocSpan<glm::vec3>(arena, OBJECT_MAX);
    objects.rotations = MemoryArena::AllocSpan<glm::quat>(arena, OBJECT_MAX);
    objects.materials = MemoryArena::AllocSpan<Material>(arena, OBJECT_MAX);
    objects.meshOffsets = MemoryArena::AllocSpan<uint32_t>(arena, OBJECT_MAX);
    objects.matrices = MemoryArena::AllocSpan<glm::mat4>(arena, OBJECT_MAX);
    objects.meshes = MemoryArena::AllocSpan<MeshData*>(arena, OBJECT_MAX);
    objects.meshVertCounts = MemoryArena::AllocSpan<uint32_t>(arena, OBJECT_MAX);

    // arallel arrays per Light
    lightposandradius = MemoryArena::AllocSpan<glm::vec4>(arena, LIGHT_MAX);
    lightcolorAndIntensity = MemoryArena::AllocSpan<glm::vec4>(arena, LIGHT_MAX);

    //Non parallel arrays //TODO JS: Pack together?
    backing_diffuse_textures = MemoryArena::AllocSpan<TextureData>(arena, 300);
    backing_specular_textures = MemoryArena::AllocSpan<TextureData>(arena, 300);
    backing_normal_textures = MemoryArena::AllocSpan<TextureData>(arena, 300);
    backing_utility_textures = MemoryArena::AllocSpan<TextureData>(arena, 300);

    backing_meshes = MemoryArena::AllocSpan<MeshData>(arena, 300);
    
}

void Scene::Update()
{
    glm::mat4 model;

    for (int i = 0; i < objects.objectsCount; i++)
    {
        model = glm::mat4(1.0f);
        glm::mat4 objectLocalRotation = toMat4(objects.rotations[i]);
        model = translate(model, objects.translations[i]); //TODO: These should update at a different rate than camera stuff
        model *= objectLocalRotation;
        //TODO: Don't fake apply scale here
        model = scale(model, glm::vec3(0.5));
        objects.matrices[i] = model;
    }
}

//So things like, get the index back from this and then index in to these vecs to update them
//At some point in the future I can replace this with a more sophisticated reference system if I need
//Even just returning a pointer is probably plenty, then I can sort the lists, prune stuff, etc.
int Scene::AddObject(MeshData* mesh, int textureidx, float material_roughness, bool material_metallic,
                     glm::vec3 position, glm::quat rotation)
{
    //TODD JS: version that can add 
    objects.meshes[objects.objectsCount] = (mesh);
    objects.materials[objects.objectsCount] = (Material{
        .pipelineidx = (uint32_t)(objects.objectsCount % 20 > 10 ? 0 : 1), .backingTextureidx = textureidx, .metallic = material_metallic, .roughness = material_roughness
    });
    objects.translations[objects.objectsCount] = (position);
    objects.rotations[objects.objectsCount] = (rotation);
    objects.matrices[objects.objectsCount] = (glm::mat4(1.0));
    objects.meshOffsets[objects.objectsCount] = (getOffsetFromMeshID(mesh->id));
    objects.meshVertCounts[objects.objectsCount] = (mesh->vertcount);
    return objects.objectsCount++;
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


int Scene::objectsCount()
{
    return objects.objectsCount;
}

int Scene::materialCount()
{
    return textureSetCount;
}

int Scene::utilityTextureCount()
{
    return _utilityTextureCount;
}

int Scene::materialTextureCount()
{
    return  textureSetCount * 3;
}

int Scene::AddUtilityTexture(TextureData T)
{
    backing_utility_textures[_utilityTextureCount] = (T);
    return _utilityTextureCount ++;
}

//TODO JS: we should probably CREATE from here at some point?
int Scene::AddMaterial(TextureData D, TextureData S, TextureData N)
{
    backing_diffuse_textures[textureSetCount] = (D);
    backing_specular_textures[textureSetCount] = (S);
    backing_normal_textures[textureSetCount] = (N);
    
    return textureSetCount++;
}

int Scene::AddBackingMesh(MeshData M)
{
    backing_meshes[meshCount] = (M);
    return meshCount ++;
}

int Scene::AddLight(glm::vec3 position, glm::vec3 color, float radius, float intensity)
{
    lightposandradius[lightCount] = (glm::vec4(position.x, position.y, position.z, radius));
    lightcolorAndIntensity[lightCount] = (glm::vec4(color.x, color.y, color.z, intensity));
    lightCount ++;
    return -1; //NOT IMPLEMENTED
}

void Scene::Cleanup()
{
    for (int i = 0; i < backing_meshes.size(); i++)
    {
        backing_meshes[i].cleanup();
    }
    for (int i = 0; i < backing_diffuse_textures.size(); i++)
    {
        backing_diffuse_textures[i].cleanup();
    }
}

std::pair<VkDescriptorImageInfo, VkDescriptorImageInfo> ImageInfoFromImageData(
    TextureData texture)
{
    return std::make_pair(
        VkDescriptorImageInfo{
            .imageView = texture.textureImageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
        VkDescriptorImageInfo{
            .sampler = texture.textureSampler, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        }
    );
}


std::pair<std::vector<VkDescriptorImageInfo>, std::vector<VkDescriptorImageInfo>> Scene::getBindlessTextureInfos()
{
    //TODO JS: Don't do this every frame
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkDescriptorImageInfo> samplerInfos;
    //Material textures
    for (int texture_i = 0; texture_i < materialCount(); texture_i++)
    {
        auto [imageInfo, samplerInfo] = ImageInfoFromImageData(
            backing_diffuse_textures[texture_i]);
        imageInfos.push_back(imageInfo);
        samplerInfos.push_back(samplerInfo);

        auto [imageInfo2, samplerInfo2] = ImageInfoFromImageData(
            backing_specular_textures[texture_i]);
        imageInfos.push_back(imageInfo2);
        samplerInfos.push_back(samplerInfo2);

        auto [imageInfo3, samplerInfo3] = ImageInfoFromImageData(
            backing_normal_textures[texture_i]);
        imageInfos.push_back(imageInfo3);
        samplerInfos.push_back(samplerInfo3);
    }

    for (int texture_i = 0; texture_i < utilityTextureCount(); texture_i++)
    {
        auto [imageInfo, samplerInfo] = ImageInfoFromImageData(
            backing_utility_textures[texture_i]);
        imageInfos.push_back(imageInfo);
        samplerInfos.push_back(samplerInfo);
    }

    return make_pair(imageInfos, samplerInfos);
}
