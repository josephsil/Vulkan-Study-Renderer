#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_GTC_quaternion

#include "SceneObjectData.h"

#include <algorithm>

#include "../Renderer/MeshData.h" // TODO JS: I want to separate the backing data from the scene 
#include "../Renderer/TextureData.h" // TODO JS: I want to separate the backing data from the scene 
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "../General/Memory.h"
const int OBJECT_MAX = 3000; 
const int LIGHT_MAX = 3000; 
const int ASSET_MAX = 300; 

//No scale for now
void InitializeScene(MemoryArena::memoryArena* arena, Scene* scene)
{
    //Parallel arrays per-object
    scene->objects = {};
    scene->objects.objectsCount = 0;
    scene->objects.translations = Array(MemoryArena::AllocSpan<glm::vec3>(arena, OBJECT_MAX));
    scene->objects.rotations = Array(MemoryArena::AllocSpan<glm::quat>(arena, OBJECT_MAX));
    scene->objects.scales = Array(MemoryArena::AllocSpan<glm::vec3>(arena, OBJECT_MAX));
    scene->objects.materials = Array(MemoryArena::AllocSpan<Material>(arena, OBJECT_MAX));
    scene->objects.meshIndices = Array(MemoryArena::AllocSpan<uint32_t>(arena, OBJECT_MAX));
    scene->objects.matrices = Array(MemoryArena::AllocSpan<glm::mat4>(arena, OBJECT_MAX));
    // scene->objects.meshes = Array(MemoryArena::AllocSpan<MeshData*>(arena, ASSET_MAX));
    // scene->objects.meshVertCounts = Array(MemoryArena::AllocSpan<uint32_t>(arena, ASSET_MAX));

    // arallel arrays per Light
    scene->lightshadowMapCount =  Array(MemoryArena::AllocSpan<uint32_t>(arena, LIGHT_MAX));
    scene->lightposandradius = Array(MemoryArena::AllocSpan<glm::vec4>(arena, LIGHT_MAX));
    scene->lightcolorAndIntensity = Array(MemoryArena::AllocSpan<glm::vec4>(arena, LIGHT_MAX));
    scene->lightDir = Array(MemoryArena::AllocSpan<glm::vec4>(arena, LIGHT_MAX));
    scene->lightTypes = Array(MemoryArena::AllocSpan<glm::float32>(arena, LIGHT_MAX));

    //Non parallel arrays //TODO JS: Pack together?
    scene->backing_diffuse_textures = Array(MemoryArena::AllocSpan<TextureData>(arena, ASSET_MAX));
    scene->backing_specular_textures = Array(MemoryArena::AllocSpan<TextureData>(arena, ASSET_MAX));
    scene->backing_normal_textures = Array(MemoryArena::AllocSpan<TextureData>(arena, ASSET_MAX));
    scene->backing_utility_textures = Array(MemoryArena::AllocSpan<TextureData>(arena, ASSET_MAX));

    scene->backing_meshes =  Array(MemoryArena::AllocSpan<MeshData>(arena, ASSET_MAX));
    scene->meshBoundingSphereRad = Array(MemoryArena::AllocSpan<float>(arena, ASSET_MAX));
    
}

void Scene::Update()
{
    glm::mat4 model;

    for (int i = 0; i < objects.objectsCount; i++)
    {
        model = glm::mat4(1.0f);
        glm::mat4 objectLocalRotation = glm::toMat4(objects.rotations[i]);
        model = translate(model, objects.translations[i]); //TODO: These should update at a different rate than camera stuff
        model *= objectLocalRotation; //TODO JS: temporarily turned off rotation to debug shadows
        model = scale(model, objects.scales[i]);
        objects.matrices[i] = model;
    }
}

//So things like, get the index back from this and then index in to these vecs to update them
//At some point in the future I can replace this with a more sophisticated reference system if I need
//Even just returning a pointer is probably plenty, then I can sort the lists, prune stuff, etc.
int Scene::AddObject(MeshData* mesh, int textureidx, float material_roughness, bool material_metallic,
                     glm::vec3 position, glm::quat rotation, glm::vec3 scale)
{
    //TODD JS: version that can add
    // objects.meshes.push_back(mesh);
    objects.materials.push_back(Material{
        .pipelineidx = (uint32_t)(objects.objectsCount % 20 > 10 ? 0 : 1), .backingTextureidx = textureidx, .metallic = material_metallic, .roughness = material_roughness
    });
    objects.translations.push_back(position);
    objects.rotations.push_back(rotation);
    objects.scales.push_back(scale);
    objects.matrices.push_back(glm::mat4(1.0));
    objects.meshIndices.push_back(mesh->id);
    // objects.meshVertCounts.push_back(mesh->vertcount);
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
    for (int i = 0; i < meshCount; i++)
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
    backing_utility_textures.push_back(T);
    return _utilityTextureCount ++;
}

