#include "game.h"

//rendererContext JS: I'd love to include less
#include "Renderer/MeshCreation/meshData.h"
#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/gltf/gltfLoading.h"
#include "Renderer/MeshCreation/MeshData.h"
#include "Scene/AssetManager.h"
#include "Scene/Scene.h"
#include <glm/gtc/random.hpp>

#include "General/LinearDictionary.h"
#include "General/ThreadedTextureLoading.h"
#include "Renderer/MeshCreation/MeshOptimizer.h"

void Add_Scene_Content(PerThreadRenderContext rendererContext, AssetManager* rendererData, Scene* scene)
{
    auto sceneLoading = MemoryArena::memoryArena {};
    MemoryArena::initialize(&sceneLoading, 680000 * 420);
    rendererContext.tempArena = &sceneLoading;
    ArenaAllocator loadingArena = &sceneLoading;
    LinearDictionary<char*, TextureData> textureLookup = {};
    scene->sceneCamera.extent = {WIDTH, HEIGHT}; // ????
    std::vector<ID::SubMeshGroupID> randomMeshes;
    std::vector<ID::MaterialID> randomMaterials;
    std::vector<TextureData> completedTextures;

    std::vector<char*> paths;
    std::vector<TextureCreation::TextureImportRequest> creationArgs;

    creationArgs.push_back(TextureCreation::MakeCreationArgsFromFilepathArgs("textures/blank.png", loadingArena, DIFFUSE));
    paths.push_back(const_cast<char*>("textures/blank.png_diff"));
    creationArgs.push_back(TextureCreation::MakeCreationArgsFromFilepathArgs("textures/default_roug.tga",loadingArena,  SPECULAR));
    paths.push_back(const_cast<char*>("textures/default_roug.tga"));
    creationArgs.push_back(TextureCreation::MakeCreationArgsFromFilepathArgs("textures/blank.png", loadingArena,SPECULAR));
    paths.push_back(const_cast<char*>("textures/blank.png_spec"));
    creationArgs.push_back(TextureCreation::MakeCreationArgsFromFilepathArgs("textures/pbr_cruiser-panels/space-cruiser-panels2_albedo.png", loadingArena, DIFFUSE, VK_IMAGE_VIEW_TYPE_2D));
    paths.push_back(const_cast<char*>("textures/pbr_cruiser-panels/space-cruiser-panels2_albedo.png"));
    creationArgs.push_back(TextureCreation::MakeCreationArgsFromFilepathArgs("textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga", loadingArena, SPECULAR, VK_IMAGE_VIEW_TYPE_2D));
    paths.push_back(const_cast<char*>("textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga"));
    creationArgs.push_back(TextureCreation::MakeCreationArgsFromFilepathArgs("textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png", loadingArena, NORMAL, VK_IMAGE_VIEW_TYPE_2D));
    paths.push_back(const_cast<char*>("textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png"));

    auto resultTexturedata = MemoryArena::AllocSpan<TextureData>(loadingArena, creationArgs.size());
    
    LoadTexturesThreaded(rendererContext, resultTexturedata, creationArgs);
    for(int i =0; i < resultTexturedata.size(); i++)
    {
        textureLookup.Push(paths[i], resultTexturedata[i]);
    }


    
    ID::TextureID defaultTexture = rendererData->AddTexture((textureLookup.Find(const_cast<char*>("textures/blank.png_diff"))));
    ID::TextureID defaultSPec = rendererData->AddTexture(  (textureLookup.Find(const_cast<char*>("textures/default_roug.tga"))));    
    ID::TextureID defaultNormal = rendererData->AddTexture(  (textureLookup.Find(const_cast<char*>("textures/blank.png_spec"))));

    //Set
    auto setdiff1 = (  (textureLookup.Find(const_cast<char*>("textures/pbr_cruiser-panels/space-cruiser-panels2_albedo.png"))));    
    auto setSpec1 = (  (textureLookup.Find(const_cast<char*>("textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga"))));
    auto setNorm1 = (  (textureLookup.Find(const_cast<char*>("textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png"))));    ID::MaterialID placeholderMatidx;
    auto placeholderTextureidx = rendererData->AddTextureSet(
        setdiff1,
        setSpec1,setNorm1);

    placeholderMatidx = rendererData->AddMaterial(0.2f, 0, glm::vec3(1.0f), placeholderTextureidx, 1);
    randomMaterials.push_back(placeholderMatidx);

    auto setdiff2 = (  (textureLookup.Find(const_cast<char*>("textures/pbr_cruiser-panels/space-cruiser-panels2_albedo.png"))));    
    auto setSpec2 = (  (textureLookup.Find(const_cast<char*>("textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga"))));
    auto setNorm2 = (  (textureLookup.Find(const_cast<char*>("textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png"))));
    placeholderTextureidx = rendererData->AddTextureSet(
       setdiff2,
       setSpec2,setNorm2);
    
    placeholderMatidx = rendererData->AddMaterial(0.2f, 0, glm::vec3(1.0f), placeholderTextureidx, 1);
    randomMaterials.push_back(placeholderMatidx);


    //spot light
    //rendererContext JS: paramaterize better -- hard to set power and radius currently
    scene->AddSpotLight(glm::vec3(2.5, 2, 3.3), glm::vec3(0, 0, -1), glm::vec3(0.5, 0.5, 1), 45, 260);


    //point lights    
    // scene->AddPointLight(glm::vec3(1, 1, 0), glm::vec3(0.3, 0.8, 1), 2);
    scene->AddDirLight(glm::vec3(0.00, 0.9, 0.1), glm::vec3(1, 1, 1), 1);
    scene->AddPointLight(glm::vec3(-2, 2, 0), glm::vec3(1, 0, 0), 999 / 2);
    // scene->AddPointLight(glm::vec3(0, 0, 0), glm::vec3(0, 1, 1), 999 / 2);

    GltfData gltf;
#define SPONZA
#ifdef SPONZA
    //rendererContext: gltf load fn that gets back struct, then append its contents to scene 
    gltf = GltfLoadMeshes(rendererContext,*rendererData, "Meshes/sponza.glb");
    ObjectImport::CreateObjectAssets(loadingArena, *scene, *rendererData, gltf, {defaultTexture, defaultSPec,defaultNormal});
#pragma region gltf awwwwwwwwwdding stuff --- todo move to fn
   
#endif
#pragma endregion
    printf("objects count: %llu \n", scene->objects.objectsCount);
    gltf = GltfLoadMeshes(rendererContext,*rendererData, "Meshes/pig.glb");
    
    
    auto setSpec3 = (  (textureLookup.Find(const_cast<char*>("textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga"))));
    auto setNorm3 = (  (textureLookup.Find(const_cast<char*>("textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png"))));
    
    placeholderTextureidx = rendererData->AddTextureSet(
        gltf.textures[gltf.materials[0].diffIndex],
        setSpec3,
        setNorm3);
    
    placeholderMatidx = rendererData->AddMaterial(0.2f, 0, glm::vec3(1.0f), placeholderTextureidx, 1);
    randomMaterials.push_back(placeholderMatidx);
    
    randomMeshes.push_back(rendererData->AddMultiSubmeshMeshMesh2(gltf.meshes[0].submeshes));
    auto _cubesphere = GltfLoadMeshes(rendererContext,*rendererData, "Meshes/cubesphere.glb");
    randomMeshes.push_back(rendererData->AddMultiSubmeshMeshMesh2(_cubesphere.meshes[0].submeshes));
    
    auto monkeyMesh = MeshDataCreation::MeshDataFromObjFile(rendererContext, "Meshes/monkey.obj");
    
    auto meshLetMonkey =  MeshOptimizer::RunMeshOptimizer(rendererContext.tempArena, *rendererData, monkeyMesh);
    auto meshletMonkeyMeshletInfo = MemoryArena::AllocSpan<meshletIndexInfo>(rendererContext.tempArena, 1);
    meshletMonkeyMeshletInfo[0] = {0, meshLetMonkey.meshletCount};
    randomMeshes.push_back(rendererData->AddMultiSubmeshMeshMesh2({&meshLetMonkey, 1}));
    auto _cube =GltfLoadMeshes(rendererContext, *rendererData, "Meshes/cube.glb");
    ID::SubMeshID cube = rendererData->AddMultiSubmeshMeshMesh2(_cube.meshes[0].submeshes);
    
    //direciton light
    
    
    glm::vec3 EulerAngles(0, 0, 0);
    auto MyQuaternion = glm::quat(EulerAngles);
    
    auto root = scene->AddObject(
        rendererData->subMeshGroups[randomMeshes[rand() % randomMeshes.size()]].getSpan(), MemoryArena::AllocSpanEmplaceInitialize<uint32_t>(rendererContext.arena, 1, (uint32_t) (uint32_t)randomMaterials[1]), glm::vec4(0, 0, 0, 0) * 1.2f, MyQuaternion,
        glm::vec3(0.5));

    
    localTransform* tform = &scene->transforms.transformNodes[root];
    for (int i = 0; i < 50; i++)
    {
        for (int j = i == 0 ? 1 : 0; j < 20; j++)
        {
            float rowRoughness = static_cast<float>(glm::clamp(static_cast<float>(i) / 8.0, 0.0, 1.0));
            bool rowmetlalic = i % 3 == 0;
            int matIDX = rand() % randomMaterials.size();
    
            auto sourceMaterial = rendererData->materials[matIDX];
            auto randColor = glm::ballRand(0.5) + 0.5;
            auto randColorLinear = randColor*randColor;
            auto newRandomColorMaterial = rendererData->AddMaterial(
            sourceMaterial.roughness, sourceMaterial.metallic, randColorLinear, {sourceMaterial.diffuseIndex, sourceMaterial.specIndex, sourceMaterial.normalIndex}, sourceMaterial.shaderGroupIndex);
    
            scene->AddObject(
                rendererData->subMeshGroups[randomMeshes[rand() % randomMeshes.size()]].getSpan(), 
               MemoryArena::AllocSpanEmplaceInitialize<uint32_t>(rendererContext.arena, 1, (uint32_t)newRandomColorMaterial), glm::vec4((j), (i / 10) * 1.0, -(i % 10), 1) * 1.2f, MyQuaternion,
                glm::vec3(0.5));
            matIDX = rand() % randomMaterials.size();
        }
    }

    scene->transforms.RebuildTransformDataFromNodes(rendererContext.arena);
}
