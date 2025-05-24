#include "structs.hlsl"

struct cullComputeGLobals
{
    float4x4 view;
    float4x4 proj;
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
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

[[vk::binding(0, 0)]]
Texture2D<float4> bindless_textures[];
[[vk::binding(2, 0)]]
SamplerState bindless_samplers[];

[[vk::push_constant]]
cullComputeGLobals globals;

[[vk::binding(12,0)]]
RWStructuredBuffer<float4> frustumData;

// #ifdef SHADOWPASS
[[vk::binding(13, 0)]]
RWStructuredBuffer<drawCommandData> drawData;
// #endif
// #ifdef SHADOWPASS
[[vk::binding(14, 0)]]
RWStructuredBuffer<objectData> _objectData;
[[vk::binding(15, 0)]]
RWStructuredBuffer<transformdata> _transformdata;
// #endif


[numthreads(64, 1, 1)]
void Main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    if (GlobalInvocationID.x >= globals.objectCount) return;
    uint objIndex = drawData[globals.offset + GlobalInvocationID.x].objectIndex;
    transformdata transform = _transformdata[_objectData[objIndex].indexInfo.a];
    objectData mesh = _objectData[objIndex];
    float4x4 modelView = mul(globals.view, transform.Model);

    float4x4 viewProj = mul(globals.proj,
                               globals.view);
    float4 centerNDCSpace = mul(viewProj, float4(mesh.objectSpaceboundsCenter.xyz, 1.0));
    float3 centerProjection = (centerNDCSpace.xyz / centerNDCSpace.w);
    float3 centerUV = centerProjection * 0.5 + 0.5;

    // center.z = center.z * -1;
    float radius = mesh.objectSpaceboundsRadius * 1.5f;
    int size = 1024;
    int uvlevel = 2;
    
    // TODO JS Culling doesn't work properly for low FOVs -- scale up to be conservative
    float hiZValue = bindless_textures[0].SampleLevel(bindless_samplers[0], centerUV.xy, 0);
    bool visible = true;

    if (hiZValue <  centerProjection.z)
    {
        visible = false;
    }
    float4 centerClipSpaceForFrustum = mul(modelView, float4(0, 0, 0, 1) + mesh.objectSpaceboundsCenter);
    for (int i = 0; i < 6; i++)
    {
        // visible = visible && dot(frustumData[i + globals.frustumOffset], float4(centerClipSpaceForFrustum.xyz, 1)) > -(radius);
    }

    // visible = 0;
    drawData[globals.offset + GlobalInvocationID.x].instanceCount = visible ? 1 : 0;
    // drawData[GlobalInvocationID.x].debugData = center;
    // drawData[globals.offset + GlobalInvocationID.x].instanceCount = 0;


    // BufferTable[2].position = mul(float4(1,1,1,0), globals.projection) + d.indexCount;
}
