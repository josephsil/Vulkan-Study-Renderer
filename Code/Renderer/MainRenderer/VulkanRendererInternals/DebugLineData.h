
#include <span>
#include <vector>
#include <General/GLM_impl.h>

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
    GPU_DebugLinePushConstants getDebugLineForRendering(size_t i);
};
