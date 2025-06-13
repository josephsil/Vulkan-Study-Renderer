#pragma once

#include "General/Array.h"

#include <string>

#include "Transforms.h"
#include "Renderer/AssetManagerTypes.h"
#include "Renderer/rendererGlobals.h"
#include "Renderer/RendererSharedTypes.h"

struct localTransform;

namespace MemoryArena
{
    struct Allocator;
}

//Used for allocations 
struct SceneSizeData 
{
    size_t objectCount = 0;
    size_t totalDrawCt = 0;
    size_t lightCount = 0;
    std::span<LightType> lightTypes;
};

// Scene contains everything to do with accessing and updating objects
// Expected to update every frame. Updated from 'gameplay logic' (althogh there isn't much)
// Has light data, object data (which is mostly in the form of indices to the assetmanager,)
// And camera data.
// Currently also contains Transform logic (which is rough/proof of concept)
struct Scene
{
    struct Objects
    {
        size_t objectsCount = 0;
        size_t subMeshesCount = 0;
        //Parallel arrays per-object
        Array<glm::vec3> translations;
        Array<glm::quat> rotations;
        Array<glm::vec3> scales;
        Array<std::span<uint32_t>> subMeshes; 
        Array<std::span<uint32_t>> subMeshMaterials; 
        Array<size_t> transformIDs;
    };

    Array<uint32_t>  allMaterials; //Backs the Array of spans in Object
    Array<uint32_t> allSubmeshes; //Backs the Array of spans in Objects
    // parallel arrays per Light
    size_t lightCount;
    Array<glm::vec4> lightposandradius;
    Array<glm::vec4> lightcolorAndIntensity;
    Array<glm::vec4> lightDir;
    Array<LightType> lightTypes;
    
    Objects objects;
    cameraData sceneCamera;


    objectTransforms transforms;
	MemoryArena::Allocator scratchMemory; //Freed every frame

	size_t GetObjectsCount();
    size_t GetMeshesCount();
	static size_t getShadowDataIndex(size_t idx, std::span<LightType> lightTypes);
    
	void Update();


    int AddDirLight(glm::vec3 position, glm::vec3 color, float intensity);
    int AddSpotLight(glm::vec3 position, glm::vec3 dir, glm::vec3 color, float radius, float intensity);
    int AddPointLight(glm::vec3 position, glm::vec3 color, float intensity);
	size_t AddObject(std::span<ID::SubMeshID> submeshIndices, std::span<ID::MaterialID> materialIndices,
                     glm::vec3 position, glm::quat rotation, glm::vec3 scale = glm::vec3(1), localTransform* parent = nullptr,
                     std::string name = "");

   

   

private:
    int AddLight(glm::vec3 position, glm::vec3 dir = glm::vec3(-1), glm::vec3 color = glm::vec3(1), float radius = 1,
                 float intensity = 1, LightType type = LIGHT_POINT);
    void lightSort();
    void UpdateCamera(inputData input);
    void UpdateRotations();
};


void InitializeScene(MemoryArena::Allocator* arena, Scene* scene);
size_t shadowCountFromLightType(LightType t);
