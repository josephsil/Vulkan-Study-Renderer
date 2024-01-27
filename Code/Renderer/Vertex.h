#ifndef VERTEX_H
#define VERTEX_H
#pragma once

#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>


#pragma region Forward Declarations
#include "VulkanIncludes/forward-declarations-renderer.h"
#pragma endregion
struct Vertex
{
    glm::vec4 pos;
    glm::vec4 color;
    glm::vec4 texCoord;
    glm::vec4 normal;
    glm::vec4 tangent;

    static VkVertexInputBindingDescription getBindingDescription();

    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions();

    bool operator==(const Vertex& other) const
    {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

struct VertexHash
{
    std::size_t operator()(const Vertex& v) const;
};

#endif
