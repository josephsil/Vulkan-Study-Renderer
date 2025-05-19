#include "Scene.h"

#include "engineGlobals.h"
#include "glm_misc.h"
#include "Transforms.h"
#include "General/Array.h"
#include "General/Array.h"
#include "General/Array.h"
#include "General/Array.h"
#include "General/Array.h"
#include "General/Array.h"
#include "General/Array.h"
#include "General/InputHandling.h"
#include "General/MemoryArena.h"
#include "Renderer/AssetManagerTypes.h"

void InitializeScene(MemoryArena::memoryArena* arena, Scene* scene)
{
    //Objects
    scene->objects = {};
    scene->objects.objectsCount = 0;
    scene->objects.translations = Array(MemoryArena::AllocSpan<glm::vec3>(arena, OBJECT_MAX));
    scene->objects.rotations = Array(MemoryArena::AllocSpan<glm::quat>(arena, OBJECT_MAX));
    scene->objects.scales = Array(MemoryArena::AllocSpan<glm::vec3>(arena, OBJECT_MAX));
    scene->objects.subMeshMaterials = Array(MemoryArena::AllocSpan<std::span<uint32_t>>(arena, OBJECT_MAX));
    scene->allMaterials = Array(MemoryArena::AllocSpanDefaultInitialize<uint32_t>(arena, OBJECT_MAX * 100));
    scene->allSubmeshes = Array(MemoryArena::AllocSpanDefaultInitialize<uint32_t>(arena, OBJECT_MAX * 100));
    scene->objects.subMeshes = Array(MemoryArena::AllocSpan<std::span<uint32_t>>(arena, OBJECT_MAX));
    scene->objects.transformIDs = Array(MemoryArena::AllocSpan<size_t>(arena, OBJECT_MAX));

    //Lights 
    scene->lightposandradius = Array(MemoryArena::AllocSpan<glm::vec4>(arena, LIGHT_MAX));
    scene->lightcolorAndIntensity = Array(MemoryArena::AllocSpan<glm::vec4>(arena, LIGHT_MAX));
    scene->lightDir = Array(MemoryArena::AllocSpan<glm::vec4>(arena, LIGHT_MAX));
    scene->lightTypes = Array(MemoryArena::AllocSpan<LightType>(arena, LIGHT_MAX));
    scene->sceneCamera = {};
    scene->lightCount = 0;

    //Transforms (TODO)
    scene->transforms = {};

    scene->transforms.worldMatrices = Array(MemoryArena::AllocSpan<std::span<glm::mat4>>(arena, OBJECT_MAX));
    scene->transforms.transformNodes.reserve(OBJECT_MAX);
    scene->transforms.rootTransformsView = Array(MemoryArena::AllocSpan<localTransform*>(arena, OBJECT_MAX));
}

size_t shadowCountFromLightType(LightType t)
{
    return t == LIGHT_POINT ? 6 : t == LIGHT_DIR ? CASCADE_CT : 1;
}

//TODO JS: this sucks!
void Scene::UpdateCamera(inputData input)
{
    sceneCamera.eyeRotation += (input.mouseRot * 30000.0f * deltaTime);
    // 30000 degrees per full screen rotation per second
    if (sceneCamera.eyeRotation.y > 89.0f)
        sceneCamera.eyeRotation.y = 89.0f;
    if (sceneCamera.eyeRotation.y < -89.0f)
        sceneCamera.eyeRotation.y = -89.0f;

    glm::quat Orientation = OrientationFromYawPitch(sceneCamera.eyeRotation);

    glm::quat forwardQuat = Orientation * glm::quat(0, 0, 0, -1) * conjugate(Orientation);
    auto UP = glm::vec3(0, 1, 0);
    glm::vec3 Front = {forwardQuat.x, forwardQuat.y, forwardQuat.z};
    glm::vec3 RIGHT = normalize(cross(Front, UP));

    glm::vec3 translateFWD = Front * input.keyboardMove.y;
    glm::vec3 translateSIDE = RIGHT * input.keyboardMove.x;

    sceneCamera.eyePos += (translateFWD + translateSIDE) * deltaTime;
}