//TODO JS: we should probably CREATE from here at some point?
int Scene::AddMaterial(TextureData D, TextureData S, TextureData N)
{
    backing_diffuse_textures.push_back(D);
    backing_specular_textures.push_back(S);
    backing_normal_textures.push_back(N);
    
    return textureSetCount++;
}

int Scene::AddBackingMesh(MeshData M)
{
    backing_meshes.push_back(M);
    meshBoundingSphereRad.push_back(M.getBoundingSphere());
    return meshCount ++;
}

float Scene::GetBoundingSphere(int idx)
{
    int i = objects.meshIndices[idx];
    return meshBoundingSphereRad[i] * glm::compMax(objects.scales[i]);
}

struct sortData
{
    glm::vec3 pos;
    std::span<glm::vec3> positions;
    int ct;
    int invert;
};

int orderComparator(void * context, void const* elem1, void const* elem2 )
{
    glm::vec3 pos = ((sortData*)context)->pos;
    std::span<glm::vec3> positions = (*(sortData*)context).positions;
    int sign = (*(sortData*)context).invert;
    int idx1 = *((int*)elem1);
    int idx2 = *((int*)elem2);



    return glm::distance2(positions[idx1], pos) -   glm::distance2(positions[idx2], pos);
    
    
}
void Scene::OrderedObjectIndices(glm::vec3 eyePos, std::span<int> indices, bool invert)
{
    assert(indices.size() == objects.translations.ct);
    sortData s = {eyePos, std::span<glm::vec3>(objects.translations.data, objects.translations.ct),  (int)objects.translations.ct, invert ? -1 : 1};
    qsort_s(indices.data(), indices.size(), sizeof(int), orderComparator, &s);
    return;
}
//very dumb/brute force for now
void Scene::lightSort()
{
    int dir_array[LIGHT_MAX];
    int point_array[LIGHT_MAX];
    int spot_array[LIGHT_MAX];
    Array<int> dirlightIndices = std::span<int>(dir_array);
    Array<int> pointlightIndices = std::span<int>(point_array); 
    Array<int> spotlightIndices = std::span<int>(spot_array); 

    for(int i = 0; i < lightCount; i++)
    {
        switch((lightType)lightTypes[i])
        {
        case LIGHT_DIR:
            dirlightIndices.push_back(i);  
            break;
        case LIGHT_SPOT:
            spotlightIndices.push_back(i); 
            break;          
        case LIGHT_POINT:
            pointlightIndices.push_back(i);
            break;
      
    
        }
    }
    std::span<uint32_t> tempShadowCt;
    std::span<glm::vec4> tempPos;
    std::span<glm::vec4> tempCol;
    std::span<glm::vec4> tempDir;
    std::span<glm::float32> tempType;

    //if there's room, use the back half of the existing arrays
    if (lightCount < LIGHT_MAX / 2)
    {
    tempShadowCt = std::span<uint32_t>(lightshadowMapCount.data, LIGHT_MAX).subspan(LIGHT_MAX / 2, LIGHT_MAX / 2);
    tempPos = std::span<glm::vec4>(lightposandradius.data, LIGHT_MAX).subspan(LIGHT_MAX / 2, LIGHT_MAX / 2);
    tempCol =  std::span<glm::vec4>(lightcolorAndIntensity.data, LIGHT_MAX).subspan(LIGHT_MAX / 2, LIGHT_MAX / 2);   
    tempDir =  std::span<glm::vec4>(lightDir.data, LIGHT_MAX).subspan(LIGHT_MAX / 2, LIGHT_MAX / 2);   
    tempType = std::span<glm::float32>(lightTypes.data, LIGHT_MAX).subspan(LIGHT_MAX / 2, LIGHT_MAX / 2);    
    }
    else
    {
        //not implemented
        assert(lightCount < LIGHT_MAX / 2);
        // tempPos = MemoryArena::AllocSpan<glm::vec4>(allocator, lightCount);
        // tempCol = MemoryArena::AllocSpan<glm::vec4>(allocator, lightCount);
        // tempDir = MemoryArena::AllocSpan<glm::vec4>(allocator, lightCount);
        // tempType = MemoryArena::AllocSpan<glm::float32>(allocator, lightCount);
    }

    for(int i =0; i < lightCount; i++)
    {
        tempShadowCt[i] = lightshadowMapCount[i];
        tempPos[i] = lightposandradius[i];
        tempCol[i] = lightcolorAndIntensity[i];
        tempDir[i] = lightDir[i];
        tempType[i] = lightTypes[i];
    }

    int i = 0;
    int j;
    for(j = 0; j < dirlightIndices.ct; j++)
    {
         lightshadowMapCount[i + j] = tempShadowCt[dirlightIndices[j]];
        lightposandradius[i + j ] = tempPos[dirlightIndices[j]];
        lightcolorAndIntensity[i + j ] = tempCol[dirlightIndices[j]];
        lightDir[i + j ] = tempDir[dirlightIndices[j]];
        lightTypes[i + j ] = tempType[dirlightIndices[j]];
    }
    i += j;
    for(j = 0; j < spotlightIndices.ct; j++)
    {
         lightshadowMapCount[i + j] = tempShadowCt[spotlightIndices[j]];
        lightposandradius[i + j ] = tempPos[spotlightIndices[j]];
        lightcolorAndIntensity[i + j ] = tempCol[spotlightIndices[j]];
        lightDir[i + j ] = tempDir[spotlightIndices[j]];
        lightTypes[i + j ] = tempType[spotlightIndices[j]];
    }
    i += j;
    for(j = 0; j < pointlightIndices.ct; j++)
    {
         lightshadowMapCount[i + j] = tempShadowCt[pointlightIndices[j]];
        lightposandradius[i + j ] = tempPos[pointlightIndices[j]];
        lightcolorAndIntensity[i + j ] = tempCol[pointlightIndices[j]];
        lightDir[i + j ] = tempDir[pointlightIndices[j]];
        lightTypes[i + j ] = tempType[pointlightIndices[j]];
    }

}

