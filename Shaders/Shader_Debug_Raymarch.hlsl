#include "BindlessIncludes.hlsl"

struct VSInput
{
    [[vk::location(0)]] float3 Position : POSITION0;
    [[vk::location(1)]] float3 Color : COLOR0;
    [[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
    // [[vk::location(3)]] uint vertex_id : SV_VertexID;
};


struct FSOutput
{
    [[vk::location(0)]] float3 Color : SV_Target;
};


struct VSOutput
{
    [[vk::location(0)]] float4 Pos : SV_POSITION;
    [[vk::location(1)]] float3 Color : COLOR0;
    [[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
    [[vk::location(3)]] float3 Normal : NORMAL0;
    [[vk::location(4)]] float3 worldPos : TEXCOORD1;
    [[vk::location(5)]] float3 Tangent : TEXCOORD2;
    [[vk::location(6)]] float3 BiTangent : TEXCOORD3;
    [[vk::location(7)]] float3x3 TBN : TEXCOORD4;
    [[vk::location(10)]] uint InstanceID : SV_InstanceID;
};

static float2 fullscreen_quad_positions[6] = {
    float2(-1.0, -1.0),
    float2(1.0, -1.0),
    float2(-1.0, 1.0),
    float2(-1.0, 1.0),
    float2(1.0, -1.0),
    float2(1.0, 1.0)
};


#define MAX_STEPS 300
#define MAX_DIST 100.0
#define EPS .01
// -spirv -T vs_6_5 -E Vert .\Shader1.hlsl -Fo .\triangle.vert.spv


VSOutput Vert(VSInput input, [[vk::builtin("BaseInstance")]] uint InstanceIndex : BaseInstance,
              uint VertexIndex : SV_VertexID)
{
#ifdef USE_RW
    MyVertexStructure myVertex = BufferTable[VertexIndex];
#else
	//Interesting buffer load perf numbers
	// https://github.com/sebbbi/perfindexInfo
	// https://github.com/microsoft/DirectXShaderCompiler/issues/2193 	
 	MyVertexStructure myVertex = BufferTable.Load<MyVertexStructure>((VERTEXOFFSET + VertexIndex) * sizeof(MyVertexStructure));
#endif
    float4 vertPos = float4(fullscreen_quad_positions[VertexIndex], 1, 1.0);

    //
    objectData ubo = uboarr[InstanceIndex];
    VSOutput output = (VSOutput)0;
    //
    float4x4 modelView = mul(globals.view, uboarr[InstanceIndex].Model);
    float4x4 mvp = mul(globals.projection, modelView);
    output.Pos = vertPos;
    // output.Texture_ST = myVertex.uv0.xy;
    output.Color = vertPos;


    // output.Normal = myVertex.normal.xyz;
    // output.worldPos = mul(ubo.Model, vertPos);
    //
    // float3x3 normalMatrix = ubo.NormalMat; // ?????
    // //bitangent = fSign * cross(vN, tangent);
    // //Not sure if the mul here is correct? would need something baked
    // float3 worldNormal = normalize(mul(normalMatrix, float4(output.Normal.x, output.Normal.y, output.Normal.z, 0.0)));
    // float3 worldTangent = normalize(
    //     mul(ubo.Model, float4(myVertex.Tangent.x, myVertex.Tangent.y, myVertex.Tangent.z, 1.0)));
    // worldTangent = (worldTangent - dot(worldNormal, worldTangent) * worldNormal);
    // float3 worldBinormal = (cross((worldNormal), (worldTangent))) * (myVertex.Tangent.w);
    //
    // output.Tangent = worldTangent;
    // output.Normal = worldNormal.xyz;
    // output.BiTangent = worldBinormal;
    //
    //
    // output.TBN = transpose(float3x3((worldTangent), (worldBinormal), (output.Normal)));
    //
    // //   float3 normalW = normalize(float3(u_NormalMatrix * vec4(a_Normal.xyz, 0.0)));
    // //   float3 tangentW = normalize(float3(u_ModelMatrix * vec4(a_Tangent.xyz, 0.0)));
    // //   float3 bitangentW = cross(normalW, tangentW) * a_Tangent.w;
    // //   v_TBN = mat3(tangentW, bitangentW, normalW);
    //
    // output.InstanceID = InstanceIndex;
    //
    //
    // output.Color = ubo.color;
    return output;
}


struct FSInput
{
    //[[vk::location(0)]] float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 Pos : SV_POSITION;
    [[vk::location(1)]] float3 Color : COLOR0;
    [[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
    [[vk::location(3)]] float3 Normal : NORMAL0;
    [[vk::location(4)]] float3 worldPos : TEXCOORD1;
    [[vk::location(5)]] float3 Tangent : TEXCOORD2;
    [[vk::location(6)]] float3 BiTangent : TEXCOORD3;
    [[vk::location(7)]] float3x3 TBN : TEXCOORD4;
};


//
float3x3 calculateNormal(FSInput input)
{
    // float3 tangentNormal = normalMapTexture.Sample(normalMapSampler, input.UV).xyz * 2.0 - 1.0;

    float3 N = normalize(input.Normal);
    float3 T = normalize(input.Tangent);
    float3 B = normalize(cross(N, T));
    float3x3 TBN = transpose(float3x3(T, B, N));

    return TBN;
}

//
// -spirv -T ps_6_5 -E Frag .\Shader1.hlsl -Fo .\triangle.frag.spv
//TODO JS: copied and pasted, could diverge

const float WIDTH = 1280.0;
const float HEIGHT = 720.0;

float4x4 inverse(float4x4 m)
{
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 *
        n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 *
        n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 *
        n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 *
        n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0f / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 *
        n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 *
        n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 *
        n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 *
        n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 *
        n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 *
        n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 *
        n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 *
        n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 *
        n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 *
        n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 *
        n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 *
        n33) * idet;

    return ret;
}

FSOutput Frag(VSOutput input)
{
    FSOutput output; //

    float2 screenRatio = {1.0, 0.5625};

    float2 uv = ((input.Color));
    // uv *= screenRatio;
    // Time varying pixel color

    float _fov = 70.0;
    float aspect = 1.0 / 0.5625;
    float fov2 = radians(_fov) * 0.5f;
    float2 screenCoord = uv;
    screenCoord.x *= -aspect;
    float2 offsets = screenCoord * tan(fov2);
    float3 front = float3(0.0, 0.0, -1.0); // where are we looking at
    float3 up = float3(0.0, 1.0, 0.0); // what we consider to be up
    float3 rayFront = normalize(front);
    float3 rayRight = normalize(cross(normalize(up), rayFront));
    float3 rayUp = cross(rayRight, rayFront);
    float3 rayDir = rayFront + (rayRight * offsets.x) + (rayUp * offsets.y);


    float3 ray_origin = float3(0, 0, 0);
    float3 ray_dir = (normalize(rayDir));

    // output.Color = ray_dir;
    // return output;


    float d_out = 0.;
    int start = 0;
    for (int j = start; j < start + 10; j++)
    {
        uint InstanceIndex = j;
        objectData ubo = uboarr[InstanceIndex];
        float4x4 modelView = mul(globals.view, uboarr[InstanceIndex].Model);
        float4x4 mvp = mul(globals.projection, modelView);
        float d = 0.;
        for (int i = 0; i < MAX_STEPS; i++)
        {
            float3 _point = ray_origin + ray_dir * d;
            float4 sphere = float4(uboarr[InstanceIndex].objectSpaceboundsCenter.xyz, 1);
            sphere = mul(modelView, sphere);

            float radius = uboarr[InstanceIndex].objectSpaceboundsRadius;
            float ds = length(_point - sphere.xyz) - radius;
            d += ds;
            if (d > MAX_DIST || ds < EPS)
                break; // hit object or out of scene
        }

        if (d > MAX_DIST)
        {
            d = 0;
        }
        d /= (MAX_DIST / 100);
        d_out = max(d_out, d);
    }

    // for (int i =0; i < 600; i++)
    // {
    //     float4 objectSpacePos = uboarr[i].objectSpaceboundsCenter; 
    //     float4 Pos = mul(mvp,objectSpacePos);
    //     float4 worldPos = mul(ubo.Model, objectSpacePos);
    // }


    float4 __out;
    __out = d_out;
    // float4 __out = {uv.x, uv.y, 1, 1};
    output.Color = __out;


    return output;
}
