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

//"Gameplay"/engine scene -- extracted from the renderer scene, needs to get cleaner separation 
struct Scene
{
    struct Objects
    {
        int objectsCount = 0;
        //Parallel arrays per-object
        Array<glm::vec3> translations;
        Array<glm::quat> rotations;
        Array<glm::vec3> scales;
        Array<uint32_t> meshIndices;
        Array<uint32_t> materials;
        Array<uint64_t> transformIDs;
    };

    Objects objects;
    cameraData sceneCamera;


    objectTransforms transforms;
    void Update();


    int AddDirLight(glm::vec3 position, glm::vec3 color, float intensity);
    int AddSpotLight(glm::vec3 position, glm::vec3 dir, glm::vec3 color, float radius, float intensity);
    int AddPointLight(glm::vec3 position, glm::vec3 color, float intensity);

    // arallel arrays per Light
    int lightCount = 0;
    Array<glm::vec4> lightposandradius;
    Array<glm::vec4> lightcolorAndIntensity;
    Array<glm::vec4> lightDir;
    Array<lightType> lightTypes;


    int AddObject(int meshIndexTODO, int materialIndex, glm::vec3 position,
                  glm::quat rotation, glm::vec3 scale = glm::vec3(1), localTransform* parent = nullptr,
                  std::string name = "");
    int objectsCount();

    //TODO JS: This probably belongs in the rendering side
    int getShadowDataIndex(int idx);

private:
    int AddLight(glm::vec3 position, glm::vec3 dir = glm::vec3(-1), glm::vec3 color = glm::vec3(1), float radius = 1,
                 float intensity = 1, lightType type = LIGHT_POINT);
    void lightSort();
    void UpdateCamera(inputData input);
    void UpdateRotations();
};

//No scale for now
void InitializeScene(MemoryArena::memoryArena* arena, Scene* scene);

int shadowCountFromLightType(lightType t);
