#pragma once
#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <memory>
#include <span>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "./VulkanIncludes/Vulkan_Includes.h"
#include "../General/Array.h"
#include "rendererGlobals.h"
#include "CommandPoolManager.h"
#include "../General/Memory.h"
#include "PipelineDataObject.h"
// My stuff 

class Scene;
struct PerShadowData;
struct MeshData; //Forward Declaration
struct Vertex; //Forward Declaration
struct ShaderLoader;
struct TextureData;
using VmaAllocator = struct VmaAllocator_T*;
//Include last

const uint32_t SHADOW_MAP_SIZE = 1024;


struct  semaphoreData
{
    std::span<VkSemaphore> semaphores;
    std::span<VkPipelineStageFlags> waitStages;
};

class HelloTriangleApplication
{
public:

    struct cameraData
    {
        glm::vec3 eyePos = glm::vec3(-4.0f, 0.4f, 1.0f);
        glm::vec2 eyeRotation = glm::vec2(55.0f, -22.0f); //yaw, pitch
        float nearPlane = 0.01f;
        float farPlane = 35.0f;

        VkExtent2D extent;
        float fov = 70;
    };
    
    Scene* scene;
    RendererHandles getHandles();
    void updateShadowImageViews(int frame);
    HelloTriangleApplication();


private:

    

    MemoryArena::memoryArena rendererArena{};
    
    MemoryArena::memoryArena perFrameArenas[MAX_FRAMES_IN_FLIGHT];
#pragma region SDL
    uint32_t T;
    uint32_t T2;
    float deltaTime;


   
    //GLFWwindow* window;
    int WIDTH = 1280;
    int HEIGHT = 720;
    struct SDL_Window* _window{nullptr};

#pragma endregion

  

    cameraData camera;
   
    
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device; //Logical device
    VkInstance instance;
    CommandPoolManager commandPoolmanager;
    VmaAllocator allocator;

    VkDebugUtilsMessengerEXT debugMessenger;
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

   
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };
   

    VkSwapchainKHR swapChain;
    VkFormat swapChainColorFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkImage> swapChainImages;

    std::span<std::span<VkImageView>> shadowMapRenderingImageViews;
    std::span<std::span<VkImageView>> shadowMapSamplingImageViews;
    std::vector<VkSampler> shadowSamplers;
    std::vector<VkImage> shadowImages;
    std::vector<VmaAllocation> shadowMemory;

    std::span<std::span<PerShadowData>> perLightShadowData;


    VkFormat shadowFormat;
    
    VkImage depthImage;
    VmaAllocation depthImageMemory;
    VkFormat depthFormat;
    VkImageView depthImageView;

#pragma region  descriptor sets 
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout pushDescriptorSetLayout;
    VkDescriptorSetLayout perMaterialSetLayout;
    
    PipelineDataObject descriptorsetLayoutsData;

    
    void createDescriptorSetPool(RendererHandles handles, VkDescriptorPool* pool);
    void updateOpaqueDescriptorSets(PipelineDataObject* layoutData);
    std::span<descriptorUpdateData> createOpaqueDescriptorUpdates(uint32_t frame, MemoryArena::memoryArena* arena, std::span<VkDescriptorSetLayoutBinding> layoutBindings);
    std::span<descriptorUpdateData> createShadowDescriptorUpdates(MemoryArena::memoryArena* arena, uint32_t frame, uint32_t shadowIndex, std::span<VkDescriptorSetLayoutBinding>
                                                                  layoutBindings);

    void updateShadowDescriptorSets(
        PipelineDataObject* layoutData, uint32_t shadowIndex);

    std::span<VkDescriptorSetLayoutBinding> opaqueLayout;
    std::span<descriptorUpdateData> opaqueUpdates[MAX_FRAMES_IN_FLIGHT] = {};

    std::span<VkDescriptorSetLayoutBinding> shadowLayout;
    std::span<std::span<descriptorUpdateData>> shadowUpdates[MAX_FRAMES_IN_FLIGHT] = {};

