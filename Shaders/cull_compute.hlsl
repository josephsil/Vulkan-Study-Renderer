struct cullComputeGLobals
{
	float4x4 viewProjection;
	uint frustumOffset;
};

struct drawdata
{
	// VkDrawIndirectCommand
	uint vertexCount;
	uint instanceCount;
	uint firstVertex;
	uint firstInstance;
};
struct objectData
{
	float4 boundsCenter;
	float boundsRadius;
};

[[vk::push_constant]]
cullComputeGLobals globals;

[[vk::binding(1,0)]]
RWStructuredBuffer<float3> frustumData;

// #ifdef SHADOWPASS
[[vk::binding(2, 0)]]
RWStructuredBuffer<drawdata> drawData;
// #endif
// #ifdef SHADOWPASS
[[vk::binding(3, 0)]]
RWStructuredBuffer<objectData> objectData;
// #endif



[numthreads(16, 1, 1)]
void Main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
	
	// BufferTable[2].position = mul(float4(1,1,1,0), globals.projection) + d.indexCount;
	
}