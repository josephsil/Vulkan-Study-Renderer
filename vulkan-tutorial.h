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

    const int MAX_FRAMES_IN_FLIGHT = 3;

#pragma region SDL
    uint32_t T;
    uint32_t T2;
    float deltaTime;

    struct inputData
    {
        glm::vec3 translate;
        glm::vec3 mouseRot;
    };
   
    //GLFWwindow* window;
    int WIDTH = 1280;
    int HEIGHT = 720;
    struct SDL_Window* _window{nullptr};

#pragma endregion

    glm::vec3 eyePos = glm::vec3(2.0f, 1.0f, 0.0f);
    glm::vec3 eyeEulers;
    
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
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkImage> swapChainImages;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    
    VkImage depthImage;
    VmaAllocation depthImageMemory;
    VkImageView depthImageView;
    VkRenderPass renderPass;

#pragma region  descriptor sets 
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout pushDescriptorSetLayout;
    VkDescriptorSetLayout perMaterialSetLayout;
    VkPipelineLayout pipelineLayout;
    
    DescriptorSets::bindlessDescriptorSetData descriptorsetLayoutsData;

    
    void createDescriptorSetPool(RendererHandles handles, VkDescriptorPool* pool, std::vector<DescriptorSets::layoutInfo> infos);
    void updateDescriptorSets(RendererHandles handles, VkDescriptorPool pool, DescriptorSets::bindlessDescriptorSetData* layoutData);

#pragma endregion

    struct per_frame_data
    {
        VkSemaphore imageAvailableSemaphores {};
        VkSemaphore renderFinishedSemaphores {};
        VkFence inFlightFences {};

#pragma region buffers
        dataBuffer shaderGlobalsBuffer;
        VmaAllocation shaderGlobalsMemory;
        void* shaderGlobalsMapped;
    
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
    std::vector<VkCommandBuffer> commandBuffers {};
    
    VkPipeline bindlessPipeline_1;
    VkPipeline bindlessPipeline_2;

    int cubemaplut_utilitytexture_index;

    struct UniformBufferObject
    {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 Normal;
        alignas(16) glm::mat4 padding1;
        alignas(16) glm::mat4 padding2;
    };

    struct ShaderGlobals
    {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::vec4 viewPos;
        alignas(16) glm::vec4 lightcountx_modey_paddingzw;
        alignas(16) glm::vec4 cubemaplutidx_cubemaplutsampleridx_paddingzw;
    };

    struct per_object_data
    {
        //Light count, vertex offset, texture index, ubo index
        alignas(16) glm::vec4 indexInfo;

        alignas(16) glm::vec4 materialprops; //roughness, metalness, padding, padding
        alignas(16) glm::vec4 padding_1;
        alignas(16) glm::vec4 padding_2;
        alignas(16) glm::vec4 padding_3;
        //Unused
        alignas(16) glm::mat4 padding1;
        //Unused
        alignas(16) glm::mat4 padding2;
    };
   
    uint32_t currentFrame = 0;

#ifdef NDEBUG
        const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    ShaderLoader* shaderLoader = nullptr;

    
    void initWindow();
    void initVulkan();




    void updateLightBuffers(uint32_t currentImage);
    void populateMeshBuffers();
    void createUniformBuffers();

    void updateUniformBuffer(uint32_t currentImage, glm::mat4 model);
    void updateUniformBuffers(uint32_t currentImage, std::vector<glm::mat4> models, inputData input);


    void createDescriptorSetLayout(VkDescriptorSetLayoutCreateInfo layoutinfo, VkDescriptorSetLayout* layout);


    void compileShaders();
    void createSyncObjects();

    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, VkPipeline graphicsPipeline);

    void createGraphicsCommandPool();
    void createTransferCommandPool();

    void createDepthResources();
    void createFramebuffers();


    bool hasStencilComponent(VkFormat format);


    VkPipeline createGraphicsPipeline(const char* shaderName, VkRenderPass renderPass,
                                      VkPipelineCache pipelineCache, std::vector<VkDescriptorSetLayout> layouts);
    void createInstance();

    int _selectedShader{0};

    void mainLoop();

    void UpdateRotations();

    void drawFrame(inputData input);

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
