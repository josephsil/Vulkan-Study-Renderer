#include "Scene.h"

#include "engineGlobals.h"
#include "glm_misc.h"
#include "Transforms.h"
#include "General/Algorithms.h"
#include <algorithm>
#include "General/Array.h"
#include "General/InputHandling.h"
#include "General/MemoryArena.h"
#include "Renderer/AssetManagerTypes.h"
#include "Renderer/VulkanIncludes/structs_hlsl.h"

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
    scene->transforms.worldUniformScales = Array(MemoryArena::AllocSpan<std::span<float>>(arena, OBJECT_MAX));
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

    UpdateCamera(input);
    UpdateRotations();
    for (int i = 0; i < objects.objectsCount; i++)
    {
        glm::mat4& model =  transforms.get(objects.transformIDs[i])->matrix;
        model = glm::mat4(1.0f);
        glm::mat4 objectLocalRotation = toMat4(objects.rotations[i]);
        model = translate(model, objects.translations[i]);
        model *= objectLocalRotation;
        model = scale(model, objects.scales[i]);
        auto& scale = objects.scales[i];
        transforms.get(objects.transformIDs[i])->uniformScale = glm::max(scale.y, glm::max(scale.z, scale.x));
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
    allMaterials.push_copy_span(materialIndices);
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
    objects.subMeshes.push_back(allSubmeshes.push_copy_span(submeshIndices));
    objects.transformIDs.push_back(objects.transformIDs.size());

    //TODO JS: When we use real objects, we'll only create transforms with these ids
    // objects.meshVertCounts.push_back(mesh->vertcount);

    if (parent != nullptr)
    {
        localTransform newT = {
        .matrix =  glm::mat4(1.0),
            .uniformScale = glm::max(scale.x,glm::max(scale.y,scale.z)),
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
            glm::mat4(1.0),  glm::max(scale.x,glm::max(scale.y,scale.z)), name.empty() ? "default" : name, objects.transformIDs[objects.objectsCount], 0, {}
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

size_t Scene::MeshesCount()
{
    return objects.subMeshesCount;
}

int lightOrder(LightType t)
{
    if (t == LightType::LIGHT_DIR)
    {
        return 0;
    }
    if (t == LIGHT_SPOT)
    {
        return 2; 
    }
    if (t == LIGHT_POINT)
    {
         return 1;
    }
    return 0;
}



void Scene::lightSort()
{
	MemoryArena::memoryArena scratchArena; //TODO pass in 
	MemoryArena::initialize(&scratchArena);

	std::span<size_t> indices = GetIndicesAscending<size_t>(&scratchArena, lightCount);
	
	//Get indices sorted by light type
	std::sort(indices.data(), indices.data() + indices.size(), [ & ](size_t a, size_t b) 
			  { return (int)(lightTypes[a]) < (int)(lightTypes[b]); });
	
	ReorderSOA(&scratchArena, indices,  lightposandradius.getSpan(),
												lightcolorAndIntensity.getSpan(),
												lightDir.getSpan(),
												lightTypes.getSpan());
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