#pragma endregion

    struct per_frame_data
    {
        //Below is all vulkan stuff 
        VkSemaphore shadowAvailableSemaphores {};
        VkSemaphore shadowFinishedSemaphores {};
        VkSemaphore imageAvailableSemaphores {};
        VkSemaphore renderFinishedSemaphores {};

        VkSemaphore swapchaintransitionedOutSemaphores {};
        VkSemaphore swapchaintransitionedInSemaphores {};
        VkSemaphore shadowtransitionedOutSemaphores {};
        VkSemaphore shadowtransitionedInSemaphores {};
        
        VkFence inFlightFences {};
        VkCommandBuffer opaqueCommandBuffers {};
        VkCommandBuffer shadowCommandBuffers {};
        VkCommandBuffer swapchainTransitionInCommandBuffer {};
        VkCommandBuffer swapchainTransitionOutCommandBuffer {};
        VkCommandBuffer shadowTransitionInCommandBuffer {};
        VkCommandBuffer shadowTransitionOutCommandBuffer {};

#pragma region buffers

        std::vector<dataBuffer> perLightShadowShaderGlobalsBuffer;
        std::vector<VmaAllocation> perLightShadowShaderGlobalsMemory;
        std::vector<void*> perLightShadowShaderGlobalsMapped;
        
        dataBuffer opaqueShaderGlobalsBuffer;
        VmaAllocation opaqueShaderGlobalsMemory;
    
        //TODO JS: Move the data buffer stuff?
        dataBuffer uniformBuffers;
        VmaAllocation uniformBuffersMemory;
        
        dataBuffer meshBuffers;
        VmaAllocation meshBuffersMemory;

        //Basic data about the light used in all passes 
        dataBuffer lightBuffers;
        VmaAllocation lightBuffersMemory;
        
        //Additional light matrices.... Maybe should be additional matrices period? Used in shadow pass
        dataBuffer shadowDataBuffers;
        VmaAllocation shadowDataBuffersMemory;
#pragma endregion
    };
    
    std::vector<per_frame_data> FramesInFlightData;
    
    

    int cubemaplut_utilitytexture_index;

    uint32_t currentFrame = 0;

#ifdef NDEBUG
        const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    ShaderLoader* shaderLoader = nullptr;

    
    void initWindow();
    void initVulkan();


    PFN_vkCopyImageToMemoryEXT vkCopyImageToMemoryEXT;

    void updateLightBuffers(uint32_t currentImage);
    void populateMeshBuffers();
    void createUniformBuffers();

    void updateUniformBuffer(uint32_t currentImage, glm::mat4 model);
    void updateCamera(inputData input);
    //Globals per pass, ubos, and lights are updated every frame
    void updatePerFrameBuffers(unsigned currentFrame, Array<glm::mat4> models);
    void recordCommandBufferShadowPass(VkCommandBuffer commandBuffer, uint32_t imageIndex);


    void createDescriptorSetLayout(VkDescriptorSetLayoutCreateInfo layoutinfo, VkDescriptorSetLayout* layout);


    void compileShaders();
    void createSyncObjects();

    void createCommandBuffers();
    void recordCommandBufferOpaquePass(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void createGraphicsCommandPool();
    void createTransferCommandPool();

    void createDepthResources();


    bool hasStencilComponent(VkFormat format);


    void createGraphicsPipeline(const char* shaderName,
                                PipelineDataObject* descriptorsetdata, bool shadow = false, bool lines = false);
    void createInstance();

    int _selectedShader{0};

    void mainLoop();

    void UpdateRotations();

    void drawFrame();


    void renderShadowPass(uint32_t currentFrame, uint32_t imageIndex, semaphoreData waitSemaphoreData,
                          std::vector<VkSemaphore> signalsemaphores);
    void renderOpaquePass(uint32_t currentFrame, uint32_t imageIndex, semaphoreData waitSemaphoreData, std::vector<VkSemaphore>
                          signalsemaphores);

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


    //Test/practice fn to confirm that a set of extensions is supported

    void printDebugSupportedExtensions(std::vector<const char*> requiredExtensions,
                                       std::vector<VkExtensionProperties> supportedExtensions);

    bool checkValidationLayerSupport();

    std::vector<const char*> getRequiredExtensions();


#pragma endregion
};