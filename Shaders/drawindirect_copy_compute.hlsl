#include "structs.hlsl"
#include "culling_includes.hlsl"
#include "ObjectDataMacros.hlsl"
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
    uint passIndex;
};
[[vk::push_constant]]
PushConstants PC;

#define InstanceIndex drawData[PC.drawOffset + GlobalInvocationID.x].firstInstance
[numthreads(COPY_WORKGROUP_X, 1, 1)]
void Main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
	if (GlobalInvocationID.x >= PC.objectCount) return;
	uint32_t offsetCounterForPassIdx = (GetPassOffsetIndex(PC.passIndex)); //17-34 -- these are the offsets for the NEXT index. 
	
	bool visible = EarlyDrawList[PC.drawOffset + InstanceIndex] == 1;
	if (visible)
	{
		drawData[PC.drawOffset + GlobalInvocationID.x].cull = 1;
		uint shaderBucketIndex = GetPassSubpassIndex(PC.passIndex, SHADERINDEX);
		InterlockedAdd(drawIndices[shaderBucketIndex], 1);

		uint32_t globalOffset;
		uint32_t forPassOffset;
		//Update the counters s
		InterlockedAdd(drawIndices[GetGlobalDrawCountIndex()], 1, globalOffset);
	}
	else 
	{

		drawData[PC.drawOffset + GlobalInvocationID.x].cull = 0;
	}

	//Update the offset -- TODO JS, I'm sure this is very slow, I should just build this buffer after the compute	
	uint32_t _discard;
	InterlockedMax(drawIndices[offsetCounterForPassIdx],drawIndices[0], _discard);
}