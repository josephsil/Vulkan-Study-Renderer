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
    auto perSubmeshMeshHandles = MemoryArena::AllocSpan<std::span<ID::MeshID>>(arena, gltf.meshes.size());
    auto perSubmeshMaterialHandles = MemoryArena::AllocSpan<std::span<ID::MaterialID>>(arena, gltf.meshes.size());




    std::span<ID::TextureID> createdTextureIDs = MemoryArena::AllocSpan<ID::TextureID>(arena, gltf.textures.size());
    for (int i = 0; i < gltf.textures.size(); i++)
    {
        createdTextureIDs[i] = assetManager.AddTexture(gltf.textures[i]);
    }
    std::span<ID::MaterialID> createdMaterialIDs = MemoryArena::AllocSpan<ID::MaterialID>(arena, gltf.materials.size());
    for (int i = 0; i < gltf.materials.size(); i++)
    {
        auto mat = gltf.materials[i];
        textureSetIDs textures = {
            mat.diffIndex >= 0 ? createdTextureIDs[mat.diffIndex] : defaults.defaultTexture,
            mat.specIndex >= 0 ? createdTextureIDs[mat.specIndex] : defaults.defaultSPec,
            mat.normIndex >= 0 ? createdTextureIDs[mat.normIndex] : defaults.defaultNormal
        };
        createdMaterialIDs[i] = assetManager.AddMaterial(mat.roughnessFactor, mat.metallicFactor, mat.baseColorFactor,
                                                   textures, 1);
    }

    for (int i = 0; i < gltf.meshes.size(); i++)
    {
        perSubmeshMeshHandles[i] = MemoryArena::AllocSpan<ID::MeshID>(arena, gltf.meshes[i].submeshes.size());
        perSubmeshMaterialHandles[i] = MemoryArena::AllocSpan<ID::MaterialID>(arena, gltf.meshes[i].submeshes.size());
        for (int j = 0; j < gltf.meshes[i].submeshes.size(); j++)
        {
            perSubmeshMeshHandles[i][j] = assetManager.AddBackingMesh(gltf.meshes[i].submeshes[j]);
            perSubmeshMaterialHandles[i][j] =  createdMaterialIDs[gltf.meshes[i].materialIndices[j]];
        }
    }

    //< Tform
    std::span<localTransform*> objectTransformReferences = MemoryArena::AllocSpan<localTransform*>(arena, gltf.objects.size());
    std::span<size_t> parentTransformIndex = MemoryArena::AllocSpanEmplaceInitialize<size_t>(arena, gltf.objects.size(), SIZE_MAX);
    /// />
    //prepass to set up parents
    for (int i = 0; i < gltf.objects.size(); i++)
    {
        for (int j = 0; j < gltf.objects[i].children.size(); j++)
        {
            parentTransformIndex[gltf.objects[i].children[j]] = i;
        }
    }
        for (int obj = 0; obj < gltf.objects.size(); obj++)
        {
            auto transformData = gltf.objects[obj]; //todo js: return list of created parent objects?

            auto mesh =gltf.objects[obj].meshidx;
            size_t TODO_J = 0;
            localTransform* parentTransformReference = (parentTransformIndex[obj] != SIZE_MAX) ? objectTransformReferences[parentTransformIndex[obj]] : nullptr;
            size_t objectID = scene.AddObject(
                perSubmeshMeshHandles[mesh],
                perSubmeshMaterialHandles[mesh],
                transformData.translation,
                transformData.rotation,
                transformData.scale,
                parentTransformReference);
            objectTransformReferences[obj] = &scene.transforms.transformNodes[objectID];
            //TODO setting parent transform(s) is possibly not working right -- could be failing to offset for other objects
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
