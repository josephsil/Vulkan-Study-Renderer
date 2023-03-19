#pragma once


#include <array>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <map>
#include <set>
#include <vector>
#include <optional>

#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp

#include <fstream>

#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

//TODO JS: use
#include "vk_mem_alloc.h"
#include <tiny_obj_loader.h>

#include "stb_image.h"

#include <chrono>
// My stuff 
#include "ShaderLoading.h"

struct MeshData; //Forward Declaration
struct Vertex;  //Forward Declaration

//Include last

   class HelloTriangleApplication
    {
    public:

       VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
       VkDevice device; //Logical device
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer,
                          VkDeviceMemory& bufferMemory);
        void run();

        HelloTriangleApplication();

    private:
        struct TextureData;
        struct SDL_Window* _window{nullptr};
        VkInstance instance;
        VkDebugUtilsMessengerEXT debugMessenger;
        //GLFWwindow* window;
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;
      


        VkQueue graphicsQueue;

        VkQueue presentQueue;

        VkQueue transferQueue;

        VkSwapchainKHR swapChain;

        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;


        std::vector<VkImage> swapChainImages;

        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;

        std::vector<VkImageView> swapChainImageViews;

        VkRenderPass renderPass;

        VkDescriptorSetLayout descriptorSetLayout;
        VkPipelineLayout pipelineLayout;

        VkPipeline graphicsPipeline_1;
        VkPipeline graphicsPipeline_2;

        struct SwapChainSupportDetails
        {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        const std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        std::vector<VkFramebuffer> swapChainFramebuffers;

        const std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

        VkCommandPool commandPool;
        VkCommandPool transferCommandPool;

      
        MeshData* placeholderMesh;
      


        //TODO JS: obviously improve 
        struct TextureData
        {
        public:
            VkImageView textureImageView;
            VkSampler textureSampler;
            HelloTriangleApplication* appref;
            VkImage textureImage;
            VkDeviceMemory textureImageMemory;

            TextureData(HelloTriangleApplication* app, const char* path);

            TextureData();

            void cleanup();

        private:
            void createTextureSampler();


            void createTextureImageView();


            //TODO JS: 
            /*All of the helper functions that submit commands so far have been set up to execute synchronously by waiting for the queue to become idle.
            For practical applications it is recommended to combine these operations in a single command bufferand execute them asynchronously for
            higher throughput, especially the transitionsand copy in the createTextureImage function.
            Try to experiment with this by creating a setupCommandBuffer that the helper functions record commands into,
            and add a flushSetupCommands to execute the commands that have been recorded so far.
            It's best to do this after the texture mapping works to check if the texture resources are still set up correctly.*/


            void createTextureImage(const char* path);
        };

        TextureData placeholderTexture;
        //STill not 100% sure how the mesh and material component of a "draw' is supposed to get submitted in a real program
        //I think my mesh/texture structs are vaguely along the right lines, but the _work_ getting done shouldnt be taking in copies, it should be refs
        //The work probably shouldnt be happening on a per tex or per mesh case either?

        std::vector<VkCommandBuffer> commandBuffers;


        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;

        struct UniformBufferObject
        {
            glm::vec2 foo;
            alignas(16) glm::mat4 model;
            alignas(16) glm::mat4 view;
            alignas(16) glm::mat4 proj;
        };


        const int MAX_FRAMES_IN_FLIGHT = 2;
        uint32_t currentFrame = 0;

#ifdef NDEBUG
        const bool enableValidationLayers = false;
#else
        const bool enableValidationLayers = true;
#endif

        ShaderLoader* shaderLoader = nullptr;


        std::vector<Vertex> trivertices;
        std::vector<uint32_t> triindices;

        void initWindow();


        void initVulkan();

        VkImageView createImageView(VkImage image, VkFormat format,
                                    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

        void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
                                   VkCommandBuffer workingBuffer = nullptr);

        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height,
                               VkCommandBuffer workingBuffer = nullptr);

        // TODO JS : Descriptor sets more related to shaders?

        VkDescriptorPool descriptorPool;
        std::vector<VkDescriptorSet> descriptorSets;


        //TODO JS: Move the uniform buffer data and fns and ? This belongs to like, a "material" 
        std::vector<VkBuffer> uniformBuffers;
        std::vector<VkDeviceMemory> uniformBuffersMemory;
        std::vector<void*> uniformBuffersMapped;

        void createDescriptorPool();

        struct test
        {
            int* something;
        };

        void createDescriptorSets(TextureData tex);


        void createUniformBuffers();


        //TODO JS: Push constants
        void updateUniformBuffer(uint32_t currentImage);


        void createDescriptorSetLayout();


        void compileShaders();

        

        /* It should be noted that in a real world application, you're not supposed to
         * actually call vkAllocateMemory for every individual buffer.
        The maximum number of simultaneous memory allocations is limited by the maxMemoryAllocationCount
        physical device limit, which may be as low as 4096 even on high end hardware like an NVIDIA GTX 1080.
         The right way to allocate memory for a large number of objects at the same time is to create a
         custom allocator that splits up a single allocation among many different objects
         by using the offset parameters that we've seen in many functions.
    
    
    You can either implement such an allocator yourself,
    or use the VulkanMemoryAllocator library provided by the GPUOpen initiative.
    However, for this tutorial it's okay to use a separate allocation for every resource,
    because we won't come close to hitting any of these limits for now.*/


        

        VkCommandBuffer beginSingleTimeCommands();

        void endSingleTimeCommands(VkCommandBuffer commandBuffer);

        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

        void createSyncObjects();

        //struct meshBuffers
        //{
        //	std::vector<VkBuffer> vertexBuffers;
        //	VkBuffer indexBuffer;
        //};

        /* Driver developers recommend that you also store multiple buffers, like the vertex and index buffer,
         * into a single VkBuffer and use offsets in commands like vkCmdBindVertexBuffers.*/


        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, VkPipeline graphicsPipeline,
                                 MeshData* mesh);

        void createCommandPool();

        void createTransferCommandPool();

        void createCommandBuffers();

        void createFramebuffers();


        void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                         VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

        void createDepthResources();

        VkFormat findDepthFormat();

        bool hasStencilComponent(VkFormat format);


        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                                     VkFormatFeatureFlags features);

        //TODO JS: VK_KHR_dynamic_rendering gets rid of render pass types and just lets you vkBeginRenderPass
        void createRenderPass();


        //TODO JS Collapse this? zoux niagra stream seems to do way less 
        VkPipeline createGraphicsPipeline(const char* shaderName, VkRenderPass renderPass, VkPipelineCache pipelineCache);


        void createImageViews();

        void createSwapChain();

        void createSurface();

        void createLogicalDevice();

        void pickPhysicalDevice();

        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);


        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);


        bool isDeviceSuitable(VkPhysicalDevice physicaldevice);

        bool checkDeviceExtensionSupport(VkPhysicalDevice device);


        struct QueueFamilyIndices
        {
            std::optional<uint32_t> graphicsFamily;
            std::optional<uint32_t> presentFamily;
            std::optional<uint32_t> transferFamily;

            bool isComplete();
        };

        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);


        void createInstance();

        int _selectedShader{0};

        void mainLoop();

        void drawFrame();

        void cleanup();

#pragma region debug info
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData);

        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

        void setupDebugMessenger();

#pragma endregion

#pragma region utility fns
#

        static std::vector<char> readFile(const std::string& filename);

        //Test/practice fn to confirm that a set of extensions is supported

        void printDebugSupportedExtensions(std::vector<const char*> requiredExtensions,
                                           std::vector<VkExtensionProperties> supportedExtensions);

        bool checkValidationLayerSupport();

        std::vector<const char*> getRequiredExtensions();



#pragma endregion
    };