//Placeholder "gameplay" function
void Scene::UpdateRotations()
{
    //<Rotation update
    glm::vec3 EulerAngles = glm::vec3(0, 1, 0.00) * deltaTime / 10.0f; // One revolution per second
    auto MyQuaternion = glm::quat(EulerAngles);

    // Conversion from axis-angle
    // In GLM the angle must be in degrees here, so convert it.

    for (int i = 0; i < ObjectsCount(); i++)
    {
        objects.rotations[i] *= MyQuaternion;
    }
}


void Scene::Update()
{
    //This stuff goes in scene
    float translateSpeed = 3.0;
    inputData input = {
        glm::vec3(INPUT_translate_x, INPUT_translate_y, 0.0f) * translateSpeed, glm::vec2(INPUT_mouse_x, INPUT_mouse_y)
    };
    glm::mat4 model;

    UpdateCamera(input);
    UpdateRotations();
    for (int i = 0; i < objects.objectsCount; i++)
    {
        model = glm::mat4(1.0f);
        glm::mat4 objectLocalRotation = toMat4(objects.rotations[i]);
        model = translate(model, objects.translations[i]);
        model *= objectLocalRotation;
        model = scale(model, objects.scales[i]);
        transforms.get(objects.transformIDs[i])->matrix = model;
    }

    transforms.UpdateWorldTransforms();
}

size_t Scene::AddObject(std::span<ID::SubMeshID> submeshIndices, std::span<ID::MaterialID> materialIndices,
                     glm::vec3 position, glm::quat rotation, glm::vec3 scale, localTransform* parent, std::string name)
{
    uint32_t submeshCt = static_cast<uint32_t>(submeshIndices.size());


    //TODD JS: version that can add
    // objects.meshes.push_back(mesh);

    //Add our materials
   auto firstSubmeshMaterial = allMaterials.size();
    allMaterials.push_back_span(materialIndices);
    for(int i =0; i < submeshIndices.size() - materialIndices.size(); i++)
    {
        //Use the last material index for the remaining submeshes. Don't love this behavior, but it's convenient for now
        allMaterials.push_back(materialIndices.back());
    }
    objects.subMeshMaterials.push_back(allMaterials.getSpan().subspan(firstSubmeshMaterial, submeshIndices.size()));

    
    objects.translations.push_back(position);
    objects.rotations.push_back(rotation);
    objects.scales.push_back(scale);
    // transforms.worldMatrices.push_back(glm::mat4(1.0));
    objects.subMeshes.push_back(allSubmeshes.push_back_span(submeshIndices));
    objects.transformIDs.push_back(objects.transformIDs.size());

    //TODO JS: When we use real objects, we'll only create transforms with these ids
    // objects.meshVertCounts.push_back(mesh->vertcount);

    if (parent != nullptr)
    {
        localTransform newT = {
        .matrix =  glm::mat4(1.0),
        .name = "CHILD",
        .ID = objects.transformIDs[objects.objectsCount],
        .depth = static_cast<uint8_t>(parent->depth + 1),
        .children =  {}};
        transforms.transformNodes.push_back(newT);
        addChild(parent, &transforms.transformNodes[transforms.transformNodes.size() - 1]);
    }
    else
    {
        transforms.transformNodes.push_back({
            glm::mat4(1.0), name.empty() ? "default" : name, objects.transformIDs[objects.objectsCount], 0, {}
        });
        transforms.rootTransformsView.push_back(&transforms.transformNodes[transforms.transformNodes.size() - 1]);
    }


    objects.subMeshesCount += submeshCt;
    return objects.objectsCount++;
}


size_t Scene::ObjectsCount()
{
    return objects.objectsCount;
}

size_t Scene::DrawCount()
{
    return MeshesCount();
}

size_t Scene::MeshesCount()
{
    return objects.subMeshesCount;
}

