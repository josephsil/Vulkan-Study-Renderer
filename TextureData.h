#pragma once

#pragma region forward declarations
#include <cstdint>
#include <vulkan/vulkan_core.h>
struct VkPipelineShaderStageCreateInfo;
struct VkPipelineShaderStageCreateInfo;
using VkPhysicalDevice = struct VkPhysicalDevice_T*;
using VkDevice = struct VkDevice_T*;
using VkShaderModule = struct VkShaderModule_T*;
using VkBuffer = struct VkBuffer_T*;
using VkDeviceMemory = struct VkDeviceMemory_T*;
using VkImageView = struct VkImageView_T*;
using VkImage = struct VkImage_T*;
using VkSampler = struct VkSampler_T*;


class HelloTriangleApplication;
#pragma endregion
//TODO JS: obviously improve 
struct TextureData
{
public:
    enum TextureType
    {
        DIFFUSE,
        SPECULAR,
        NORMAL,
        CUBE,
        LINEAR_DATA
    };

    VkImageView textureImageView;
    VkSampler textureSampler;
    HelloTriangleApplication* appref;
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    uint32_t maxmip = 1;
    uint32_t layerct = 1;
    int id;

    TextureData(HelloTriangleApplication* app, const char* path, TextureType type);

    TextureData();

    void cleanup();

private:
    void createTextureSampler(VkSamplerAddressMode mode = VK_SAMPLER_ADDRESS_MODE_REPEAT, float bias = 0);


    void createTextureImageView(VkFormat format, VkImageViewType type);


    //TODO JS: 
    /*All of the helper functions that submit commands so far have been set up to execute synchronously by waiting for the queue to become idle.
    For practical applications it is recommended to combine these operations in a single command bufferand execute them asynchronously for
    higher throughput, especially the transitionsand copy in the createTextureImage function.
    Try to experiment with this by creating a setupCommandBuffer that the helper functions record commands into,
    and add a flushSetupCommands to execute the commands that have been recorded so far.
    It's best to do this after the texture mapping works to check if the texture resources are still set up correctly.*/


    void createTextureImage(const char* path, VkFormat format, bool mips = true);
    void createCubemapImageKTX(const char* path, VkFormat format);
};
