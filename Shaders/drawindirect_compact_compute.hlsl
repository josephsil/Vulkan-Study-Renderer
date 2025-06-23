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
RWStructuredBuffer<uint> drawIndices; 

[[vk::binding(18, 0)]]
RWStructuredBuffer<drawCommandData> compactDrawData; 
[[vk::binding(19, 0)]]
RWStructuredBuffer<meshletData> _meshletData; 

struct  PushConstants
{
    uint drawOffset;
    uint objectCount;
	uint passIndex;
    // uint disable;
};
[[vk::push_constant]]
PushConstants PC;

#define InstanceIndex drawData[PC.drawOffset + GlobalInvocationID.x].firstInstance
[numthreads(COPY_WORKGROUP_X, 1, 1)]
void Main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    if (GlobalInvocationID.x >= PC.objectCount) return;
	uint32_t counterIdx = GetPassCountIndex(PC.passIndex);  //1-16 
											
	uint32_t offsetCounterForPassIdx = (GetPassOffsetIndex(PC.passIndex)); //17-34 -- these are the offsets for the NEXT index. 
		
	bool visible = drawData[PC.drawOffset + GlobalInvocationID.x].draw == 1;
	uint shaderBucketCounterIndex = GetPassSubpassCounterIndex(PC.passIndex, SHADERINDEX);

	cullData uncompactedDraw = drawData[PC.drawOffset + GlobalInvocationID.x];
	if (visible)
	{
		uint32_t globalOffset;
		uint32_t withinSubPassOffset;

		//Counter for draws in this shader bucket 
		InterlockedAdd(drawIndices[shaderBucketCounterIndex], 1, withinSubPassOffset);

		cullData cullData =  drawData[PC.drawOffset + GlobalInvocationID.x];
	
		//TODO: Right now the offset counters are "the offset after me is X", so I have to do this ternary -1 thing
		//TODO: Want to update and just index in to +1 in most places
		uint passOffset =counterIdx == 0 ? 0 : drawIndices[offsetCounterForPassIdx -1];
		uint shader = SHADERINDEX;
		uint subPassOffset =  shader == 0? 0 : drawIndices[GetPassSubpassIndex(PC.passIndex, shader-1)];


		meshletData _meshlet = _meshletData[cullData.objectIndex];
		compactDrawData[subPassOffset + passOffset + withinSubPassOffset].instanceCount = 1;
		compactDrawData[subPassOffset + passOffset + withinSubPassOffset].objectIndex = cullData.objectIndex;
		compactDrawData[subPassOffset + passOffset + withinSubPassOffset].firstInstance = cullData.firstInstance;
		compactDrawData[subPassOffset + passOffset + withinSubPassOffset].indexCount = _meshlet.meshletIndexCount;
		compactDrawData[subPassOffset + passOffset + withinSubPassOffset].firstIndex = _meshlet.meshletIndexOffset;
		compactDrawData[subPassOffset + passOffset + withinSubPassOffset].vertexOffset = _meshlet.meshletVertexOffset;
	}

}