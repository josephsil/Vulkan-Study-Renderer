#include <Renderer/ObjectImport.h>

#include <General/MemoryArena.h>
#include <Scene/AssetManager.h>
#include <Scene/Transforms.h>

#include "MeshCreation/meshData.h"
#include "Scene/Scene.h"


struct AddObjectResult
{
    
};
void ObjectImport::CreateObjectAssets(Allocator &arena, Scene &scene, AssetManager &assetManager, ImportedObjectData &gltf, DefaultTextures defaults)
{
  std::span<std::span<size_t>> gltfSubmeshIndexToAssetManagerIndex = MemoryArena::AllocSpan<std::span<size_t>>(
   arena, gltf.meshes.size());
    std::span<size_t> gltfTextureToAssetManagerTextureIndex = MemoryArena::AllocSpan<size_t>(arena, gltf.textures.size());
    std::span<size_t> gltfMaterialIndexToAssetManagerIndex = MemoryArena::AllocSpan<size_t>(arena, gltf.materials.size());

    //< Tform
    int addedCt = 0;
    std::span<bool> created = MemoryArena::AllocSpan<bool>(arena, gltf.objects.size());
    std::span<localTransform*> tforms = MemoryArena::AllocSpan<localTransform*>(arena, gltf.objects.size());

    std::span<size_t> parent = MemoryArena::AllocSpanEmplaceInitialize<size_t>(arena, gltf.objects.size(), SIZE_MAX);

    /// />


    for (int i = 0; i < gltf.meshes.size(); i++)
    {
        gltfSubmeshIndexToAssetManagerIndex[i] = MemoryArena::AllocSpan<size_t>(arena, gltf.meshes[i].submeshes.size());
        for (int j = 0; j < gltf.meshes[i].submeshes.size(); j++)
        {
            gltfSubmeshIndexToAssetManagerIndex[i][j] = assetManager.AddBackingMesh(gltf.meshes[i].submeshes[j]);
        }
    }
    for (int i = 0; i < gltf.textures.size(); i++)
    {
        gltfTextureToAssetManagerTextureIndex[i] = assetManager.AddTexture(gltf.textures[i]);
    }
    for (int i = 0; i < gltf.materials.size(); i++)
    {
        auto mat = gltf.materials[i];
        textureSetIDs textures = {
            mat.diffIndex >= 0 ? gltfTextureToAssetManagerTextureIndex[mat.diffIndex] : defaults.defaultTexture,
            mat.specIndex >= 0 ? gltfTextureToAssetManagerTextureIndex[mat.specIndex] : defaults.defaultSPec,
            mat.normIndex >= 0 ? gltfTextureToAssetManagerTextureIndex[mat.normIndex] : defaults.defaultNormal
        };

        gltfMaterialIndexToAssetManagerIndex[i] = assetManager.AddMaterial(mat.roughnessFactor, mat.metallicFactor, mat.baseColorFactor,
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
                //Submeshes -- todo 
                for (int j = 0; j < mesh.submeshes.size(); j++)
                {
                    size_t gltfMatIDX = mesh.materialIndices[j];
                    size_t sceneMatIDX = gltfMaterialIndexToAssetManagerIndex[mesh.materialIndices[j]];
                    size_t objectID = 0;
                    localTransform* parentTransform = (parent[i] != -1) ? tforms[parent[i]] : nullptr;
                    //rendererContext JS: add objcet that takes matrix
                    objectID = scene.AddObject(
                        gltfSubmeshIndexToAssetManagerIndex[object.meshidx][j],
                        sceneMatIDX,
                        object.translation,
                        object.rotation,
                        object.scale,
                        parentTransform);

                    //Submeshes -- todo 
                    if (j == 0)
                    {
                        tforms[i] = &scene.transforms.transformNodes[objectID];
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

  

     
}

// void ObjectImport::AddObjectToScene(Allocator &arena, Scene &scene, ImportedObjectData &gltf)
// {
//     int addedCt = 0;
//     std::span<bool> created = MemoryArena::AllocSpan<bool>(arena, gltf.objects.size());
//     std::span<localTransform*> tforms = MemoryArena::AllocSpan<localTransform*>(arena, gltf.objects.size());
//
//     std::span<size_t> parent = MemoryArena::AllocSpanEmplaceInitialize<size_t>(arena, gltf.objects.size(), SIZE_MAX);
//
//     
// }
