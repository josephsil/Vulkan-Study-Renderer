#include "structs.hlsl"
struct cullComputeGLobals
{
	float4x4 view;
	uint offset;
	uint frustumOffset;
	uint objectCount;
	//TODO JS: frustum should just go in here
};

struct drawCommandData_OLD
{
	uint objectIndex;
	// float4 debugData;
	// VkDrawIndirectCommand
	uint vertexCount;
	uint instanceCount;
	uint firstVertex;
	uint firstInstance;
};
struct drawCommandData
{
	uint objectIndex;
	uint    indexCount;
	uint    instanceCount;
	uint    firstIndex;
	int     vertexOffset;
	uint    firstInstance;
};

[[vk::push_constant]]
cullComputeGLobals globals;

[[vk::binding(0,0)]]
RWStructuredBuffer<float4> frustumData;

// #ifdef SHADOWPASS
[[vk::binding(1, 0)]]
RWStructuredBuffer<drawCommandData> drawData;
// #endif
// #ifdef SHADOWPASS
[[vk::binding(2, 0)]]
RWStructuredBuffer<objectData> _objectData;
// #endif



[numthreads(64, 1, 1)]
void Main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
	if (GlobalInvocationID.x >= globals.objectCount) return;
	uint objIndex = drawData[globals.offset + GlobalInvocationID.x].objectIndex;
	objectData object = _objectData[objIndex];
	float4x4 modelView = mul(globals.view, object.Model);
	// float4x4 mvp = mul(globals.proj, modelView);
	float4 center = mul(modelView,float4(0,0,0,1) + object.objectSpaceboundsCenter);
	// center.z = center.z * -1;
	float radius = object.objectSpaceboundsRadius; // TODO JS needs to be scaled! 
 
	bool visible = true;

	for(int i = 0 ; i < 6; i++)
	{
		visible = visible && dot(frustumData[i + globals.frustumOffset], float4(center.xyz,1)) > -(radius);
	}

	// visible = 0;
	drawData[globals.offset + GlobalInvocationID.x].instanceCount = visible ? 1 : 0;
	// drawData[GlobalInvocationID.x].debugData = center;
	// drawData[globals.offset + GlobalInvocationID.x].instanceCount = 0;
	
	
	// BufferTable[2].position = mul(float4(1,1,1,0), globals.projection) + d.indexCount;
	
}