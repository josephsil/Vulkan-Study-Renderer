#pragma once
#include <string>
#include <vector>
#include <map>


#pragma region forward declarations 
struct VkPipelineShaderStageCreateInfo;
struct VkDevice_T;
struct VkShaderModule_T;
typedef  VkDevice_T* VkDevice;
typedef  VkShaderModule_T* VkShaderModule;
#pragma endregion

struct ShaderLoader {

public:
	ShaderLoader(VkDevice device);

	std::map<const char*, std::vector<VkPipelineShaderStageCreateInfo>>		compiledShaders;

	void AddShader(const char* name, std::wstring shaderPath);
private:
	VkShaderModule shaderLoad(std::wstring shaderFilename, bool is_frag);
	VkDevice device_;
};