#include "structs.hlsl"

[[vk::binding(12, 0)]]
Texture2D<float4> srcTexture; //could be read only
[[vk::binding(13, 0)]]
RWTexture2D<float4> dstTexture; //could be write  only
[[vk::binding(14, 0)]]
SamplerState samplers;

struct mipchainPushConstant
{
	float2 size;
};
[[vk::push_constant]]
mipchainPushConstant pc;

[numthreads(8, 8, 1)]
void Main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
	//todo js Need to undo the part where we put actual mip levels on our depth buffer/attatchment
		//Zeux thing where he snaps dpeth buffer down to next POT *before* generating mip pyramid, and uses that new image with its mips seems better
	//https://www.rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/
	//https://vkguide.dev/docs/gpudriven/compute_culling/
	uint2 pos = GlobalInvocationID.xy;

	// Sampler is set up to do min reduction, so this computes the minimum depth of a 2x2 texel quad
	float depth = srcTexture.SampleLevel(samplers, (float2(pos) + float2(0.5, 0.5)) / pc.size, 0).x;

	dstTexture[pos] = (depth);
	
}