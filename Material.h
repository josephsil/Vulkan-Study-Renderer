#pragma once

struct Material
{
public:

    uint32_t pipelineidx;
    int backingTextureidx;
    bool metallic;
    float roughness;
};
