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
    struct memoryArena;
}

struct sceneCountData
{
    size_t objectCount = 0;
    size_t totalDrawCt = 0;
    size_t lightCount = 0;
    std::span<LightType> lightTypes;
};

//"Gameplay"/engine scene -- extracted from the renderer scene, needs to get cleaner separation 
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
        Array<std::span<uint32_t>> subMeshes; //for renderer
        Array<std::span<uint32_t>> subMeshMaterials; //todo js meshlet perf: there are one of these per meshlet currently -- need to fix
        Array<size_t> transformIDs;
    };

    Array<uint32_t>  allMaterials; //Backs the span of spans
    Array<uint32_t> allSubmeshes; //Backs the span of spans
    // arallel arrays per Light
    size_t lightCount;
    Array<glm::vec4> lightposandradius;
    Array<glm::vec4> lightcolorAndIntensity;
    Array<glm::vec4> lightDir;
    Array<LightType> lightTypes;
    
    Objects objects;
    cameraData sceneCamera;


    objectTransforms transforms;
	MemoryArena::memoryArena scratchMemory; //Freed every frame
    void Update();


    int AddDirLight(glm::vec3 position, glm::vec3 color, float intensity);
    int AddSpotLight(glm::vec3 position, glm::vec3 dir, glm::vec3 color, float radius, float intensity);
    int AddPointLight(glm::vec3 position, glm::vec3 color, float intensity);


    

    
    size_t AddObject(std::span<ID::SubMeshID> submeshIndices, std::span<ID::MaterialID> materialIndices,
                     glm::vec3 position, glm::quat rotation, glm::vec3 scale = glm::vec3(1), localTransform* parent = nullptr,
                     std::string name = "");

    size_t ObjectsCount();
    size_t MeshesCount();
    size_t GetTotalSubmeshesForObjects(std::span<uint32_t> objectIndices);

    //TODO JS: This probably belongs in the rendering side
    static size_t getShadowDataIndex(size_t idx, std::span<LightType> lightTypes);

private:
    int AddLight(glm::vec3 position, glm::vec3 dir = glm::vec3(-1), glm::vec3 color = glm::vec3(1), float radius = 1,
                 float intensity = 1, LightType type = LIGHT_POINT);
    void lightSort();
    void UpdateCamera(inputData input);
    void UpdateRotations();
};


//No scale for now
void InitializeScene(MemoryArena::memoryArena* arena, Scene* scene);

size_t shadowCountFromLightType(LightType t);
