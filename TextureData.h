#pragma once

#pragma region forward declarations
#include <cstdint>

#include "AppStruct.h"
#include "forward-declarations-renderer.h"
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

    
    TextureData(RendererHandles rendererHandles, const char* path, TextureType type);
    VkFormat GetOrLoadTexture(const char* path, VkFormat format, TextureType textureType, bool use_mipmaps);

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


    VkFormat createImageKTX(const char* path, TextureType type, bool mips);
};

