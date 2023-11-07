#pragma once
#include <string>
#include <vector>
#include <map>


#pragma region forward declarations
struct VkPipelineShaderStageCreateInfo;
using VkDevice = struct VkDevice_T*;
using VkShaderModule = struct VkShaderModule_T*;
#pragma endregion

struct ShaderLoader
{
public:
    ShaderLoader(VkDevice device);

    std::map<const char*, std::vector<VkPipelineShaderStageCreateInfo>> compiledShaders;

    void AddShader(const char* name, std::wstring shaderPath);
private:
    void shaderCompile(std::wstring shaderFilename, bool is_frag);
    VkShaderModule shaderLoad(std::wstring shaderFilename, bool is_frag);
    VkDevice device_;
};
