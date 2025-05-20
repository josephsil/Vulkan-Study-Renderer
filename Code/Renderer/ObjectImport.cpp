#include <Renderer/ObjectImport.h>

#include <General/MemoryArena.h>
#include <Scene/AssetManager.h>
#include <Scene/Transforms.h>

#include "MeshCreation/meshData.h"
#include "Scene/Scene.h"


struct AddObjectResult
{
    
};
//TODO JS: Unify this path and the other mesh import path -- everything should go thru here 
void ObjectImport::CreateObjectAssets(ArenaAllocator &arena, Scene &scene, AssetManager &assetManager, ImportedObjectData &gltf, DefaultTextures defaults)
{
    size_t totalSubmeshct = 0;
    for (auto& gltfmesh : gltf.meshes)
    {
        totalSubmeshct+= gltfmesh.submeshes.size();
        
    }
    auto subMeshHandles = MemoryArena::AllocSpan<ID::SubMeshID>(arena, totalSubmeshct);
    auto perSubmeshMaterialHandles = MemoryArena::AllocSpan<ID::MaterialID>(arena, totalSubmeshct);

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

    size_t submeshIndex =0;
    for (int i = 0; i < gltf.meshes.size(); i++)
    {
        //TODO JS MESHLET PERF: I now create a material *per meshlet*, which is way too many
        for (int j = 0; j < gltf.meshes[i].submeshes.size(); j++)
        {
            for (auto& meshlet : gltf.meshes[i].submeshes[j])
            {
                assert( meshlet.vertices.size() <= MESHLET_VERTICES);
                assert( meshlet.indices.size() <= (MESHLET_TRIS * 3));
            }
            subMeshHandles[submeshIndex] = assetManager.AddMesh(gltf.meshes[i].submeshes[j]);
          
            //TODO JS MESHLET PERF: I now create a material *per meshlet*, which is way too many
            perSubmeshMaterialHandles[submeshIndex] =  createdMaterialIDs[gltf.meshes[i].submeshMaterialIndices[j]];
            submeshIndex++;
        }
    }

    //< Tform
    std::span<localTransform*> objectTransformReferences = MemoryArena::AllocSpan<localTransform*>(arena, gltf.objects.size()); //TODO JS: move to contiguous memory somewhere
    std::span<size_t> parentTransformIndex = MemoryArena::AllocSpanEmplaceInitialize<size_t>(arena, gltf.objects.size(), SIZE_MAX); //TODO JS: move to contiguous memory somewhere
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
            localTransform* parentTransformReference = (parentTransformIndex[obj] != SIZE_MAX) ? objectTransformReferences[parentTransformIndex[obj]] : nullptr;
            size_t objectID = scene.AddObject(
                subMeshHandles,
                perSubmeshMaterialHandles,
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
