#pragma once

#include <Windows.h>
#include <string>
#include <iostream>
#include <vector>
#include "dxcapi.h"
#include <map>

#include <d3dcompiler.h>
#include <atlbase.h>
#include <vulkan/vulkan.h>

class ShaderLoader {

public:
	ShaderLoader(VkDevice device);

	std::map<const char*, std::vector<VkPipelineShaderStageCreateInfo>>		compiledShaders;

	void AddShader(const char* name, std::wstring shaderPath);
private:
	VkDevice device_;
	VkShaderModule shaderLoad(std::wstring shaderFilename, bool is_frag);
};