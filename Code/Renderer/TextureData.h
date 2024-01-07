#pragma once

#pragma region forward declarations
#include <cstdint>

#include "RendererHandles.h"
#include "VulkanIncludes/forward-declarations-renderer.h"
using ktxTexture2 =  struct ktxTexture2;
using  ktxVulkanTexture = struct ktxVulkanTexture;

#pragma endregion

struct temporaryTextureInfo
{
    VkImage textureImage;
    VmaAllocation alloc;
    uint32_t width;
    uint32_t height;
    uint8_t mipCt;
};
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
        LINEAR_DATA,
        DATA_DONT_COMPRESS
    };

  

    VkImageView textureImageView;
    VkSampler textureSampler;
    VkImage textureImage;
    VmaAllocation textureImageMemory;

    //TODO JS: move out of texturedata
    RendererHandles rendererHandles;
    uint32_t maxmip = 1; //TODO JS: mutate less places
    uint32_t layerct = 1;
    bool uncompressed = false;

    
    TextureData(RendererHandles rendererHandles, const char* path, TextureType type, VkImageViewType viewType = (VkImageViewType)-1);
    VkFormat GetOrLoadTexture(const char* path, VkFormat format, TextureType textureType, bool use_mipmaps);

    TextureData();

    void cleanup();

    //TODO JS: move tou tilities?
    // 0 = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    static void createTextureSampler(VkSampler* textureSampler, RendererHandles handles, VkSamplerAddressMode mode, float bias, float maxMip, bool shadow = false);
    VkImageView createTextureImageView(VkFormat format, VkImageViewType type);
private:


    void cacheKTXFromSTB(const char* path, const char* outpath, VkFormat format, TextureType textureType,
                         bool use_mipmaps);
    


    //TODO JS: 
    /*All of the helper functions that submit commands so far have been set up to execute synchronously by waiting for the queue to become idle.
    For practical applications it is recommended to combine these operations in a single command bufferand execute them asynchronously for
    higher throughput, especially the transitionsand copy in the createTextureImage function.
    Try to experiment with this by creating a setupCommandBuffer that the helper functions record commands into,
    and add a flushSetupCommands to execute the commands that have been recorded so far.
    It's best to do this after the texture mapping works to check if the texture resources are still set up correctly.*/


    VkFormat createImageKTX(const char* path, TextureType type, bool mips);
};

