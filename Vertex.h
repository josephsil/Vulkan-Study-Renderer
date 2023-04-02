#ifndef VERTEX_H
#define VERTEX_H
#pragma once

#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include "glm/gtx/hash.hpp"

#pragma region Forward Declarations 
struct VkVertexInputBindingDescription;
struct VkVertexInputAttributeDescription;
#pragma endregion 
struct Vertex
{
    glm::vec4 pos;
    glm::vec4 color;
    glm::vec4 texCoord;
    glm::vec4 normal;

    static VkVertexInputBindingDescription getBindingDescription();

    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions();

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

struct VertexHash {
    std::size_t operator()(const Vertex& v) const {
        std::size_t h1 = std::hash<glm::vec4>{}(v.pos);
        std::size_t h2 = std::hash<glm::vec4>{}(v.color);
        std::size_t h3 = std::hash<glm::vec4>{}(v.texCoord);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

#endif 