#pragma once
#include <span>
#include <General/GLM_IMPL.h>

#include "Renderer/ObjectImport.h"
#include "Renderer/TextureCreation/TextureData.h"


struct PerThreadRenderContext;
struct preMeshletMesh;
struct TextureMetaData;


using gltfNode = ObjectImport::MeshObject;
using gltfMaterial = ObjectImport::Material ;

using GltfMesh = ObjectImport::Mesh ;

using GltfData = ObjectImport::ImportedObjectData ;

GltfData GltfLoadMeshes(PerThreadRenderContext handles,   AssetManager& rendererData, const char* gltfpath);
