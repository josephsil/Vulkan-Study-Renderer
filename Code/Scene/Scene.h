#pragma once

#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "General/Array.h"

#include <glm/glm.hpp>
#include <string>

#include "Transforms.h"
#include "Renderer/rendererGlobals.h"

struct localTransform;

namespace MemoryArena
{
    struct memoryArena;
}

struct sceneCountData
{
    size_t objectCount = 0;
    size_t lightCount = 0;
    std::span<lightType> lightTypes;
};

//"Gameplay"/engine scene -- extracted from the renderer scene, needs to get cleaner separation 
struct Scene
{
    struct Objects
    {
        size_t objectsCount = 0;
        //Parallel arrays per-object
        Array<glm::vec3> translations;
        Array<glm::quat> rotations;
        Array<glm::vec3> scales;
        Array<uint32_t> meshIndices; //for renderer 
        Array<uint32_t> materials; //for renderer 
        Array<size_t> transformIDs;
    };

    // arallel arrays per Light
    size_t lightCount;
    Array<glm::vec4> lightposandradius;
    Array<glm::vec4> lightcolorAndIntensity;
    Array<glm::vec4> lightDir;
    Array<lightType> lightTypes;
    
    Objects objects;
    cameraData sceneCamera;


    objectTransforms transforms;
    void Update();


    int AddDirLight(glm::vec3 position, glm::vec3 color, float intensity);
    int AddSpotLight(glm::vec3 position, glm::vec3 dir, glm::vec3 color, float radius, float intensity);
    int AddPointLight(glm::vec3 position, glm::vec3 color, float intensity);




    size_t AddObject(unsigned long long meshIndexTODO, unsigned long long materialIndex, glm::vec3 position,
                     glm::quat rotation, glm::vec3 scale = glm::vec3(1), localTransform* parent = nullptr,
                     std::string name = "");
    size_t objectsCount();

    //TODO JS: This probably belongs in the rendering side
    static size_t getShadowDataIndex(size_t idx, std::span<lightType> lightTypes);

private:
    int AddLight(glm::vec3 position, glm::vec3 dir = glm::vec3(-1), glm::vec3 color = glm::vec3(1), float radius = 1,
                 float intensity = 1, lightType type = LIGHT_POINT);
    void lightSort();
    void UpdateCamera(inputData input);
    void UpdateRotations();
};

//No scale for now
void InitializeScene(MemoryArena::memoryArena* arena, Scene* scene);

size_t shadowCountFromLightType(lightType t);
