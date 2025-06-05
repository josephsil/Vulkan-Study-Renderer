#define SKYBOXLUTINDEX ShaderGlobals.lutIDX_lutSamplerIDX_padding_padding.x
#define SKYBOXLUTSAMPLERINDEX ShaderGlobals.lutIDX_lutSamplerIDX_padding_padding.y
#define SHADOWCOUNT ShaderGlobals.lightcount_mode_shadowct_padding.z
#define LIGHTCOUNT   ShaderGlobals.lightcount_mode_shadowct_padding.r
#define VIEW_MATRIX (ShaderGlobals.viewMatrix)
#define PROJ_MATRIX (ShaderGlobals.projMatrix)
#define VIEW_PROJ_MATRIX mul(PROJ_MATRIX, VIEW_MATRIX)


#define WorldToClip(worldSpacePos)  mul(PROJ_MATRIX, mul(VIEW_MATRIX, float4(worldSpacePos.xyz, 1)))
#define WorldToView(worldSpacePos)   mul(VIEW_MATRIX, float4(worldSpacePos, 1))
#define ObjectToWorld(objectSpacePos)  mul(MODEL_MATRIX,objectSpacePos)
#define ObjectToView(objectSpacePos)  mul(MODEL_VIEW_MATRIX,objectSpacePos)
#define ObjectToClip(objectSpacePos)  mul(MVP_MATRIX, objectSpacePos)
#define ClipToNDC(clipSpacePos) clipSpacePos.xyz / clipSpacePos.w
#define PI 3.1415926535f

float2 NDCToUV(float3 input)
{
   return input.xy * 0.5 + 0.5;
}

float NDCToDepth(float3 input)
{
   return input.z;
}
