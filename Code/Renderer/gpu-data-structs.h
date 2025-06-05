#pragma once
#include "VulkanIncludes/structs_hlsl.h"
#include "VulkanIncludes/forward-declarations-renderer.h"
#include <General/GLM_IMPL.h>
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
    glm::vec4 debug1;
    glm::vec4 debug2;
    glm::vec4 debug3;
    uint32_t objectIndex;
    __VkDrawINDEXEDIndirectCommand command;
};
