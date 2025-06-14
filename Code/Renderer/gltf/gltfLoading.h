#pragma once
#include <span>
#include <General/GLM_impl.h>

#include <Renderer/ObjectImport.h>
#include <Renderer/TextureCreation/TextureData.h>
#include <Renderer/MeshCreation/MeshData.h>
struct PerThreadRenderContext;
struct InterchangeMesh;
struct TextureMetaData;

//This is an "asset loading" step, might get separated from the main runtime at some point.
//Wraps tinygltf, does gltf mesh/texture import,
//Uploads data to assetmanager.
//The 'gltfdata' result is the data required for ObjectImport::CreateObjectAssetsAndAddToScene
using gltfNode = ObjectImport::MeshObject;
using gltfMaterial = ObjectImport::Material ;
using GltfMesh = ObjectImport::Mesh ;
using GltfData = ObjectImport::ImportedObjectData ;
GltfData GltfLoadMeshes(PerThreadRenderContext handles,   AssetManager& rendererData, const char* gltfpath);
