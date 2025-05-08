#pragma once 
#include <General/Array.h>
#include <General/GLM_Impl.h>
// #include "rendererGlobals.h"
#include <functional>

#include <General/MemoryArena.h>
#include <Renderer/RendererContext.h>
#include <Renderer/RendererDeletionQueue.h>
#include <Renderer/VulkanBuffers/HostDataBuffer.h>
#include <Renderer/MainRenderer/rendererStructs.h>

#include <Renderer/CommandPoolManager.h>
#include <Renderer/gpu-data-structs.h>
#include <Renderer/Shaders/ShaderLoading.h>
#include <Renderer/MainRenderer/PipelineManager/PipelineLayoutManager.h>
#include <Scene/Scene.h>
// My stuff 
struct gpulight;
struct gpuvertex;
struct PerShadowData;
struct MeshData; //Forward Declaration
struct Vertex; //Forward Declaration
using VmaAllocator = struct VmaAllocator_T*;
struct SDL_Window;
//Include last //





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

class VulkanRenderer
{
public:
    AssetManager* AssetDataAndMemory;

    VulkanRenderer();
    RendererContext getFullRendererContext();
    BufferCreationContext getPartialRendererContext();
    void initializePipelines(size_t shadowCasterCount);
    void InitializeRendererForScene(sceneCountData sceneCountData);
    void Update(Scene* scene);
    void BeforeFirstUpdate();
    void Cleanup();
    void initializeDearIMGUI();
    VkDescriptorSet GetOrRegisterImguiTextureHandle(VkSampler sampler, VkImageView imageView);

private:
    std::unordered_map<VkImageView, VkDescriptorSet> imguiRegisteredTextures;
    struct per_frame_data;
    rendererObjects rendererVulkanObjects;
    GlobalRendererResources globalResources;
    PerSceneShadowResources shadowResources;
    std::unique_ptr<CommandPoolManager> commandPoolmanager;
    std::unique_ptr<RendererDeletionQueue> deletionQueue;
    
    //Memory allocators
    MemoryArena::memoryArena rendererArena{};
    MemoryArena::memoryArena perFrameArenas[MAX_FRAMES_IN_FLIGHT];

    int WIDTH = (int)(1280 * 1.5);
    int HEIGHT = (int)(720 * 1.5);
    struct SDL_Window* _window{nullptr};
    glm::mat4 freezeView = {};
    std::span<std::span<PerShadowData>> perLightShadowData;

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
    
    descriptorUpdates perSceneDescriptorUpdates = {};
    std::span<descriptorUpdates> perFrameDescriptorUpdates = {};
    std::span<descriptorUpdateData> computeUpdates[MAX_FRAMES_IN_FLIGHT] = {};

    
    std::span<per_frame_data> FramesInFlightData;
    bool firstRunOfFrame[MAX_FRAMES_IN_FLIGHT];
    bool isFirstFrame = true;
    size_t cubemaplut_utilitytexture_index;
    uint32_t currentFrame = 0;

    void UpdateShadowImageViews(int frame, sceneCountData lightData);
    
    void createDescriptorSetPool(RendererContext handles, VkDescriptorPool* pool);
    std::span<descriptorUpdateData> CreatePerSceneDescriptorUpdates(uint32_t frame,
                                                                    MemoryArena::memoryArena* arena,
                                                                    std::span<VkDescriptorSetLayoutBinding>
                                                                    layoutBindings);
    std::span<descriptorUpdateData> CreatePerFrameDescriptorUpdates(uint32_t frame, size_t shadowCasterCount,
                                                                    MemoryArena::memoryArena* arena,
                                                                    std::span<VkDescriptorSetLayoutBinding>
                                                                    layoutBindings);
    std::span<descriptorUpdateData> CreateShadowDescriptorUpdates(MemoryArena::memoryArena* arena, uint32_t frame,
                                                                  uint32_t shadowIndex,
                                                                  std::span<VkDescriptorSetLayoutBinding>
                                                                  layoutBindings);



#pragma endregion


    struct per_frame_data
    {
        std::span<uint32_t> boundCommandBuffers;
        acquireImageSemaphore semaphores;
        std::unique_ptr<RendererDeletionQueue> deletionQueue;
        VkFence inFlightFence{};
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


    void initializeWindow();

    void PopulateMeshBuffers();
    void CreateUniformBuffers(size_t objectsCount, size_t lightCount);

    //Globals per pass, ubos, and lights are updated every frame
    void updatePerFrameBuffers(unsigned currentFrame, Array<std::span<glm::mat4>> models, Scene* scene);


    void RecordMipChainCompute(ActiveRenderStepData commandBufferContext, MemoryArena::memoryArena* arena,
                               VkImage dstImage, VkImageView srcView, std::span<VkImageView> pyramidviews,
                               VkSampler sampler, uint32_t _currentFrame, uint32_t
                               pyramidWidth, uint32_t pyramidHeight);
    void updateBindingsComputeCulling(ActiveRenderStepData commandBufferContext, MemoryArena::memoryArena* arena,
                                      uint32_t _currentFrame);


    void RecordUtilityPasses(VkCommandBuffer commandBuffer, size_t imageIndex);


    bool hasStencilComponent(VkFormat format);


    void RenderFrame(Scene* scene);

};



#pragma region textures


#pragma endregion
