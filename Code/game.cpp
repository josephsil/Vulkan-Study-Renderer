#include "game.h"

//TODO JS: I'd love to include less
#include "Renderer/meshData.h"
#include "Renderer/TextureData.h"
#include "Renderer/gltf/gltfLoading.h"
#include "Scene/RendererSceneData.h"
#include "Scene/Scene.h"

void Add_Scene_Content(RendererContext rendererContext, RendererSceneData* rendererData, Scene* scene)
{
    std::vector<int> randomMeshes;
    std::vector<int> randomMaterials;
     scene->sceneCamera.extent = {16, 10}; // ????
    //Original transform test  
    // localTransform root1 = {{}, "ROOT 1", TRANSFORM_ID++, {0}, {}};
    // localTransform root2 = {{}, "ROOT 2", TRANSFORM_ID++, {0}, {}};
    // auto child = AddChild(&root1, "CHILD1",TRANSFORM_ID++, {});
    // auto child1_1 = AddChild(child.get(), "CHILD1_CHILD1",TRANSFORM_ID++, {});
    // AddChild(child.get(), "CHILD1_CHILD2",TRANSFORM_ID++, {});
    // AddChild(child.get(), "CHILD1_CHILD3",TRANSFORM_ID++, {});
    // auto child1_4 = AddChild(child.get(), "CHILD1_CHILD4",TRANSFORM_ID++, {});
    // AddChild(child1_1.get(), "CHILD1_CHILD1_CHILD1",TRANSFORM_ID++, {});
    // AddChild(child1_4.get(), "CHILD1_CHILD4_CHILD1",TRANSFORM_ID++, {});
    // printf("PRE REMOVE ==== \n");
    // printGraph(root1, 0 );
    // rmChild(child.get(), child1_1);
    // printf("POST REMOVE ==== \n");
    // printGraph(root1, 0 );
    //
    // std::span roots = std::span(&root1, 1);
    // flattenTransformHiearchy(roots);
    //
    // localTransform* walk = &root1;
    // while(!walk->children.empty())
    // {
    //     printf("looking up from %s to %s \n", walk->name.c_str(), lookupflt(walk->ID)->name.c_str());
    //     walk = walk->children[0].get();
    // }
  
    // exit(10);

    int defaultTexture = rendererData->AddTexture(TextureData(rendererContext, "textures/blank.png", TextureData::DIFFUSE));
    int defaultSPec = rendererData->AddTexture(TextureData(rendererContext, "textures/default_roug.tga", TextureData::SPECULAR));
    int defaultNormal = rendererData->AddTexture(TextureData(rendererContext, "textures/blank.png", TextureData::SPECULAR));

    int placeholderMatidx;
    auto placeholderTextureidx = rendererData->AddTextureSet(
        TextureData(rendererContext, "textures/pbr_cruiser-panels/space-cruiser-panels2_albedo.png",
                    TextureData::TextureType::DIFFUSE),
        TextureData(rendererContext, "textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga",
                    TextureData::TextureType::SPECULAR),
        TextureData(rendererContext, "textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png",
                    TextureData::TextureType::NORMAL));

    placeholderMatidx = rendererData->AddMaterial(0.2, 0, glm::vec3(1.0f), placeholderTextureidx, 1);
    randomMaterials.push_back(placeholderMatidx);


    placeholderTextureidx = rendererData->AddTextureSet(
        TextureData(rendererContext, "textures/pbr_cruiser-panels/space-cruiser-panels2_albedo.png",
                    TextureData::TextureType::DIFFUSE),
        TextureData(rendererContext, "textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga",
                    TextureData::TextureType::SPECULAR),
        TextureData(rendererContext, "textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png",
                    TextureData::TextureType::NORMAL));
    placeholderMatidx = rendererData->AddMaterial(0.2, 0, glm::vec3(1.0f), placeholderTextureidx, 1);
    randomMaterials.push_back(placeholderMatidx);


#define SPONZA

    //spot light
    //TODO JS: paramaterize better -- hard to set power and radius currently
    scene->AddSpotLight(glm::vec3(2.5, 3, 3.3), glm::vec3(0, 0, -1), glm::vec3(1, 0, 1), 45, 14000);
    
    
    //point lights    
    scene->AddPointLight(glm::vec3(1, 1, 0), glm::vec3(1, 1, 1), 55);
    scene->AddDirLight(glm::vec3(0,0,1), glm::vec3(1,1,1), 3);
    scene->AddDirLight(glm::vec3(0.00, 1, 0),  glm::vec3(1, 1, 1), 33);
    scene->AddPointLight(glm::vec3(-2, 2, 0), glm::vec3(1, 0, 0), 4422 / 2);
    scene->AddPointLight(glm::vec3(0, 0, 0), glm::vec3(1, 1, 0), 999 / 2);

    #ifdef SPONZA 
    //TODO: gltf load fn that gets back struct, then append its contents to scene 
    auto gltf = GltfLoadMeshes(rendererContext, "Meshes/sponza.glb");
    //TODO JS: to span of spans for submeshes

#pragma region gltf adding stuff --- todo move to fn 
    std::span<std::span<int>> meshLUT = MemoryArena::AllocSpan<std::span<int>>(rendererContext.perframeArena, gltf.meshes.size());
    std::span<int> textureLUT = MemoryArena::AllocSpan<int>(rendererContext.perframeArena, gltf.textures.size());
    std::span<int> materialLUT = MemoryArena::AllocSpan<int>(rendererContext.perframeArena, gltf.materials.size());
    std::span<int> parent = MemoryArena::AllocSpan<int>(rendererContext.perframeArena, gltf.objects.size());
    std::span<bool> created = MemoryArena::AllocSpan<bool>(rendererContext.perframeArena, gltf.objects.size());
    std::span<localTransform*> tforms = MemoryArena::AllocSpan<localTransform*>(rendererContext.perframeArena, gltf.objects.size());
    for(int i =0; i < parent.size(); i++)
    {
        parent[i] = -1;
    }


    for(int i = 0; i < gltf.meshes.size(); i++)
    {
        meshLUT[i] =MemoryArena::AllocSpan<int>(rendererContext.perframeArena, gltf.meshes[i].submeshes.size());
        for(int j = 0; j < gltf.meshes[i].submeshes.size(); j++)
        {
            meshLUT[i][j] = rendererData->AddBackingMesh(gltf.meshes[i].submeshes[j]);
        }
    }
    for(int i =0; i < gltf.textures.size(); i++)
    {
       textureLUT[i] = rendererData->AddTexture(gltf.textures[i]);
    }
    for(int i =0; i < gltf.materials.size(); i++)
    {
        auto mat = gltf.materials[i];
       textureSetIDs textures =  {mat.diffIndex >= 0 ? textureLUT[mat.diffIndex] : defaultTexture,
            mat.specIndex >= 0 ? textureLUT[mat.specIndex] :
            defaultSPec, mat.normIndex >= 0 ? textureLUT[mat.normIndex] : defaultNormal};

        materialLUT[i] = rendererData->AddMaterial(mat.roughnessFactor, mat.metallicFactor, mat.baseColorFactor, textures, 1);
    }

    //prepass to set up parents
    for(int i = 0; i < gltf.objects.size(); i++)
    {
        for(int j =0; j < gltf.objects[i].children.size(); j++)
        {
            parent[gltf.objects[i].children[j]] = i;
        }
    }
    int addedCt = 0;

    
    //lol
    while(addedCt < gltf.objects.size())
    {
        for(int i = 0; i < gltf.objects.size(); i++)
        {
            auto object = gltf.objects[i];
            if (parent[i] == -1 || created[parent[i]])
            {
                if (created[i]) continue;
                auto mesh  =  gltf.meshes[gltf.objects[i].meshidx];
                for(int j = 0; j < mesh.submeshes.size(); j++)
                {
                    int gltfMatIDX = mesh.materialIndices[j];
                    int sceneMatIDX =  materialLUT[mesh.materialIndices[j]];
                    int objectID = 0;
                    localTransform* parentTransform =( parent[i] != -1) ? tforms[parent[i]] : nullptr;
                    //TODO JS: add objcet that takes matrix
                    objectID = scene->AddObject(
                         meshLUT[object.meshidx][j],
                        sceneMatIDX,
                        object.translation,
                        object.rotation,
                        object.scale,
                        parentTransform);
                
                    if (j == 0)
                    {
                        tforms[i] = &scene->transforms.transformNodes[objectID]; //Only the first one can have children because these are like "fake objects"}
                        created[i] = true;
                    }
                }
                addedCt++;
            }
            else
            {
                continue;
            }
        }
    }

#pragma endregion 
    printf("objects count: %d \n", scene->objects.objectsCount);
    // #else
    gltf = GltfLoadMeshes(rendererContext, "Meshes/pig.glb");
    placeholderTextureidx = rendererData->AddTextureSet(
    gltf.textures[gltf.materials[0].diffIndex],
      TextureData(rendererContext, "textures/pbr_stainless-steel/used-stainless-steel2_roughness.png",
                  TextureData::TextureType::SPECULAR),
      TextureData(rendererContext, "textures/pbr_stainless-steel/used-stainless-steel2_normal-dx.png",
                  TextureData::TextureType::NORMAL));

    placeholderMatidx = rendererData->AddMaterial(0.2, 0, glm::vec3(1.0f), placeholderTextureidx, 1);
    randomMaterials.push_back(placeholderMatidx);
    
    randomMeshes.push_back(rendererData->AddBackingMesh(gltf.meshes[0].submeshes[0]));
    randomMeshes.push_back(rendererData->AddBackingMesh(GltfLoadMeshes(rendererContext, "Meshes/cubesphere.glb").meshes[0].submeshes[0]));
    randomMeshes.push_back(rendererData->AddBackingMesh(MeshDataFromObjFile(rendererContext, "Meshes/monkey.obj")));
    
    int cube = rendererData->AddBackingMesh(GltfLoadMeshes(rendererContext, "Meshes/cube.glb").meshes[0].submeshes[0]);
    
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
        for (int j = i == 0 ? 1 : 0 ; j < 10; j ++)
        {
            float rowRoughness = (float)glm::clamp(static_cast<float>(i) / 8.0, 0.0, 1.0);
            bool rowmetlalic = i % 3 == 0;
            int matIDX = rand() % randomMaterials.size();
    
            scene->AddObject(
                randomMeshes[rand() % randomMeshes.size()],
                randomMaterials[matIDX], glm::vec4((j), (i / 10) * 1.0, - (i % 10), 1) * 1.2f, MyQuaternion,
                glm::vec3(0.5));
            matIDX = rand() % randomMaterials.size();
        }
    
    }
#endif 
    scene->transforms.RebuildTransformDataFromNodes(rendererContext.arena);
}
