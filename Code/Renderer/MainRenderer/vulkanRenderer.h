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
#include "rendererStructs.h"
#include "Renderer/RendererDeletionQueue.h"
// My stuff 
struct gpulight;
struct gpuvertex;
struct PerShadowData;
struct MeshData; //Forward Declaration
struct Vertex; //Forward Declaration
using VmaAllocator = struct VmaAllocator_T*;
struct SDL_Window;
//Include last //

struct commandBufferContext
{
    VkCommandBuffer commandBuffer;
    bool active;
    VkBuffer indexBuffer;
    VkPipeline boundPipeline;
};

struct rendererObjects
{
    vkb::Instance vkbInstance;
    vkb::PhysicalDevice vkbPhysicalDevice;
    vkb::Device vkbdevice;
    VmaAllocator vmaAllocator;
    VkSurfaceKHR surface; //not sure I need surface for anything except cleanup?
    vkb::Swapchain swapchain;
    //maybe move these two
};

struct framePasses
{
    std::span<simpleMeshPassInfo> shadowDraws;
    std::span<ComputeCullListInfo> computeDraws;
    std::span<simpleMeshPassInfo> opaqueDraw;
};
struct depthBiasSettng
{
    bool use = false;
    float depthBias;
    float slopeBias;
};
struct RenderPassConfig
{
    commandBufferContext* drawcommandBufferContext;
    commandBufferContext* computeCommandBufferContext;
    PipelineDataObject* pipelineData; //descriptorsetLayoutsDataShadow
    VkBuffer indexBuffer; //FramesInFlightData[currentFrame].indices._buffer.data
    VkIndexType indexBufferType; // VK_INDEX_TYPE_UINT32
    std::span<simpleMeshPassInfo> meshPasses;
    ComputeCullListInfo* computeCullingInfo;
    VkRenderingAttachmentInfoKHR depthAttatchment; // simpleRenderingAttatchment(rendererResources.shadowMapRenderingImageViews[currentFrame][shadowMapIndex], 1.0)
    VkRenderingAttachmentInfoKHR* colorattatchment; //nullptr
    VkExtent2D renderingAttatchmentExtent; //SHADOW_MAP_SIZE
    void* pushConstants;    //shadowPushConstants constants;
    //Light count, vert offset, texture index, and object data index
    //constants.shadowIndex = (glm::float32_t)shadowMapIndex;
    size_t pushConstantsSize;
    depthBiasSettng depthBiasSetting;
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

    DepthBufferInfo depthBufferInfo;
};


rendererObjects static_initializeRendererHandles(SDL_Window* sdlWin, int WIDTH,int HEIGHT);

class vulkanRenderer
{

public:
    AssetManager* AssetDataAndMemory;
    std::unordered_map<VkImageView, VkDescriptorSet> imguiRegisteredTextures;

    vulkanRenderer();
    RendererContext getFullRendererContext();
    BufferCreationContext getRendererForVkObjects();
    void initializeRendererForScene(Scene* scene);
    void Update(Scene* scene);
    void cleanup();
    void initializeDearIMGUI();
    VkDescriptorSet GetOrRegisterImguiTextureHandle(VkSampler sampler, VkImageView imageView);

private:
    
  
    std::span<RenderPassConfig> createRenderPasses(Scene* scene, AssetManager* rendererData, cameraData* camera, MemoryArena::memoryArena* allocator, std::span<std::span<
                                                   PerShadowData>> inputShadowdata,
                                                   PipelineDataObject opaquePipelineData, PipelineDataObject shadowPipelineData, PipelineDataObject computePipelineData,
                                                   commandBufferContext* shadowCommandBufferContext, commandBufferContext* opaqueCommandBufferContext, commandBufferContext
                                                   * computeCommandBufferContext, VkRenderingAttachmentInfoKHR* opaqueTarget);
    
    rendererObjects rendererVulkanObjects;
    RendererResources rendererResources;
    std::unique_ptr<CommandPoolManager> commandPoolmanager; 
    std::unique_ptr<RendererDeletionQueue> deletionQueue;
    //Memory allocators
    MemoryArena::memoryArena rendererArena{};
    MemoryArena::memoryArena perFrameArenas[MAX_FRAMES_IN_FLIGHT];
    
#pragma region SDL
    int WIDTH = 1280  * 1.5;
    int HEIGHT = 720 * 1.5;
    struct SDL_Window* _window{nullptr};
#pragma endregion
    glm::mat4 freezeView = {};
    std::span<std::span<PerShadowData>> perLightShadowData;


    
    
void updateShadowImageViews(int frame, Scene* scene);

#pragma region  descriptor sets 
    VkDescriptorPool descriptorPool;
    VkDescriptorPool imgui_descriptorPool;
    VkDescriptorSetLayout pushDescriptorSetLayout;
    VkDescriptorSetLayout perMaterialSetLayout;
    
    PipelineDataObject descriptorsetLayoutsData; 
    PipelineDataObject descriptorsetLayoutsDataShadow; 
    PipelineDataObject descriptorsetLayoutsDataCulling;

    
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


    struct per_frame_data
    {
        rendererSemaphores semaphores;
        
        VkFence inFlightFence {};
        
        rendererCommandBuffers commanderBuffers;

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
        dataBufferObject<drawCommandData> drawBuffers;

        //Compute culling for draw indirect 
        dataBufferObject<glm::vec4> frustumsForCullBuffers;

    };

    std::vector<per_frame_data> FramesInFlightData;
    bool firstTime[MAX_FRAMES_IN_FLIGHT];
    int firstframe = true;
    int cubemaplut_utilitytexture_index;

    uint32_t currentFrame = 0;

    void initializeWindow();

    void populateMeshBuffers();
    void createUniformBuffers(int objectsCount, int lightCount);

    //Globals per pass, ubos, and lights are updated every frame
    void updatePerFrameBuffers(unsigned currentFrame, Array<std::span<glm::mat4>> models, Scene* scene);
    void recordCommandBufferShadowPass(VkCommandBuffer commandBuffer, uint32_t imageIndex, std::span<simpleMeshPassInfo> passes);

    void initializeCommandBufferCulling(commandBufferContext commandBufferContext, MemoryArena::memoryArena* arena, uint32_t _currentFrame);
    void SubmitCommandBuffer(uint32_t commandbufferCt,
                             commandBufferContext* commandBufferContext, semaphoreData waitSemaphores, std::vector<VkSemaphore> signalsemaphores, VkFence
                             waitFence);
    void recordCommandBufferUtilityPass(VkCommandBuffer commandBuffer, int imageIndex);
    void recordCommandBufferOpaquePass(Scene* scene, VkCommandBuffer commandBuffer, uint32_t imageIndex, std::span<simpleMeshPassInfo>
                                       batchedDraws);


    bool hasStencilComponent(VkFormat format);


    void createGraphicsPipeline(const char* shaderName,
                                PipelineDataObject* descriptorsetdata, PipelineDataObject::graphicsPipelineSettings settings, bool compute, size_t pconstantsize);
    

  
    void drawFrame(Scene* scene);


    void renderShadowPass(uint32_t currentFrame, uint32_t imageIndex, semaphoreData waitSemaphores,
                          std::vector<VkSemaphore> signalsemaphores, std::span<simpleMeshPassInfo> passes);
    void renderOpaquePass( uint32_t commandbufferCt, VkCommandBuffer* commandBuffer, semaphoreData
                          waitSemaphores, std::vector<VkSemaphore>
                          signalsemaphores, VkFence waitFence);



};

#pragma region textures


#pragma endregion