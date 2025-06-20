#include "structs.hlsl"
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
	uint32_t counterIdx = PC.passIndex +1;  //1-16 
											
	uint32_t offsetCounterForPassIdx = (PC.passIndex +1 + MAX_RENDER_PASSES); //17-34 -- these are the offsets for the NEXT index. 
		
	bool visible = drawData[PC.drawOffset + GlobalInvocationID.x].cull == 1;
	if (visible)
	{
		uint32_t globalOffset;
	uint32_t forPassOffset;
	// the first MAX_RENDER_PASSES + 1entries in drawIndices are counters
	//The first index is a global counter 
	//The rest are per pass counters 
	//The NEXT MAX_RENDER_PASSES entries are offsets, written using the counters

	//Update the counters s
	InterlockedAdd(drawIndices[0], 1, globalOffset);
	InterlockedAdd(drawIndices[counterIdx], 1, forPassOffset);

	cullData cullData = drawData[PC.drawOffset + GlobalInvocationID.x];
	
	//compact draws to the front
	compactDrawData[globalOffset].instanceCount = 1;
	meshletData _meshlet = _meshletData[cullData.objectIndex];
	compactDrawData[globalOffset].objectIndex = cullData.objectIndex;
	compactDrawData[globalOffset].firstInstance = cullData.firstInstance;
    compactDrawData[globalOffset].indexCount = _meshlet.meshletIndexCount;
    compactDrawData[globalOffset].firstIndex = _meshlet.meshletIndexOffset;
	compactDrawData[globalOffset].vertexOffset = _meshlet.meshletVertexOffset;
	}

	//Update the offset -- TODO JS, I'm sure this is very slow, I should just build this buffer after the compute	
	uint32_t _discard;
	InterlockedMax(drawIndices[offsetCounterForPassIdx],drawIndices[0], _discard);

}