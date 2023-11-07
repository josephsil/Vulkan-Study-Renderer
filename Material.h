#pragma once

struct TextureData;

struct Material
{
public:
    int backingTextureidx;
    bool metallic;
    float roughness;
};
