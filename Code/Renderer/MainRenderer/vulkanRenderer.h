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
#include <functional>

#include "../CommandPoolManager.h"
#include "../gpu-data-structs.h"
#include <General/MemoryArena.h>
#include "../Shaders/ShaderLoading.h"
#include "VkBootstrap.h"
#include "PipelineManager/PipelineLayoutManager.h"
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


struct depthBiasSettng
{
    bool use = false;
    float depthBias;
    float slopeBias;
};

//TODO JS
//The idea is using this, and using objectCount in place of cullingFrustumIndex and etc, and objectCount in place of drawOffset
struct drawBatchConfig;

struct drawBatchList
{
    uint32_t drawCount;
    std::vector<drawBatchConfig> batchConfigs;
};

struct drawBatchConfig
{
    std::string debugName;
    ActiveRenderStepData* drawRenderStepContext;
    ActiveRenderStepData* computeRenderStepContext;
    PipelineLayoutHandle _NEW_pipelineLayoutGroup; //descriptorsetLayoutsDataShadow
    VkBuffer indexBuffer; //FramesInFlightData[currentFrame].indices._buffer.data
    VkIndexType indexBufferType; // VK_INDEX_TYPE_UINT32
    std::span<simpleMeshPassInfo> meshPasses;
    ComputeCullListInfo* computeCullingInfo;
    VkRenderingAttachmentInfoKHR* depthAttatchment;
    VkRenderingAttachmentInfoKHR* colorattatchment; //nullptr
    VkExtent2D renderingAttatchmentExtent; //SHADOW_MAP_SIZE
    viewProj matrices;
    void* pushConstants; //shadowPushConstants constants;
    size_t pushConstantsSize;
    depthBiasSettng depthBiasSetting;
};

struct PerSceneShadowResources
{
    std::span<std::span<VkImageView>> shadowMapRenderingImageViews;
    std::span<std::span<VkImageView>> shadowMapSamplingImageViews;
    std::span<VkSampler> shadowSamplers;
    std::span<VkImage> shadowImages;
    std::span<std::span<DepthPyramidInfo>> WIP_shadowDepthPyramidInfos; //todo js populate
    std::span<VmaAllocation> shadowMemory;
};

struct GlobalRendererResources //Buffers, images, etc, used in core rendering -- probably to break up later
{
    std::unique_ptr<ShaderLoader> shaderLoader;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkSampler depthMipSampler;
    DepthBufferInfo depthBufferInfo;
    DepthPyramidInfo depthPyramidInfo;
};


rendererObjects initializeVulkanObjects(SDL_Window* sdlWin, int WIDTH, int HEIGHT);
GlobalRendererResources static_initializeResources(rendererObjects initializedrenderer,
                                                   MemoryArena::memoryArena* allocationArena,
                                                   RendererDeletionQueue* deletionQueue,
                                                   CommandPoolManager* commandPoolmanager);
PerSceneShadowResources init_allocate_shadow_memory(rendererObjects initializedrenderer,
                                                    MemoryArena::memoryArena* allocationArena);

class vulkanRenderer
{
public:
    AssetManager* AssetDataAndMemory;
    std::unordered_map<VkImageView, VkDescriptorSet> imguiRegisteredTextures;

    vulkanRenderer();
    RendererContext getFullRendererContext();
    BufferCreationContext getPartialRendererContext();
    void initializeRendererForScene(Scene* scene);
    void Update(Scene* scene);
    void beforeFirstUpdate();
    void cleanup();
    void initializeDearIMGUI();
    VkDescriptorSet GetOrRegisterImguiTextureHandle(VkSampler sampler, VkImageView imageView);

private:
    rendererObjects rendererVulkanObjects;
    GlobalRendererResources globalResources;
    PerSceneShadowResources shadowResources;
    std::unique_ptr<CommandPoolManager> commandPoolmanager;
    std::unique_ptr<RendererDeletionQueue> deletionQueue;
    //Memory allocators
    MemoryArena::memoryArena rendererArena{};
    MemoryArena::memoryArena perFrameArenas[MAX_FRAMES_IN_FLIGHT];

#pragma region SDL
    int WIDTH = 1280 * 1.5;
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

    BindlessObjectShaderGroup opaqueObjectShaderSets;
    PipelineLayoutManager pipelineLayoutManager{};
    PipelineLayoutHandle opaqueLayoutIDX;
    PipelineLayoutHandle shadowLayoutIDX;
    PipelineLayoutHandle cullingLayoutIDX;
    PipelineLayoutHandle mipChainLayoutIDX;


