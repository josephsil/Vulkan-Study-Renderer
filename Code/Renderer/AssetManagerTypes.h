#pragma once
namespace ID
{
	//TODO: These should eventualy be some kind of opaque handles the assetmanager knows how to use
    using SubMeshID = uint32_t;
    using MaterialID = uint32_t;
    using TextureID = uint32_t;
    using SubMeshGroupID = uint8_t;
};

struct textureSetIDs
{
    ID::TextureID diffuseIndex;
    ID::TextureID specIndex;
    ID::TextureID normIndex;
};

struct MeshletData
{
    size_t meshletVertexOffset; //Offset for this meshlet's verts within the global vertex buffer
    size_t meshletIndexOffset;//Offset for this meshlet within the global index buffer
    size_t meshletIndexCount;//Index count for this meshlet
};
struct PerSubmeshData
{
    uint32_t meshletCt;
    size_t firstMeshletIndex; //For indexing into global arrays
};

