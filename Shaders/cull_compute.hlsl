#include "structs.hlsl"
#include "GeneralIncludes.hlsl"
#include "ObjectDataMacros.hlsl"
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
    if (c.z < r + znear)
        return false;

    float3 cr = c * r;
    float czr2 = c.z * c.z - r * r;

    float vx = sqrt(c.x * c.x + czr2);
    float minx = (vx * c.x - cr.z) / (vx * c.z + cr.x);
    float maxx = (vx * c.x + cr.z) / (vx * c.z - cr.x);

    float vy = sqrt(c.y * c.y + czr2);
    float miny = (vy * c.y - cr.z) / (vy * c.z + cr.y);
    float maxy = (vy * c.y + cr.z) / (vy * c.z - cr.y);

    aabb = float4(minx * P00, miny * P11, maxx * P00, maxy * P11);
    aabb = aabb.xwzy * float4(0.5f, -0.5f, 0.5f, -0.5f) + float4(0.5f,0.5f,0.5f,0.5f); // clip space -> uv space

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
    float4 centerClipSpace = ObjectToClip(objectCenter);
    float3 centerNDC = ClipToNDC(centerClipSpace);
    float2 centerUV = NDCToUV(centerNDC);
    float centerDepth = NDCToDepth(centerNDC);
    //TODO JS: ok, I understand why other renderers have a near plane -- do I need to offset this by the near plane, or is that in my proj matrix?
    //      TODO JS: if it's IS in my matrix, it's still *cheaper* to do it like how the point light code does, right?
    
    // center.z = center.z * -1;

    int uvlevel = 2;
    // TODO JS Culling doesn't work properly for low FOVs -- scale up to be conservative
    //TODO JS: See comment below on CenterToViewSpace
    bool visible = true;
    bool test = visible;
    //PLANNED FINAL VERSION: In the early pass, I'm taking things which are NOT VISIBLE and checking them against my 'visible last frame' list.
    // //PLANNED FINAL VERSION: In order to do this, I need to read from LAST FRAME's buffer to see who is initially visible. TODO 
    // bool test = drawData[cullPC.offset + GlobalInvocationID.x].indexCount == 1; //Only test things which were previously not culled
    //
    // //PLANNED FINAL VERSION the late pass, I'm taking things which are VISIBLE and updating their culling?
    // if (!cullPC.LATE_CULL)
    // {
        // test = drawData[cullPC.offset + GlobalInvocationID.x].indexCount == 0; //Only test things which were previously culled
    // }

    // if (test)
    // {
    //     float radius = mesh.boundsSphere.radius * 1.5f;
    //     float4 centerViewSpace = ObjectToView(objectCenter);
    //     for (int i = 0; i < 6; i++)
    //     {
    //         visible = visible && dot(frustumData[i + cullPC.frustumOffset], float4(centerViewSpace.xyz, 1)) > -(radius);
    //     }
    // }
    if (test && visible)
    {
        // visibl
        Bounds objectSpaceBounds = mesh.bounds;
        float4 worldSpaceBoundsmax = ObjectToWorld(objectSpaceBounds.max);
        float4 worldSpaceBoundsmin = ObjectToWorld(objectSpaceBounds.min);//GetWorldSpaceBounds(worldCenter, mesh.bounds);
        float4 minBound = WorldToClip(worldSpaceBoundsmin);
        float4 maxBound = WorldToClip(worldSpaceBoundsmax);
        float3 minNDC = ClipToNDC(minBound);
        float3 maxNDC = ClipToNDC(maxBound);
        float2 minUV = NDCToUV(minNDC);
        float2 maxUV = NDCToUV(maxNDC);
        float clipSpaceRadius = distance(minBound, maxBound) * 0.5;
        float ndcSpaceRadius = distance(minNDC, maxNDC) * 0.5;

        float4 aabb;
        float nearPlane = 0.1f;
        float farPlane = 35.0f;
        // projectSphere(worldCenter, mesh.boundsSphere.radius, nearPlane, 0.89259, -1.42815, aabb);
        // minNDC.x = aabb.x;
        // minNDC.y = aabb.y;
        // maxNDC.x = aabb.z;
        // maxNDC.y = aabb.w;        

  
        float4 centerViewSpace = WorldToClip(objectCenter);
        float3 center2NDC = ClipToNDC(centerViewSpace);
        float worldSpaceRadius =  mul(MODEL_MATRIX, mesh.boundsSphere.radius);


        float width = (maxUV.x - minUV.x) * DEPTH_PYRAMID_SIZE;
        float height = (maxUV.y - minUV.y) * DEPTH_PYRAMID_SIZE;
        float squaremax = max(width, height);
        float level = floor(log2(squaremax));
        float hiZSamplePoint = (minUV + maxUV) * 0.5; //Center
        float3 radiusAsFloat3Temp = float3(mesh.boundsSphere.radius, mesh.boundsSphere.radius,mesh.boundsSphere.radius);
        // float _depthSphere =((centerViewSpace.z + worldSpaceRadius)) / centerClipSpace.w ;
        float hiZValue = bindless_textures[0].SampleLevel(bindless_samplers[0], hiZSamplePoint, level);
        float depthBoundingSphere = (centerNDC.z - ndcSpaceRadius)   ;
        bool culled =  hiZValue < depthBoundingSphere ;
        visible = visible &&  !culled;

            drawData[ShaderGlobals.offset + GlobalInvocationID.x].instanceCount = visible ? 1 : 0;
            if (culled)
            {
                drawData[ShaderGlobals.offset + GlobalInvocationID.x].instanceCount = 3;
                return;
        }
        //
        // if (hiZValue <  centerDepth)
        // {
        //     visible = false;
        // }
    }


    drawData[ShaderGlobals.offset + GlobalInvocationID.x].instanceCount = visible ? 1 : 0;
    // drawData[GlobalInvocationID.x].debugData = center;
    // drawData[cullPC.offset + GlobalInvocationID.x].instanceCount = 0;
    // BufferTable[2].position = mul(float4(1,1,1,0), cullPC.projection) + d.indexCount;
}
