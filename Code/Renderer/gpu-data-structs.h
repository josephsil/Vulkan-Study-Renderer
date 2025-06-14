#pragma once
#include <Renderer/VulkanIncludes/structs_hlsl.h>
#include "VulkanIncludes/forward-declarations-renderer.h"
#include <General/GLM_impl.h>
#include <vector>

struct __VkDrawINDEXEDIndirectCommand
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
};

struct __VkDrawIndirectCommand
{
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
};

struct drawCommandData
{
    uint32_t objectIndex;
    __VkDrawINDEXEDIndirectCommand command;
};
