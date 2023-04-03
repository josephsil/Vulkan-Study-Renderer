#pragma once

#pragma region forward declarations
#include <cstdint>
#include <vulkan/vulkan_core.h>
struct VkPipelineShaderStageCreateInfo;
struct VkPipelineShaderStageCreateInfo;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T* VkDevice;
typedef struct VkShaderModule_T* VkShaderModule;
typedef struct VkBuffer_T* VkBuffer;
typedef struct VkDeviceMemory_T* VkDeviceMemory;
typedef struct VkImageView_T* VkImageView;
typedef struct VkImage_T* VkImage;
typedef struct VkSampler_T* VkSampler;


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
          CUBE
          
      };
      VkImageView textureImageView;
      VkSampler textureSampler;
      HelloTriangleApplication* appref;
      VkImage textureImage;
      VkDeviceMemory textureImageMemory;
      uint32_t maxmip =0;
      int id;

      TextureData(HelloTriangleApplication* app, const char* path, TextureData::TextureType type);

      TextureData();

      void cleanup();

  private:
      void createTextureSampler();


      void createTextureImageView( VkFormat format);
      

      //TODO JS: 
      /*All of the helper functions that submit commands so far have been set up to execute synchronously by waiting for the queue to become idle.
      For practical applications it is recommended to combine these operations in a single command bufferand execute them asynchronously for
      higher throughput, especially the transitionsand copy in the createTextureImage function.
      Try to experiment with this by creating a setupCommandBuffer that the helper functions record commands into,
      and add a flushSetupCommands to execute the commands that have been recorded so far.
      It's best to do this after the texture mapping works to check if the texture resources are still set up correctly.*/


      void createTextureImage(const char* path, VkFormat format);
  };
