#pragma once
#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <span>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Renderer/gpu-data-structs.h"

struct DebugLineData
{
    struct linePair
    {
        glm::vec3 start;
        glm::vec3 end;
        glm::vec3 color;
    };

    std::vector<linePair> debugLines;

    void AddDebugFrustum(std::span<glm::vec4> frustum);
    void addDebugCross(glm::vec3 point, float size, glm::vec3 color);
    debugLinePConstants getDebugLineForRendering(size_t i);
};
