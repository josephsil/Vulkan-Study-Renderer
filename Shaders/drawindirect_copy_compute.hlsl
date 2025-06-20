#include "structs.hlsl"
[[vk::binding(13, 0)]]
RWStructuredBuffer<cullData> drawData;
[[vk::binding(14, 0)]]
RWStructuredBuffer<ObjectData> PerObjectData; 

[[vk::binding(16, 0)]]
RWStructuredBuffer<uint> EarlyDrawList; //Index in with objIndex
[[vk::binding(17, 0)]]
RWStructuredBuffer<uint> drawIndices; //Draw remap table -- in progress -- not needed here?

[[vk::binding(18, 0)]]
RWStructuredBuffer<drawCommandData> compactDrawData; 

struct  PushConstants
{
    uint drawOffset;
    uint objectCount;
    // uint disable;
};
[[vk::push_constant]]
PushConstants PC;

#define InstanceIndex drawData[PC.drawOffset + GlobalInvocationID.x].firstInstance
[numthreads(COPY_WORKGROUP_X, 1, 1)]
void Main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    if (GlobalInvocationID.x >= PC.objectCount) return;
    drawData[PC.drawOffset + GlobalInvocationID.x].cull = EarlyDrawList[PC.drawOffset + InstanceIndex] ? 1 : 0;
}