#include "structs.hlsl"

[[vk::binding(0, 0)]]
RWTexture2D<float4> texture;
[[vk::binding(1, 0)]]
SamplerState samplers;

struct mipchainPushConstant
{
	int mipLevel;
};
[[vk::push_constant]]
mipchainPushConstant pc;

[numthreads(8, 8, 1)]
void Main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
	//Need to undo the part where we put actual mip levels on our depth buffer/attatchment
		//Zeux thing where he snaps dpeth buffer down to next POT *before* generating mip pyramid, and uses that new image with its mips seems better
	//https://www.rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/
	//https://vkguide.dev/docs/gpudriven/compute_culling/
	// float depth =
	//
	// 	texture.Sample(mipLevel)
	
}