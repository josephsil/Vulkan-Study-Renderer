#include "structs.hlsl"

//Functions for indexing into the 'drawIndices/drawCompactionDataBuffer' data 
//This is an array of uints, laid out like this:
//[0] = global number of draws for the frame 
//[1:MAX_RENDER_PASSES] = number of draws for the pass 
//[1+MAX_RENDER_PASSES:MAX_RENDER_PASSES] = draw offset for the start of the pass 
//[1+(MAX_RENDER_PASSES*2):..] = (wip) Offsets for each shader within each pass.	
							//Blocks of (passIndex * MAX_PIPELINES) size slots for offsets 

uint GetPassOffsetIndex(uint input)
{
	return   input + 1 + MAX_RENDER_PASSES;
}

uint GetPassCountIndex(uint input)
{
	return  input + 1;
}

uint GetGlobalDrawCountIndex()
{
	return   0;
}

uint GetPassSubpassIndex(uint passIndex, uint shaderIndex) 
{
	uint baseOffset = 1 + (MAX_RENDER_PASSES *2);
	uint passOffset = passIndex * MAX_PIPELINES;
	return baseOffset + passOffset + shaderIndex;
}

uint GetPassSubpassCounterIndex(uint passIndex, uint shaderIndex) 
{
	uint baseOffset = (1 + (MAX_RENDER_PASSES *2)) + MAX_RENDER_PASSES * MAX_PIPELINES;
	uint passOffset = passIndex * MAX_PIPELINES;
	return baseOffset + passOffset + shaderIndex;
}