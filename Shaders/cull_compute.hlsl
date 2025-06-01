#include "structs.hlsl"
#include "GeneralIncludes.hlsl"
#include "ObjectDataMacros.hlsl"
struct drawCommandData
{

    float4 debug1;
    float4 debug2;
    float4 debug3;
    uint objectIndex;
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

[[vk::binding(0, 0)]]
Texture2D<float> bindless_textures;
[[vk::binding(2, 0)]]
SamplerState bindless_samplers;

[[vk::push_constant]]
CullPushConstants cullPC;

[[vk::binding(12,0)]]
RWStructuredBuffer<float4> frustumData;

[[vk::binding(13, 0)]]
RWStructuredBuffer<drawCommandData> drawData;
// [[vk::binding(13, 0)]]
// RWStructuredBuffer<drawCommandData> lastFrameDrawData; //TODO JS: Should I just offset into one big buffer of all frames?
[[vk::binding(14, 0)]]
RWStructuredBuffer<ObjectData> PerObjectData;
[[vk::binding(15, 0)]]
RWStructuredBuffer<Transform> Transforms;


// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
bool projectSphere(float3 c, float r, float znear, float P00, float P11, out float4 aabb)
{
    //TODO JS: I think I made a mistake converting this re: cr.z -- since I only replace c.z with positive_z, the rest is wrong. This may explain the other issues.
    c.z *= -1;
    if (c.z - znear< r )
        return false;

    float3 cr = c * r;
    float czr2 = c.z  * c.z  - r * r;

    float vx = sqrt(c.x * c.x + czr2);
    float minx = (vx * c.x - cr.z) / (vx * c.z  + cr.x);
    float maxx = (vx * c.x + cr.z) / (vx * c.z  - cr.x);

    float vy = sqrt(c.y * c.y + czr2);
    float miny = (vy * c.y - cr.z) / (vy * c.z  + cr.y);
    float maxy = (vy * c.y + cr.z) / (vy * c.z  - cr.y);

    aabb = float4(minx * P00, miny * P11, maxx * P00, maxy * P11);
    aabb = aabb.zwxy * float4(0.5f, 0.5f, 0.5f, 0.5f) + float4(0.5f,0.5f,0.5f,0.5f); // clip space -> uv space

    return true;
}


Bounds GetWorldSpaceBounds(float3 center, Bounds inB)
{
    float4 worldmin = float4(center.xyz + inB.min.xyz, 1.0);
    float4 worldMax = float4(center.xyz + inB.max.xyz, 1.0);
    
    Bounds b;
    b.max = worldMax;
    b.min = worldmin;
    return b;
}


#define ShaderGlobals cullPC //To make macros work 
#define InstanceIndex drawData[ShaderGlobals.offset + GlobalInvocationID.x].objectIndex //To make macros work
[numthreads(64, 1, 1)]
void Main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    if (GlobalInvocationID.x >= cullPC.objectCount) return;
    uint objIndex = InstanceIndex;
    Transform transform = GetTransform();
    ObjectData mesh = PerObjectData[objIndex];
    float4 objectCenter = mesh.boundsSphere.center;
    float4 worldCenter = ObjectToWorld(objectCenter);
    float4 ViewCenter = ObjectToView(objectCenter);
    float4 centerClipSpace = ObjectToClip(objectCenter);
    float3 centerNDC = ClipToNDC(centerClipSpace);
    float2 centerUV = NDCToUV(centerNDC);
    float centerDepth = NDCToDepth(centerNDC);

    bool visible = true;
    bool test = visible;

    //frustum
    if (test)
    {
    // TODO JS Culling doesn't work properly for low FOVs -- scale up to be conservative
        float radius = mesh.boundsSphere.radius * mesh.objectScale * 1.5f;
        float4 centerViewSpace = ObjectToView(objectCenter);
        for (int i = 0; i < 6; i++)
        {
            visible = visible && dot(frustumData[i + cullPC.frustumOffset], float4(centerViewSpace.xyz, 1)) > -(radius);
        }
    }
    
    //Occlusion
    if (test && visible)
    {
        //todo js test 
        float scaledRadius = (mesh.boundsSphere.radius) * mesh.objectScale;
        scaledRadius *= 0.5; // temp debugging
        //
        //
        //
        float4 aabb;
        if (projectSphere(ViewCenter, scaledRadius, 0.001f,  ShaderGlobals.projMatrix[0][0], ShaderGlobals.projMatrix[1][1], aabb))
        {
            float width = ((aabb.x - aabb.z) ) *  1024.0;
            float height = ((aabb.w - aabb.y )) *  1024.0;

            int level = ceil(log2(max(width, height)));
            float2 hiZSamplePoint1 =  (aabb.xy);
            float2 hiZSamplePoint2 =  (aabb.zw);
            float2 hiZSamplePoint3 =  (aabb.xw);
            float2 hiZSamplePoint4 =  (aabb.zy); 

            float2 uv =  (aabb.xy + aabb.zw) / 2.f;
            float positive_z = ViewCenter.z * -1;

            float hiZValue = bindless_textures.SampleLevel(bindless_samplers, float3(uv.x, uv.y,(int)0), level);
            
            float depthSphere =  (0.001f /( positive_z - scaledRadius));

            drawData[ShaderGlobals.offset + GlobalInvocationID.x].debug1.xy = (aabb.xy + aabb.zw) / 2.f; 
            drawData[ShaderGlobals.offset + GlobalInvocationID.x].debug1.zw = aabb.xy;
            drawData[ShaderGlobals.offset + GlobalInvocationID.x].debug2.xy =  aabb.zw;
            drawData[ShaderGlobals.offset + GlobalInvocationID.x].debug2.z = hiZValue;
            drawData[ShaderGlobals.offset + GlobalInvocationID.x].debug2.w = depthSphere; //

            drawData[ShaderGlobals.offset + GlobalInvocationID.x].debug3.x = level;
            drawData[ShaderGlobals.offset + GlobalInvocationID.x].debug3.y = hiZValue >  (depthSphere);
            if (hiZValue >  (depthSphere) && !cullPC.disable)
            {
                
                drawData[ShaderGlobals.offset + GlobalInvocationID.x].instanceCount = visible ? 0 : 0;
                return;
            }
        }
    }


    drawData[ShaderGlobals.offset + GlobalInvocationID.x].instanceCount = visible ? 1 : 0;
    // drawData[GlobalInvocationID.x].debugData = center;
    // drawData[cullPC.offset + GlobalInvocationID.x].instanceCount = 0;
    // BufferTable[2].position = mul(float4(1,1,1,0), cullPC.projection) + d.indexCount;
}
