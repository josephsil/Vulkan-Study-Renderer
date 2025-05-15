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
    std::span<ID::MaterialID> createdMaterialIDs = MemoryArena::AllocSpan<ID::MaterialID>(arena, gltf.materials.size());

    //< Tform
    int addedCt = 0;
    std::span<localTransform*> transformPointers = MemoryArena::AllocSpan<localTransform*>(arena, gltf.objects.size());
    std::span<size_t> parentsPerObjects = MemoryArena::AllocSpanEmplaceInitialize<size_t>(arena, gltf.objects.size(), SIZE_MAX);
    /// />


    for (int i = 0; i < gltf.textures.size(); i++)
    {
        createdTextureIDs[i] = assetManager.AddTexture(gltf.textures[i]);
    }
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

            //The resulting span is the renderer material index, suitable to get passed to create object
            perSubmeshMaterialHandles[i][j] =   (uint32_t)createdMaterialIDs[gltf.meshes[i].materialIndices[j]];
        }
    }

    
    //prepass to set up parents
    for (int i = 0; i < gltf.objects.size(); i++)
    {
        for (int j = 0; j < gltf.objects[i].children.size(); j++)
        {
            parentsPerObjects[gltf.objects[i].children[j]] = i;
        }
    }

 
    
    //lol

        for (int i = 0; i < gltf.objects.size(); i++)
        {
            auto object = gltf.objects[i]; //todo js: return list of created parent objects?

            auto mesh = gltf.meshes[gltf.objects[i].meshidx];
            size_t TODO_J = 0;
            localTransform* parentTransform = (parentsPerObjects[i] != -1) ? transformPointers[parentsPerObjects[i]] : nullptr;
            size_t objectID = scene.AddObject(
                perSubmeshMeshHandles[i],
                perSubmeshMaterialHandles[i],
                object.translation,
                object.rotation,
                object.scale,
                parentTransform);
            transformPointers[i] = &scene.transforms.transformNodes[objectID];
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
