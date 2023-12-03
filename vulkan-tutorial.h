#pragma once
#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Vulkan_Includes.h"

#include "AppStruct.h"
#include "CommandPoolManager.h"
#include "Memory.h"
#include "PipelineDataObject.h"
#include "vulkan-utilities.h"
// My stuff 

struct dataBuffer;
struct MeshData; //Forward Declaration
struct Vertex; //Forward Declaration
struct ShaderLoader;
struct TextureData;
using VmaAllocator = struct VmaAllocator_T*;
class Scene;
//Include last

class HelloTriangleApplication

{
public:
    std::unique_ptr<Scene> scene;
    RendererHandles getHandles();
    HelloTriangleApplication();
    

   

private:

    const static int MAX_FRAMES_IN_FLIGHT = 3;
    const static int MAX_SHADOWCASTERS = 1;

    MemoryArena::memoryArena arena{};
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

    glm::vec3 eyePos = glm::vec3(1.0f, 0.2f, 0.0f);
    glm::vec2 eyeRotation = glm::vec2(0.0f, 0.0f); //yaw, pitch
    
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

    std::vector<VkImageView> shadowImageViews;
    std::vector<VkImage> shadowImages;
    std::vector<VmaAllocation> shadowMemory;
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
    void updateOpaqueDescriptorSets(RendererHandles handles, VkDescriptorPool pool, PipelineDataObject* layoutData);
    void updateShadowDescriptorSets(RendererHandles handles, VkDescriptorPool pool,
                                    PipelineDataObject* layoutData, uint32_t shadowIndex);

#pragma endregion

    struct per_frame_data
    {
        VkSemaphore shadowAvailableSemaphores {};
        VkSemaphore shadowFinishedSemaphores {};
        VkSemaphore imageAvailableSemaphores {};
        VkSemaphore renderFinishedSemaphores {};

        VkSemaphore swapchaintransitionedOutSemaphores {};
        VkSemaphore swapchaintransitionedInSemaphores {};
        
        VkFence inFlightFences {};
        VkCommandBuffer opaqueCommandBuffers {};
        VkCommandBuffer shadowCommandBuffers {};
        VkCommandBuffer swapchainTransitionInCommandBuffer {};
        VkCommandBuffer swapchainTransitionOutCommandBuffer {};

#pragma region buffers

        //TODO JS: More expressive pass system
        std::vector<dataBuffer> perLightShadowShaderGlobalsBuffer;
        std::vector<VmaAllocation> perLightShadowShaderGlobalsMemory;
        std::vector<void*> perLightShadowShaderGlobalsMapped;
        
        dataBuffer opaqueShaderGlobalsBuffer;
        VmaAllocation opaqueShaderGlobalsMemory;
        void* opaqueShaderGlobalsMapped;
    
        //TODO JS: Move the data buffer stuff?
        dataBuffer uniformBuffers;
        VmaAllocation uniformBuffersMemory;
        void* uniformBuffersMapped;

        dataBuffer meshBuffers;
        VmaAllocation meshBuffersMemory;
        void* meshBuffersMapped;

        dataBuffer lightBuffers;
        VmaAllocation lightBuffersMemory;
        void* lightBuffersMapped;
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
    Transform getCameraTransform();
    void updateCamera(inputData input);
    //Globals per pass, ubos, and lights are updated every frame
    void updatePerFrameBuffers(unsigned currentFrame, Array<glm::mat4> models, inputData input);
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
                                PipelineDataObject* descriptorsetdata, bool shadow = false);
    void createInstance();

    int _selectedShader{0};

    void mainLoop();

    void UpdateRotations();

    void drawFrame(inputData input);
    void renderShadowPass(uint32_t currentFrame, uint32_t imageIndex, std::vector<VkSemaphore> waitsemaphores,
                          std::vector<VkSemaphore> signalsemaphores);
    void renderOpaquePass(uint32_t currentFrame, uint32_t imageIndex, std::vector<VkSemaphore> waitsemaphores, std::vector<VkSemaphore>
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