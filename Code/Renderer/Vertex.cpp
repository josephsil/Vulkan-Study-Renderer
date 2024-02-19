#include "Vertex.h"
#include <array>
#include "VulkanIncludes/Vulkan_Includes.h"
#include "glm/gtx/hash.hpp"


std::size_t VertexHash::operator()(const Vertex& v) const
{
    std::size_t h1 = std::hash<glm::vec4>{}(v.pos);
    std::size_t h2 = std::hash<glm::vec4>{}(v.color);
    std::size_t h3 = std::hash<glm::vec4>{}(v.texCoord);
    return h1 ^ (h2 << 1) ^ (h3 << 2);
}
