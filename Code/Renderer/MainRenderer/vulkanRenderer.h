#pragma once
#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <span>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <General/Array.h>
// #include "rendererGlobals.h"
#include "../CommandPoolManager.h"
#include "../gpu-data-structs.h"
#include <General/MemoryArena.h>
#include "../Shaders/ShaderLoading.h"
#include "VkBootstrap.h"
#include "../PipelineDataObject.h"
#include "Scene/Scene.h"
// My stuff 
struct gpulight;
struct gpuvertex;
struct PerShadowData;
struct MeshData; //Forward Declaration
struct Vertex; //Forward Declaration
struct TextureData;
using VmaAllocator = struct VmaAllocator_T*;
struct SDL_Window;
//Include last //

const uint32_t SHADOW_MAP_SIZE = 1024;


struct simplePassInfo
{
    uint32_t firstDraw = 0;
    uint32_t ct;
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix;
};

struct opaquePassInfo
{
    uint32_t start;
    uint32_t ct;
    glm::mat4 viewMatrix;
    VkPipeline pipeline;
    std::span<uint32_t> overrideIndices;
    
};
struct framePasses
{
    std::span<simplePassInfo> shadowDraws;
    std::span<simplePassInfo> computeDraws;
    std::span<opaquePassInfo> opaqueDraw;
};

struct  semaphoreData
{
    std::span<VkSemaphore> semaphores;
    std::span<VkPipelineStageFlags> waitStages;
};

struct InitializedRenderer
{
    vkb::Instance vkbInstance;
    vkb::PhysicalDevice vkbPhysicalDevice;
    vkb::Device vkbdevice;
    VmaAllocator vmaAllocator;
    VkSurfaceKHR surface; //not sure I need surface for anything except cleanup?
    vkb::Swapchain swapchain;
    //maybe move these two
};


struct vulkanTextureInfo //This is like general image info -- currently only using for shadows/depth buffer/etc but need to get away from TextureData.h
{
    VkFormat format;
    VkImage image;
    VkImageView view;
    VmaAllocation vmaAllocation;
};

struct RendererResources //Buffers, images, etc, used in core rendering -- probably to break up later
{
    
    std::unique_ptr<ShaderLoader> shaderLoader;

    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    
    std::span<std::span<VkImageView>> shadowMapRenderingImageViews;
    std::span<std::span<VkImageView>> shadowMapSamplingImageViews;
    
    std::vector<VkSampler> shadowSamplers;
    std::vector<VkImage> shadowImages;
    std::vector<VmaAllocation> shadowMemory;

    vulkanTextureInfo depthBufferInfo;
};

struct rendererCommandBuffers
{
    VkCommandBuffer computeCommandBuffers {};
    VkCommandBuffer opaqueCommandBuffers {};
    VkCommandBuffer shadowCommandBuffers {};
    VkCommandBuffer swapchainTransitionInCommandBuffer {};
    VkCommandBuffer swapchainTransitionOutCommandBuffer {};
    VkCommandBuffer shadowTransitionInCommandBuffer {};
    VkCommandBuffer shadowTransitionOutCommandBuffer {};
};

struct rendererSemaphores
{
    VkSemaphore computeFinishedSemaphores {};
    VkSemaphore shadowAvailableSemaphores {};
    VkSemaphore shadowFinishedSemaphores {};
    VkSemaphore imageAvailableSemaphores {};
    VkSemaphore renderFinishedSemaphores {};

    VkSemaphore swapchaintransitionedOutSemaphores {};
    VkSemaphore swapchaintransitionedInSemaphores {};
    VkSemaphore shadowtransitionedOutSemaphores {};
    VkSemaphore shadowtransitionedInSemaphores {};
};
    

InitializedRenderer initializeRendererHandles(SDL_Window* sdlWin, int WIDTH,int HEIGHT);




class vulkanRenderer
{

public:
    std::unordered_map<VkImageView, VkDescriptorSet> imguiRegisteredTextures;
    RendererLoadedAssetData* rendererSceneData;
    VkDescriptorSet GetOrRegisterImguiTextureHandle(VkSampler sampler, VkImageView imageView);
    void initializeDearIMGUI();
    RendererContext getHandles();
    void updateShadowImageViews(int frame, Scene* scene);
    vulkanRenderer();
    void initializeRendererForScene(Scene* scene);

    
    void Update(Scene* scene);
    void cleanup();


private:

    

    MemoryArena::memoryArena rendererArena{};
    
    MemoryArena::memoryArena perFrameArenas[MAX_FRAMES_IN_FLIGHT];
#pragma region SDL
    uint32_t T;
    uint32_t T2;


   
    //GLFWwindow* window;
    int WIDTH = 1280  * 1.5;
    int HEIGHT = 720 * 1.5;
    struct SDL_Window* _window{nullptr};

#pragma endregion
    glm::mat4 freezeView = {};

    InitializedRenderer rendererVulkanObjects;
    RendererResources rendererResources;
    // VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    // VkDevice device; //Logical device
    // // VkInstance instance;
    CommandPoolManager commandPoolmanager; //todo js
    // VmaAllocator allocator;

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
   



    std::span<std::span<PerShadowData>> perLightShadowData;


    VkFormat shadowFormat = VK_FORMAT_D16_UNORM;
    

#pragma region  descriptor sets 
    VkDescriptorPool descriptorPool;
    VkDescriptorPool imgui_descriptorPool;
    VkDescriptorSetLayout pushDescriptorSetLayout;
    VkDescriptorSetLayout perMaterialSetLayout;
    
