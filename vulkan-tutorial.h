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
// My stuff 

struct dataBuffer;
struct MeshData; //Forward Declaration
struct Vertex; //Forward Declaration
struct ShaderLoader;
struct TextureData;
class Scene;
//Include last

class HelloTriangleApplication
{
public:
    std::unique_ptr<Scene> scene;
    float deltaTime;


    static std::vector<char> readFile(const char* filename);

    RendererHandles getHandles();
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device; //Logical device

    HelloTriangleApplication();

    //Images

    //TODO JS
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height,
                           VkCommandBuffer workingBuffer = nullptr);

    //submitting commands


    CommandPoolManager commandPoolmanager;


    struct inputData
    {
        glm::vec3 translate;
        glm::vec3 mouseRot;
    };

    glm::vec3 eyePos = glm::vec3(2.0f, 1.0f, 0.0f);
    glm::vec3 eyeEulers;

private:
    uint32_t T;
    uint32_t T2;

    PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
    VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProps{};
    struct SDL_Window* _window{nullptr};
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    //GLFWwindow* window;
    int WIDTH = 1280;
    int HEIGHT = 720;
    int fullscreenquadIDX;


    VkSwapchainKHR swapChain;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;


    std::vector<VkImage> swapChainImages;

    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    std::vector<VkImageView> swapChainImageViews;

    VkRenderPass renderPass;

    VkDescriptorSetLayout pushDescriptorSetLayout;
    VkDescriptorSetLayout perMaterialSetLayout;
    VkPipelineLayout pipelineLayout;

    void createDescriptorPool();
    void createDescriptorSets(TextureData tex);
    VkPipeline graphicsPipeline_1;
    VkPipeline graphicsPipeline_2;
    VkPipeline testSkyPipeline;

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };


    std::vector<VkFramebuffer> swapChainFramebuffers;

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };


    int cubemaplut_utilitytexture_index;


    MeshData* fullscreenQuad;

    std::vector<VkCommandBuffer> commandBuffers;


    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

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


    const int MAX_FRAMES_IN_FLIGHT = 1;
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


    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> perMaterialDescriptorSets;
    std::vector<VkDescriptorSet> pushDescriptorSets;


    std::vector<dataBuffer> shaderGlobalsBuffer;
    std::vector<VkDeviceMemory> shaderGlobalsMemory;
    std::vector<void*> shaderGlobalsMapped;
    //TODO JS: Move the uniform buffer data and fns and ? This belongs to like, a "material" 
    std::vector<dataBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    std::vector<dataBuffer> meshBuffers;
    std::vector<VkDeviceMemory> meshBuffersMemory;
    std::vector<void*> meshBuffersMapped;

    std::vector<dataBuffer> lightBuffers;
    std::vector<VkDeviceMemory> lightBuffersMemory;
    std::vector<void*> lightBuffersMapped;


    void updateLightBuffers(uint32_t currentImage);
    void populateMeshBuffers();
    void updateDescriptorSet(Scene* scene, int idx);


    void createUniformBuffers();

    std::vector<UniformBufferObject> ubos;

    void updateUniformBuffer(uint32_t currentImage, glm::mat4 model);
    void updateUniformBuffers(uint32_t currentImage, std::vector<glm::mat4> models, inputData input);


    void createDescriptorSetLayout(VkDescriptorSetLayoutCreateInfo layoutinfo, VkDescriptorSetLayout* layout);


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

    void createGraphicsCommandPool();

    void createTransferCommandPool();

    void createCommandBuffers();

    void createFramebuffers();

    void createDepthResources();


    bool hasStencilComponent(VkFormat format);


    //TODO JS Collapse this? zoux niagra stream seems to do way less 
    VkPipeline createGraphicsPipeline(const char* shaderName, VkRenderPass renderPass,
                                      VkPipelineCache pipelineCache, VkDescriptorSetLayout layout);
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
