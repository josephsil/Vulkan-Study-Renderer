#include "DebugLineData.h"

#include <span>
#include "Renderer/gpu-data-structs.h"

struct linePair
{
    glm::vec3 start;
    glm::vec3 end;
    glm::vec3 color;
};

std::vector<linePair> debugLines;

void DebugLineData::AddDebugFrustum(std::span<glm::vec4> frustum)
{
    //Back plane
    debugLines.push_back({{frustum[0]}, {frustum[1]}, {0, 0.5, 0}});
    debugLines.push_back({{frustum[1]}, {frustum[2]}, {0, 0.5, 0}});
    debugLines.push_back({{frustum[2]}, {frustum[3]}, {0, 0.5, 0}});
    debugLines.push_back({{frustum[3]}, {frustum[0]}, {0, 0.5, 0}});

    //front plane 
    debugLines.push_back({{frustum[4 + 0]}, {frustum[4 + 1]}, {0.5, 0, 0}});
    debugLines.push_back({{frustum[4 + 1]}, {frustum[4 + 2]}, {0.5, 0, 0}});
    debugLines.push_back({{frustum[4 + 2]}, {frustum[4 + 3]}, {0.5, 0, 0}});
    debugLines.push_back({{frustum[4 + 3]}, {frustum[4 + 0]}, {0.5, 0, 0}});

    //sides 
    debugLines.push_back({{frustum[0]}, {frustum[4 + 0]}, {1, 1, 1}});
    debugLines.push_back({{frustum[1]}, {frustum[4 + 1]}, {1, 1, 1}});
    debugLines.push_back({{frustum[2]}, {frustum[4 + 2]}, {1, 1, 1}});
    debugLines.push_back({{frustum[3]}, {frustum[4 + 3]}, {1, 1, 1}});
}

void DebugLineData::addDebugCross(glm::vec3 point, float size, glm::vec3 color)
{
    glm::vec3 xoffset = (glm::vec3{1, 0, 0} * (size / 2));
    glm::vec3 yoffset = (glm::vec3{0, 1, 0} * (size / 2));
    glm::vec3 zoffset = (glm::vec3{0, 0, 1} * (size / 2));
    debugLines.push_back({point - xoffset, point + xoffset, color});
    debugLines.push_back({point - yoffset, point + yoffset, color});
    debugLines.push_back({point - zoffset, point + zoffset, color});
}


debugLinePConstants DebugLineData::getDebugLineForRendering(size_t i)
{
    return {
        .m = glm::mat4(1.0),
        .pos1 = glm::vec4(debugLines[i].start, 1.0),
        .pos2 = glm::vec4(debugLines[i].end, 1.0),
        .color = glm::vec4(debugLines[i].color, 1.0)
    };
}
