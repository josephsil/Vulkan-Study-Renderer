#pragma once

#include <General/GLM_impl.h>

struct Vertex
{
    glm::vec4 pos;
    glm::vec4 color;
    glm::vec4 texCoord;
    glm::vec4 normal;
    glm::vec4 tangent;


    bool operator==(const Vertex& other) const
    {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

struct VertexHash
{
    std::size_t operator()(const Vertex& v) const;
};