    PipelineDataObject descriptorsetLayoutsData; //uhhh
    PipelineDataObject descriptorsetLayoutsDataShadow; //uhhh
    PipelineDataObject descriptorsetLayoutsDataCompute;

    
    void createDescriptorSetPool(RendererContext handles, VkDescriptorPool* pool);
    void updateOpaqueDescriptorSets(PipelineDataObject* layoutData);
    std::span<descriptorUpdateData> createOpaqueDescriptorUpdates(uint32_t frame, int shadowCasterCount, MemoryArena::memoryArena* arena, std::span<VkDescriptorSetLayoutBinding>
                                                                  layoutBindings);
    std::span<descriptorUpdateData> createShadowDescriptorUpdates(MemoryArena::memoryArena* arena, uint32_t frame, uint32_t shadowIndex, std::span<VkDescriptorSetLayoutBinding>
                                                                  layoutBindings);

    void updateShadowDescriptorSets(
        PipelineDataObject* layoutData, uint32_t shadowIndex);

    std::span<descriptorUpdateData> opaqueUpdates[MAX_FRAMES_IN_FLIGHT] = {};

    std::span<descriptorUpdateData> computeUpdates[MAX_FRAMES_IN_FLIGHT] = {};

    std::span<std::span<descriptorUpdateData>> shadowUpdates[MAX_FRAMES_IN_FLIGHT] = {};

#pragma endregion

#pragma region deletionQueue

    

    VmaAllocation validateVMADeleteableResource(deleteableResource d);
    void runDeletionQueue(std::span<deleteableResource> queue);

    Array<deleteableResource> deletionQueue;
#pragma endregion;


    struct per_frame_data
    {
        //Below is all vulkan stuff

        rendererSemaphores semaphores;
        
        VkFence inFlightFence {};

        rendererCommandBuffers commanderBuffers;


#pragma region buffers

        std::vector<dataBufferObject<ShaderGlobals>> perLightShadowShaderGlobalsBuffer;
       
        
        dataBufferObject<ShaderGlobals> opaqueShaderGlobalsBuffer;

      
  


        dataBufferObject<glm::vec4> verts;
        dataBufferObject<uint32_t> indices;
        
        dataBufferObject<UniformBufferObject> uniformBuffers;
        dataBufferObject<gpuvertex> meshBuffers;
        //Basic data about the light used in all passes 
        dataBufferObject<gpulight> lightBuffers;
        dataBufferObject<gpuPerShadowData> shadowDataBuffers;
        //Draw indirect
        uint32_t currentDrawOffset = 0;
        dataBufferObject<drawCommandData> drawBuffers;
        //Compute culling for draw indirect 
        dataBufferObject<glm::vec4> frustumsForCullBuffers;
#pragma endregion
    };
    std::vector<per_frame_data> FramesInFlightData;
    bool firstTime[MAX_FRAMES_IN_FLIGHT];

    
    

    int cubemaplut_utilitytexture_index;

    uint32_t currentFrame = 0;

#ifdef NDEBUG
        const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    
    void initializeWindow();


    PFN_vkCopyImageToMemoryEXT vkCopyImageToMemoryEXT;

    void updateLightBuffers(uint32_t currentImage);
    void populateMeshBuffers();
    void createUniformBuffers(int objectsCount, int lightCount);

    void updateUniformBuffer(uint32_t currentImage, glm::mat4 model);
    //Globals per pass, ubos, and lights are updated every frame
    void updatePerFrameBuffers(unsigned currentFrame, Array<std::span<glm::mat4>> models, Scene* scene);
    void recordCommandBufferShadowPass(VkCommandBuffer commandBuffer, uint32_t imageIndex, std::span<simplePassInfo> passes);


    void createDescriptorSetLayout(VkDescriptorSetLayoutCreateInfo layoutinfo, VkDescriptorSetLayout* layout);


    void compileShaders();

 
    void recordCommandBufferCompute(VkCommandBuffer commandBuffer, uint32_t currentFrame, std::span<simplePassInfo> passes);
    void submitComputePass(uint32_t currentFrame, uint32_t imageIndex, semaphoreData waitSemaphores,
                           std::vector<VkSemaphore> signalsemaphores, std::span<simplePassInfo> passes);
    void recordCommandBufferOpaquePass(Scene* scene, VkCommandBuffer commandBuffer, uint32_t imageIndex, std::span<opaquePassInfo> batchedDraws);

    void createGraphicsCommandPool();
    void createTransferCommandPool();


    bool hasStencilComponent(VkFormat format);


    void createGraphicsPipeline(const char* shaderName,
                                PipelineDataObject* descriptorsetdata, PipelineDataObject::graphicsPipelineSettings settings, bool compute, size_t pconstantsize);
    

    int firstframe = true;
    void createInstance();

    void drawFrame(Scene* scene);


    void renderShadowPass(uint32_t currentFrame, uint32_t imageIndex, semaphoreData waitSemaphores,
                          std::vector<VkSemaphore> signalsemaphores, std::span<simplePassInfo> passes);
    void renderOpaquePass(uint32_t currentFrame, uint32_t imageIndex, semaphoreData waitSemaphores, std::vector<VkSemaphore>
                          signalsemaphores, std::span<opaquePassInfo> batchedDraws, Scene* scene);



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