int Scene::AddLight(glm::vec3 position, glm::vec3 dir, glm::vec3 color, float radius, float intensity, lightType type)
{
    lightshadowMapCount.push_back( type == LIGHT_POINT ? 6 : type == LIGHT_DIR ? CASCADE_CT : 1); //TODO JS
    lightposandradius.push_back(glm::vec4(position.x, position.y, position.z, radius));
    lightcolorAndIntensity.push_back(glm::vec4(color.x, color.y, color.z, intensity));
    lightDir.push_back(glm::vec4(dir, -1.0));
    lightTypes.push_back(type);
    lightCount ++;

    lightSort();
    return -1; //NOT IMPLEMENTED
}
int Scene::AddDirLight(glm::vec3 position, glm::vec3 color,float intensity)
{
    return this->AddLight(position, glm::vec3(-1), color, -1, intensity, lightType::LIGHT_DIR);
}
int Scene::AddSpotLight(glm::vec3 position, glm::vec3 dir, glm::vec3 color, float radius, float intensity)
{
    return this->AddLight(position, dir, color, radius, intensity, lightType::LIGHT_SPOT);
}

int Scene::AddPointLight(glm::vec3 position, glm::vec3 color,  float intensity)
{
    return this->AddLight(position, glm::vec3(-1), color, -1, intensity, lightType::LIGHT_POINT);
}


int Scene::getShadowDataIndex(int idx)
{
    int output = 0;
    for(int i = 0; i < idx; i++)
    {
        output +=  lightshadowMapCount[i];
    }
    return output;
}

int Scene::shadowCasterCount()
{
    return min(MAX_SHADOWCASTERS, this->lightCount);
}

void Scene::Cleanup()
{
    for (int i = 0; i < meshCount; i++)
    {
        backing_meshes[i].cleanup();
    }
    for (int i = 0; i <  materialCount(); i++)
    {
        backing_diffuse_textures[i].cleanup();
        backing_specular_textures[i].cleanup();
        backing_normal_textures[i].cleanup();
    }
}