    void createDescriptorSetPool(RendererContext handles, VkDescriptorPool* pool);
    //TODO 
    std::span<descriptorUpdateData> createperSceneDescriptorUpdates(uint32_t frame, int shadowCasterCount,
                                                                    MemoryArena::memoryArena* arena,
                                                                    std::span<VkDescriptorSetLayoutBinding>
                                                                    layoutBindings);
    std::span<descriptorUpdateData> createperFrameDescriptorUpdates(uint32_t frame, int shadowCasterCount,
                                                                    MemoryArena::memoryArena* arena,
                                                                    std::span<VkDescriptorSetLayoutBinding>
                                                                    layoutBindings);
    std::span<descriptorUpdateData> createShadowDescriptorUpdates(MemoryArena::memoryArena* arena, uint32_t frame,
                                                                  uint32_t shadowIndex,
                                                                  std::span<VkDescriptorSetLayoutBinding>
                                                                  layoutBindings);


    descriptorUpdates perSceneDescriptorUpdates = {};

    std::span<descriptorUpdates> perFrameDescriptorUpdates = {};

    std::span<descriptorUpdateData> computeUpdates[MAX_FRAMES_IN_FLIGHT] = {};

#pragma endregion


    struct per_frame_data
    {
        std::span<uint32_t> boundCommandBuffers;
        acquireImageSemaphore semaphores;
        std::unique_ptr<RendererDeletionQueue> deletionQueue;
        VkFence inFlightFence{};

        std::vector<HostDataBufferObject<ShaderGlobals>> perLightShadowShaderGlobalsBuffer;

        HostDataBufferObject<ShaderGlobals> opaqueShaderGlobalsBuffer;


        HostDataBufferObject<glm::vec4> hostVerts;
        dataBuffer deviceVerts;
        HostDataBufferObject<uint32_t> hostIndices;
        dataBuffer deviceIndices;
        HostDataBufferObject<UniformBufferObject> uniformBuffers;
        HostDataBufferObject<gpuvertex> hostMesh;
        dataBuffer deviceMesh;

        //Basic data about the light used in all passes 
        HostDataBufferObject<gpulight> lightBuffers;
        HostDataBufferObject<gpuPerShadowData> shadowDataBuffers;

        //Draw indirect
        HostDataBufferObject<drawCommandData> drawBuffers;

        //Compute culling for draw indirect 
        HostDataBufferObject<glm::vec4> frustumsForCullBuffers;
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
    void recordCommandBufferShadowPass(VkCommandBuffer commandBuffer, uint32_t imageIndex,
                                       std::span<simpleMeshPassInfo> passes);

    void doMipChainCompute(ActiveRenderStepData commandBufferContext, MemoryArena::memoryArena* arena,
                           VkImage dstImage, VkImageView srcView, std::span<VkImageView> pyramidviews,
                           VkSampler sampler, uint32_t _currentFrame, int
                           pyramidWidth, int pyramidHeight);
    void updateBindingsComputeCulling(ActiveRenderStepData commandBufferContext, MemoryArena::memoryArena* arena,
                                      uint32_t _currentFrame);
    void SubmitCommandBuffer(uint32_t commandbufferCt,
                             ActiveRenderStepData* commandBufferContext, semaphoreData waitSemaphores,
                             std::vector<VkSemaphore> signalsemaphores, VkFence
                             waitFence);
    void SubmitCommandBuffer(uint32_t commandbufferCt,
                             ActiveRenderStepData* commandBufferContext, VkFence
                             waitFence);
    void recordUtilityPasses(VkCommandBuffer commandBuffer, int imageIndex);
    void recordCommandBufferOpaquePass(Scene* scene, VkCommandBuffer commandBuffer, uint32_t imageIndex,
                                       std::span<simpleMeshPassInfo>
                                       batchedDraws);


    bool hasStencilComponent(VkFormat format);


    void RenderFrame(Scene* scene);


    void renderShadowPass(uint32_t currentFrame, uint32_t imageIndex, semaphoreData waitSemaphores,
                          std::vector<VkSemaphore> signalsemaphores, std::span<simpleMeshPassInfo> passes);
    void renderOpaquePass(uint32_t commandbufferCt, VkCommandBuffer* commandBuffer, semaphoreData
                          waitSemaphores, std::vector<VkSemaphore>
                          signalsemaphores, VkFence waitFence);
};



#pragma region textures


#pragma endregion