size_t Scene:: GetTotalSubmeshesForObjects(std::span<uint32_t> objectIndices)
{
    size_t result = 0;
    for (auto element : objectIndices)
    {
        result += objects.subMeshes[element].size();
    }
    return result;
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

    for (int i = 0; i < lightCount; i++)
    {
        switch (lightTypes[i])
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
    std::span<LightType> tempType;

    //if there's room, use the back half of the existing arrays
    if (lightCount < LIGHT_MAX / 2)
    {
        tempPos = std::span<glm::vec4>(lightposandradius.data, LIGHT_MAX).subspan(LIGHT_MAX / 2, LIGHT_MAX / 2);
        tempCol = std::span<glm::vec4>(lightcolorAndIntensity.data, LIGHT_MAX).subspan(LIGHT_MAX / 2, LIGHT_MAX / 2);
        tempDir = std::span<glm::vec4>(lightDir.data, LIGHT_MAX).subspan(LIGHT_MAX / 2, LIGHT_MAX / 2);
        tempType = std::span<LightType>(lightTypes.data, LIGHT_MAX).subspan(LIGHT_MAX / 2, LIGHT_MAX / 2);
    }
    else
    {
        //not implemented
        assert(lightCount < LIGHT_MAX / 2);
        // tempPos = MemoryArena::AllocSpan<glm::vec4>(allocator, lightCount);
        // tempCol = MemoryArena::AllocSpan<glm::vec4>(allocator, lightCount);
        // tempDir = MemoryArena::AllocSpan<glm::vec4>(allocator, lightCount);
        // tempType = MemoryArena::AllocSpan<lightType>(allocator, lightCount);
    }

    for (int i = 0; i < lightCount; i++)
    {
        tempPos[i] = lightposandradius[i];
        tempCol[i] = lightcolorAndIntensity[i];
        tempDir[i] = lightDir[i];
        tempType[i] = lightTypes[i];
    }

    int i = 0;
    int j;
    for (j = 0; j < dirlightIndices.ct; j++)
    {
        lightposandradius[i + j] = tempPos[dirlightIndices[j]];
        lightcolorAndIntensity[i + j] = tempCol[dirlightIndices[j]];
        lightDir[i + j] = tempDir[dirlightIndices[j]];
        lightTypes[i + j] = tempType[dirlightIndices[j]];
    }
    i += j;
    for (j = 0; j < spotlightIndices.ct; j++)
    {
        lightposandradius[i + j] = tempPos[spotlightIndices[j]];
        lightcolorAndIntensity[i + j] = tempCol[spotlightIndices[j]];
        lightDir[i + j] = tempDir[spotlightIndices[j]];
        lightTypes[i + j] = tempType[spotlightIndices[j]];
    }
    i += j;
    for (j = 0; j < pointlightIndices.ct; j++)
    {
        lightposandradius[i + j] = tempPos[pointlightIndices[j]];
        lightcolorAndIntensity[i + j] = tempCol[pointlightIndices[j]];
        lightDir[i + j] = tempDir[pointlightIndices[j]];
        lightTypes[i + j] = tempType[pointlightIndices[j]];
    }
}

size_t Scene::getShadowDataIndex(size_t idx, std::span<LightType> lightTypes)
{
    size_t output = 0;
    for (size_t i = 0; i < idx; i++)
    {
        LightType type = lightTypes[i];
        output += shadowCountFromLightType(type);
    }
    return output;
}

int Scene::AddLight(glm::vec3 position, glm::vec3 dir, glm::vec3 color, float radius, float intensity, LightType type)
{
    lightposandradius.push_back(glm::vec4(position.x, position.y, position.z, radius));
    lightcolorAndIntensity.push_back(glm::vec4(color.x, color.y, color.z, intensity));
    lightDir.push_back(glm::vec4(dir, -1.0));
    lightTypes.push_back(type);
    lightCount++;
    lightSort();
    return 0;
}

int Scene::AddDirLight(glm::vec3 position, glm::vec3 color, float intensity)
{
    return this->AddLight(position, glm::vec3(-1), color, -1, intensity, LIGHT_DIR);
}

int Scene::AddSpotLight(glm::vec3 position, glm::vec3 dir, glm::vec3 color, float radius, float intensity)
{
    return this->AddLight(position, dir, color, radius, intensity, LIGHT_SPOT);
}

int Scene::AddPointLight(glm::vec3 position, glm::vec3 color, float intensity)
{
    return this->AddLight(position, glm::vec3(-1), color, -1, intensity, LIGHT_POINT);
}
