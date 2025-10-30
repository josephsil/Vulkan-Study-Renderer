#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <FileCaching.h>


#pragma region forward declarations
struct VkPipelineShaderStageCreateInfo;
using VkDevice = struct VkDevice_T*;
using VkShaderModule = struct VkShaderModule_T*;
#pragma endregion

struct ShaderLoader
{
    ShaderLoader(VkDevice device);

    std::unordered_map<std::string_view, std::vector<VkPipelineShaderStageCreateInfo>> compiledShaders;

    void AddShader(const char* name, platform_string shaderPath, bool compute = false);

private:
    enum shaderType
    {
        frag,
        vert,
        comp
    };

    void shaderCompile(platform_string shaderFilename, shaderType stagetype);
    VkShaderModule shaderLoad(platform_string shaderFilename, shaderType stagetype);
    VkDevice device_;
};
