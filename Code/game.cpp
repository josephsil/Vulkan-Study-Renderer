#include "game.h"

//rendererContext JS: I'd love to include less
#include "Renderer/MeshCreation/meshData.h"
#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/gltf/gltfLoading.h"
#include "Renderer/MeshCreation/MeshData.h"
#include "Scene/AssetManager.h"
#include "Scene/Scene.h"

void Add_Scene_Content(PerThreadRenderContext rendererContext, AssetManager* rendererData, Scene* scene)
{
    scene->sceneCamera.extent = {16, 10}; // ????
    std::vector<size_t> randomMeshes;
    std::vector<size_t> randomMaterials;
    std::vector<TextureCreation::TextureCreationStep1Result> enqueuedTextures;
    std::vector<TextureData> comletedTextures;

    enqueuedTextures.push_back(CreateTextureFromArgs_Start( rendererContext, TextureCreation::MakeCreationArgsFromFilepathArgs("textures/blank.png", DIFFUSE)));
    comletedTextures.push_back (CreateTextureFromArgsFinalize(rendererContext, enqueuedTextures.back()));

    size_t defaultTexture = rendererData->AddTexture(comletedTextures.back());

    enqueuedTextures.push_back (CreateTextureFromArgs_Start(    rendererContext, TextureCreation::MakeCreationArgsFromFilepathArgs("textures/default_roug.tga",  SPECULAR)));
    comletedTextures.push_back(CreateTextureFromArgsFinalize(rendererContext, enqueuedTextures.back()));
    size_t defaultSPec = rendererData->AddTexture(comletedTextures.back());
    
    enqueuedTextures.push_back (CreateTextureFromArgs_Start(rendererContext, TextureCreation::MakeCreationArgsFromFilepathArgs("textures/blank.png",SPECULAR)));
    comletedTextures.push_back(CreateTextureFromArgsFinalize(rendererContext, enqueuedTextures.back()));
    size_t defaultNormal = rendererData->AddTexture(comletedTextures.back());


    //Set
    enqueuedTextures.push_back (CreateTextureFromArgs_Start(rendererContext, TextureCreation::MakeCreationArgsFromFilepathArgs("textures/pbr_cruiser-panels/space-cruiser-panels2_albedo.png", DIFFUSE,
                                                                VK_IMAGE_VIEW_TYPE_2D)));
    comletedTextures.push_back(CreateTextureFromArgsFinalize(rendererContext, enqueuedTextures.back()));
    auto setdiff1 = (comletedTextures.back());
    
    enqueuedTextures.push_back (CreateTextureFromArgs_Start(rendererContext, TextureCreation::MakeCreationArgsFromFilepathArgs("textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga", SPECULAR,
                                                                VK_IMAGE_VIEW_TYPE_2D)));
    comletedTextures.push_back(CreateTextureFromArgsFinalize(rendererContext, enqueuedTextures.back()));
    auto setSpec1 = (comletedTextures.back());

    enqueuedTextures.push_back (CreateTextureFromArgs_Start(rendererContext, TextureCreation::MakeCreationArgsFromFilepathArgs("textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png", NORMAL,
                                                                VK_IMAGE_VIEW_TYPE_2D)));
    comletedTextures.push_back(CreateTextureFromArgsFinalize(rendererContext, enqueuedTextures.back()));
    auto setNorm1 = (comletedTextures.back());
    size_t placeholderMatidx;
    auto placeholderTextureidx = rendererData->AddTextureSet(
        setdiff1,
        setSpec1,setNorm1);

    placeholderMatidx = rendererData->AddMaterial(0.2f, 0, glm::vec3(1.0f), placeholderTextureidx, 1);
    randomMaterials.push_back(placeholderMatidx);

    enqueuedTextures.push_back (CreateTextureFromArgs_Start(rendererContext, TextureCreation::MakeCreationArgsFromFilepathArgs("textures/pbr_cruiser-panels/space-cruiser-panels2_albedo.png", DIFFUSE,
                                                                VK_IMAGE_VIEW_TYPE_2D)));
    comletedTextures.push_back(CreateTextureFromArgsFinalize(rendererContext, enqueuedTextures.back()));
    auto setdiff2 = (comletedTextures.back());
    
    enqueuedTextures.push_back (CreateTextureFromArgs_Start(rendererContext, TextureCreation::MakeCreationArgsFromFilepathArgs("textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga", SPECULAR,
                                                                VK_IMAGE_VIEW_TYPE_2D)));
    comletedTextures.push_back(CreateTextureFromArgsFinalize(rendererContext, enqueuedTextures.back()));
    auto setSpec2 = (comletedTextures.back());

    enqueuedTextures.push_back (CreateTextureFromArgs_Start(rendererContext, TextureCreation::MakeCreationArgsFromFilepathArgs("textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png", NORMAL,
                                                                VK_IMAGE_VIEW_TYPE_2D)));
    comletedTextures.push_back(CreateTextureFromArgsFinalize(rendererContext, enqueuedTextures.back()));
    auto setNorm2 = (comletedTextures.back());
     placeholderTextureidx = rendererData->AddTextureSet(
       setdiff2,
       setSpec2,setNorm2);
    
    placeholderMatidx = rendererData->AddMaterial(0.2f, 0, glm::vec3(1.0f), placeholderTextureidx, 0);
    randomMaterials.push_back(placeholderMatidx);


    //spot light
    //rendererContext JS: paramaterize better -- hard to set power and radius currently
    scene->AddSpotLight(glm::vec3(2.5, 3, 3.3), glm::vec3(0, 0, -1), glm::vec3(0.5, 0.5, 1), 45, 14000);


    //point lights    
    // scene->AddPointLight(glm::vec3(1, 1, 0), glm::vec3(1, 1, 1), 55);
    // scene->AddDirLight(glm::vec3(0,0,1), glm::vec3(1,1,1), 3);
    scene->AddDirLight(glm::vec3(0.00, 1, 0), glm::vec3(1, 1, 1), 33);
    // scene->AddPointLight(glm::vec3(-2, 2, 0), glm::vec3(1, 0, 0), 4422 / 2);
    // scene->AddPointLight(glm::vec3(0, 0, 0), glm::vec3(1, 1, 0), 999 / 2);

    gltfdata gltf;
#define SPONZA
#ifdef SPONZA
    //rendererContext: gltf load fn that gets back struct, then append its contents to scene 
    gltf = GltfLoadMeshes(rendererContext, "Meshes/sponza.glb");
    //rendererContext JS: to span of spans for submeshes

#pragma region gltf adding stuff --- todo move to fn
    std::span<std::span<size_t>> meshLUT = MemoryArena::AllocSpan<std::span<size_t>>(
        rendererContext.tempArena, gltf.meshes.size());
    std::span<size_t> textureLUT = MemoryArena::AllocSpan<size_t>(rendererContext.tempArena, gltf.textures.size());
    std::span<size_t> materialLUT = MemoryArena::AllocSpan<size_t>(rendererContext.tempArena, gltf.materials.size());
    std::span<size_t> parent = MemoryArena::AllocSpan<size_t>(rendererContext.tempArena, gltf.objects.size());
    std::span<bool> created = MemoryArena::AllocSpan<bool>(rendererContext.tempArena, gltf.objects.size());
    std::span<localTransform*> tforms = MemoryArena::AllocSpan<localTransform*>(
        rendererContext.tempArena, gltf.objects.size());
    for (int i = 0; i < parent.size(); i++)
    {
        parent[i] = -1;
    }


    for (int i = 0; i < gltf.meshes.size(); i++)
    {
        meshLUT[i] = MemoryArena::AllocSpan<size_t>(rendererContext.tempArena, gltf.meshes[i].submeshes.size());
        for (int j = 0; j < gltf.meshes[i].submeshes.size(); j++)
        {
            meshLUT[i][j] = rendererData->AddBackingMesh(gltf.meshes[i].submeshes[j]);
        }
    }
    for (int i = 0; i < gltf.textures.size(); i++)
    {
        textureLUT[i] = rendererData->AddTexture(gltf.textures[i]);
    }
    for (int i = 0; i < gltf.materials.size(); i++)
    {
        auto mat = gltf.materials[i];
        textureSetIDs textures = {
            mat.diffIndex >= 0 ? textureLUT[mat.diffIndex] : defaultTexture,
            mat.specIndex >= 0 ? textureLUT[mat.specIndex] : defaultSPec,
            mat.normIndex >= 0 ? textureLUT[mat.normIndex] : defaultNormal
        };

        materialLUT[i] = rendererData->AddMaterial(mat.roughnessFactor, mat.metallicFactor, mat.baseColorFactor,
                                                   textures, 1);
    }

    //prepass to set up parents
    for (int i = 0; i < gltf.objects.size(); i++)
    {
        for (int j = 0; j < gltf.objects[i].children.size(); j++)
        {
            parent[gltf.objects[i].children[j]] = i;
        }
    }
    int addedCt = 0;


    //lol
    while (addedCt < gltf.objects.size())
    {
        for (int i = 0; i < gltf.objects.size(); i++)
        {
            auto object = gltf.objects[i];
            if (parent[i] == -1 || created[parent[i]])
            {
                if (created[i]) continue;
                auto mesh = gltf.meshes[gltf.objects[i].meshidx];
                for (int j = 0; j < mesh.submeshes.size(); j++)
                {
                    size_t gltfMatIDX = mesh.materialIndices[j];
                    size_t sceneMatIDX = materialLUT[mesh.materialIndices[j]];
                    size_t objectID = 0;
                    localTransform* parentTransform = (parent[i] != -1) ? tforms[parent[i]] : nullptr;
                    //rendererContext JS: add objcet that takes matrix
                    objectID = scene->AddObject(
                        meshLUT[object.meshidx][j],
                        sceneMatIDX,
                        object.translation,
                        object.rotation,
                        object.scale,
                        parentTransform);

                    if (j == 0)
                    {
                        tforms[i] = &scene->transforms.transformNodes[objectID];
                        //Only the first one can have children because these are like "fake objects"}
                        created[i] = true;
                    }
                }
                addedCt++;
            }
            else
            {
            }
        }
    }
#endif
#pragma endregion
    printf("objects count: %llu \n", scene->objects.objectsCount);
    // #else
    gltf = GltfLoadMeshes(rendererContext, "Meshes/pig.glb");

    enqueuedTextures.push_back (CreateTextureFromArgs_Start(rendererContext, TextureCreation::MakeCreationArgsFromFilepathArgs("textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga", SPECULAR,
                                                                VK_IMAGE_VIEW_TYPE_2D)));
    comletedTextures.push_back(CreateTextureFromArgsFinalize(rendererContext, enqueuedTextures.back()));
    auto setSpec3 = (comletedTextures.back());

    enqueuedTextures.push_back (CreateTextureFromArgs_Start(rendererContext, TextureCreation::MakeCreationArgsFromFilepathArgs("textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png", NORMAL,
                                                                VK_IMAGE_VIEW_TYPE_2D)));
    comletedTextures.push_back(CreateTextureFromArgsFinalize(rendererContext, enqueuedTextures.back()));
    auto setNorm3 = (comletedTextures.back());

    
    placeholderTextureidx = rendererData->AddTextureSet(
        gltf.textures[gltf.materials[0].diffIndex],
        setSpec3,
        setNorm3);

    placeholderMatidx = rendererData->AddMaterial(0.2f, 0, glm::vec3(1.0f), placeholderTextureidx, 1);
    randomMaterials.push_back(placeholderMatidx);

    randomMeshes.push_back(rendererData->AddBackingMesh(gltf.meshes[0].submeshes[0]));
    randomMeshes.push_back(
        rendererData->AddBackingMesh(GltfLoadMeshes(rendererContext, "Meshes/cubesphere.glb").meshes[0].submeshes[0]));
    randomMeshes.push_back(rendererData->AddBackingMesh(MeshDataCreation::MeshDataFromObjFile(rendererContext, "Meshes/monkey.obj")));

    size_t cube = rendererData->AddBackingMesh(GltfLoadMeshes(rendererContext, "Meshes/cube.glb").meshes[0].submeshes[0]);

    //direciton light


    glm::vec3 EulerAngles(0, 0, 0);
    auto MyQuaternion = glm::quat(EulerAngles);

    auto root = scene->AddObject(
        randomMeshes[rand() % randomMeshes.size()],
        randomMaterials[1], glm::vec4(0, 0, 0, 0) * 1.2f, MyQuaternion,
        glm::vec3(0.5));

    localTransform* tform = &scene->transforms.transformNodes[root];
    for (int i = 0; i < 100; i++)
    {
        for (int j = i == 0 ? 1 : 0; j < 10; j++)
        {
            float rowRoughness = static_cast<float>(glm::clamp(static_cast<float>(i) / 8.0, 0.0, 1.0));
            bool rowmetlalic = i % 3 == 0;
            int matIDX = rand() % randomMaterials.size();

            scene->AddObject(
                randomMeshes[rand() % randomMeshes.size()],
                randomMaterials[matIDX], glm::vec4((j), (i / 10) * 1.0, -(i % 10), 1) * 1.2f, MyQuaternion,
                glm::vec3(0.5));
            matIDX = rand() % randomMaterials.size();
        }
    }

    scene->transforms.RebuildTransformDataFromNodes(rendererContext.arena);
}
