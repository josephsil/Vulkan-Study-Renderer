#include "structs.hlsl"
[[vk::binding(13, 0)]]
RWStructuredBuffer<drawCommandData> drawData;
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
	uint passIndex;
    // uint disable;
};
[[vk::push_constant]]
PushConstants PC;

#define InstanceIndex drawData[PC.drawOffset + GlobalInvocationID.x].objectIndex
[numthreads(COPY_WORKGROUP_X, 1, 1)]
void Main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    if (GlobalInvocationID.x >= PC.objectCount) return;

	uint32_t globalIndex;
	uint32_t forPassIndex;
	// the first MAX_RENDER_PASSES + 1entries in drawIndices are counters
	//The first index is a global counter 
	//The rest are per pass counters 
	//The NEXT MAX_RENDER_PASSES entries are offsets, written using the counters
	uint32_t counterIdx = PC.passIndex +1; 
	uint32_t offsetCounterForPassIdx = counterIdx + MAX_RENDER_PASSES;
		
	//Update the counters s
	InterlockedAdd(drawIndices[0], 1, globalIndex);
	InterlockedAdd(drawIndices[counterIdx], 1, forPassIndex);

	uint32_t _discard;
	//Update the offset 
	InterlockedMax(drawIndices[offsetCounterForPassIdx], globalIndex +1, _discard);
	compactDrawData[globalIndex] = drawData[PC.drawOffset + GlobalInvocationID.x]; //compact draws to the front
}