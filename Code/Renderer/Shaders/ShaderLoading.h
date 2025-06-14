#pragma once
#include <string>
#include <vector>
#include <unordered_map>


#pragma region forward declarations
struct VkPipelineShaderStageCreateInfo;
using VkDevice = struct VkDevice_T*;
using VkShaderModule = struct VkShaderModule_T*;
#pragma endregion

struct ShaderLoader
{
    ShaderLoader(VkDevice device);

    std::unordered_map<std::string_view, std::vector<VkPipelineShaderStageCreateInfo>> compiledShaders;

    void AddShader(const char* name, std::wstring shaderPath, bool compute = false);

private:
    enum shaderType
    {
        frag,
        vert,
        comp
    };

    void shaderCompile(std::wstring shaderFilename, shaderType stagetype);
    VkShaderModule shaderLoad(std::wstring shaderFilename, shaderType stagetype);
    VkDevice device_;
};
