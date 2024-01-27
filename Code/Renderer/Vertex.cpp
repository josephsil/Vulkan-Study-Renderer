#include "Vertex.h"
#include <array>
#include "VulkanIncludes/Vulkan_Includes.h"
#include "glm/gtx/hash.hpp"

//TODO JS: this sucks -- it shoudlnt return binding descs i should return raw data?
VkVertexInputBindingDescription Vertex::getBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;


    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 4> Vertex::getAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);


    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, normal);


    return attributeDescriptions;
}

std::size_t VertexHash::operator()(const Vertex& v) const
{
    std::size_t h1 = std::hash<glm::vec4>{}(v.pos);
    std::size_t h2 = std::hash<glm::vec4>{}(v.color);
    std::size_t h3 = std::hash<glm::vec4>{}(v.texCoord);
    return h1 ^ (h2 << 1) ^ (h3 << 2);
}
