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

struct PerSubmeshData
{
    uint32_t meshletCt;
    size_t firstMeshletIndex; //For indexing into global arrays
};

