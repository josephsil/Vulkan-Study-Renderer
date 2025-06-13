#pragma once
#include "Renderer/rendererGlobals.h"
#include <General/GLM_impl.h>

#include "Renderer/RendererSharedTypes.h"


struct DebugLineData;
struct GPU_perShadowData;

namespace LightAndCameraHelpers
{
    
 Transform getCameraTransform(cameraData camera);

 glm::vec4 maxComponentsFromSpan(std::span<glm::vec4> input);


 glm::vec4 minComponentsFromSpan(std::span<glm::vec4> input);


std::span<glm::vec4> FillFrustumCornersForSpace(std::span<glm::vec4> output_span, glm::mat4 matrix);


 viewProj CalcViewProjFromCamera( cameraData camera);

 std::span<GPU_perShadowData> CalculateLightMatrix(MemoryArena::Allocator* allocator, cameraData cam, glm::vec3 lightPos, glm::vec3 spotDir, float spotRadius, LightType type, DebugLineData* debugLinesManager);
};
