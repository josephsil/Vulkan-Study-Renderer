#pragma once

#pragma region forward declarations
#include <cstdint>
#include <vulkan/vulkan_core.h>

#include "AppStruct.h"
#include "vulkan-forwards.h"


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

    struct bufferAndMemory
    {
        VkBuffer buffer;
        VkDeviceMemory bufferMemory;
    };

    bufferAndMemory stagingBuffer;
    VkImageView textureImageView;
    VkSampler textureSampler;
    RendererHandles rendererHandles;
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    uint32_t maxmip = 1; //TODO JS: mutate less places
    uint32_t layerct = 1;
    int id;


    //TODO JS: Specific to writing ktx
    struct imageData
    {
        int width;
        int height;
        int mipLevels;
        bool generateMips = false;
        int depth = 1;
        int layers = 1;
        int dimension = 2;
    };

    imageData iData;

    
    TextureData(RendererHandles rendererHandles, const char* path, TextureType type);
    void GetOrLoadTexture(const char* path, VkFormat format, TextureType textureType, bool use_mipmaps);

    TextureData();

    void cleanup();

private:
    // 0 = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    void createTextureSampler(VkSamplerAddressMode mode = (VkSamplerAddressMode)0, float bias = 0);
    void cacheKTXFromSTB(const char* path, const char* outpath, VkFormat format, TextureType textureType,
                         bool use_mipmaps);


    void createTextureImageView(VkFormat format, VkImageViewType type);


    //TODO JS: 
    /*All of the helper functions that submit commands so far have been set up to execute synchronously by waiting for the queue to become idle.
    For practical applications it is recommended to combine these operations in a single command bufferand execute them asynchronously for
    higher throughput, especially the transitionsand copy in the createTextureImage function.
    Try to experiment with this by creating a setupCommandBuffer that the helper functions record commands into,
    and add a flushSetupCommands to execute the commands that have been recorded so far.
    It's best to do this after the texture mapping works to check if the texture resources are still set up correctly.*/


    TextureData::bufferAndMemory createTextureImage(const char* path, VkFormat format, bool mips = true);
    void createImageKTX(const char* path, TextureType type, bool mips);
};
