#include "structs.hlsl"
[[vk::binding(13, 0)]]
RWStructuredBuffer<drawCommandData> drawData;
[[vk::binding(14, 0)]]
RWStructuredBuffer<ObjectData> PerObjectData;
[[vk::binding(16, 0)]]
RWStructuredBuffer<uint> EarlyDrawList; //Index in with objIndex

struct  PushConstants
{
    uint drawOffset;
    uint objectCount;
    // uint disable;
};
[[vk::push_constant]]
PushConstants PC;

#define InstanceIndex drawData[PC.drawOffset + GlobalInvocationID.x].objectIndex
[numthreads(COPY_WORKGROUP_X, 1, 1)]
void Main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    if (GlobalInvocationID.x >= PC.objectCount) return;
        drawData[PC.drawOffset + GlobalInvocationID.x].instanceCount = EarlyDrawList[PC.drawOffset + InstanceIndex] ? 1 : 0;
}