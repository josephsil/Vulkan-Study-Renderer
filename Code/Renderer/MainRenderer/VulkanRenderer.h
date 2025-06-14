#pragma once 
#include <General/Array.h>
#include <General/GLM_impl.h>
// #include "rendererGlobals.h"
#include <functional>

#include <General/MemoryArena.h>
#include <Renderer/PerThreadRenderContext.h>
#include <Renderer/RendererDeletionQueue.h>
#include <Renderer/VulkanBuffers/HostDataBuffer.h>
#include <Renderer/MainRenderer/rendererStructs.h>

#include <Renderer/CommandPoolManager.h>
#include <Renderer/gpu-data-structs.h>
#include <Renderer/Shaders/ShaderLoading.h>
#include <Renderer/MainRenderer/PipelineManager/PipelineLayoutManager.h>
#include <Scene/Scene.h>

#include "Scene/AssetManager.h"
// My stuff 
struct GPU_LightData;
struct GPU_VertexData;
struct GPU_perShadowData;
struct InterchangeMesh; //Forward Declaration
struct Vertex; //Forward Declaration
using VmaAllocator = struct VmaAllocator_T*;
struct SDL_Window;
//Include last //


//Main renderer initialization and update code
//Two implementation files: VulkanRenderer.cpp and VulkanRendererInitialization.cpp




struct PerSceneShadowResources
{
    //The way shadow maps are set up, different parts of the renderer need different views.
    //"shadowMapSingleLayerViews" are used primarily for shadowmap rendering (and mip chain)
    //These are one view per each shadowmap (for each cascade, cube face, etc)
    std::span<std::span<VkImageView>> shadowMapSingleLayerViews;
    //ShadowMapTextureArayViews contain one view for each shadow*caster, they are indexed by light count
    //Each view has the appropriate number of array layers for cube faces, cascades, etc
    //This is (currently) used for opaque rendering. I'd like to simplify.
    std::span<std::span<VkImageView>> shadowMapTextureArrayViews;
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
    VkSampler writeDepthMipSampler;
    VkSampler readDepthMipSampler;
    std::vector<DepthBufferInfo> depthBufferInfoPerFrame;
    std::vector<DepthPyramidInfo> depthPyramidInfoPerFrame;
};


rendererObjects initializeVulkanObjects(SDL_Window* sdlWin, int WIDTH, int HEIGHT);
GlobalRendererResources static_initializeResources(rendererObjects initializedrenderer,
                                                   MemoryArena::Allocator* allocationArena,
                                                   RendererDeletionQueue* deletionQueue,
                                                   CommandPoolManager* commandPoolmanager);
PerSceneShadowResources AllocateShadowMemory(rendererObjects initializedrenderer,
                                                    MemoryArena::Allocator* allocationArena);

class VulkanRenderer
{
public:
    AssetManager* AssetDataAndMemory;

    VulkanRenderer();
    PerThreadRenderContext GetMainRendererContext();
    BufferCreationContext getPartialRendererContext();
    void InitializePipelines(size_t shadowCasterCount);
    void InitializeRendererForScene(SceneSizeData sceneCountData);
    void Update(Scene* scene);
    void BeforeFirstUpdate();
    void Cleanup();
    void initializeDearIMGUI();
    VkDescriptorSet GetOrRegisterImguiTextureHandle(VkSampler sampler, VkImageView imageView);
    static uint32_t CalculateTotalDrawCount(Scene* scene, std::span<PerSubmeshData> submeshData);

private:
    std::unordered_map<VkImageView, VkDescriptorSet> imguiRegisteredTextures;
    struct FrameData;
    rendererObjects vulkanObjects;

    GlobalRendererResources globalResources;
    PerSceneShadowResources shadowResources;

    CommandPoolManager* commandPoolmanager;
    RendererDeletionQueue* deletionQueue;
    
    //Memory allocators
    MemoryArena::Allocator rendererArena{};
    MemoryArena::Allocator perFrameArenas[MAX_FRAMES_IN_FLIGHT];

 
    struct SDL_Window* _window{nullptr};
    glm::mat4 freezeView = {};
    std::span<std::span<GPU_perShadowData>> perLightShadowData;

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
    PipelineLayoutHandle cullDataCopyLayoutIDX;
    PipelineLayoutHandle mipChainLayoutIDX;
    
    descriptorUpdates perSceneDescriptorUpdates = {};
    std::span<descriptorUpdates> perFrameDescriptorUpdates = {};
    std::span<DescriptorUpdateData> computeUpdates[MAX_FRAMES_IN_FLIGHT] = {};

    
    std::span<FrameData> perFrameData;
    FrameData& GetFrameData(size_t frame);
    FrameData& GetFrameData();
    bool haveInitializedFrame[MAX_FRAMES_IN_FLIGHT];
    bool isFirstFrame = true;
    size_t cubemaplut_utilitytexture_index;
    uint32_t currentFrame = 0;

    void UpdateShadowImageViews(int frame, SceneSizeData lightData);
    
    void CreateDescriptorSetPool(PerThreadRenderContext handles, VkDescriptorPool* pool);
    std::span<DescriptorUpdateData> CreatePerSceneDescriptorUpdates(uint32_t frame,
                                                                    MemoryArena::Allocator* arena,
                                                                    std::span<VkDescriptorSetLayoutBinding>
                                                                    layoutBindings);
    std::span<DescriptorUpdateData> CreatePerFrameDescriptorUpdates(uint32_t frame, size_t shadowCasterCount,
                                                                    MemoryArena::Allocator* arena,
                                                                    std::span<VkDescriptorSetLayoutBinding>
                                                                    layoutBindings);


    uint32_t GetDrawCountForFrame(Scene* scene);



#pragma endregion


    std::span<RendererDeletionQueue> perFrameDeletionQueuse;
    struct FrameData
    {
        uint32_t swapChainIndex;
        std::span<uint32_t> boundCommandBuffers;
        FrameSemaphores perFrameSemaphores;
        VkFence inFlightFence{};
       
		//TODO: Less of this should be host mapped
		//Fine on modern GPUs, for now.
        HostMappedDataBuffer<GPU_ShaderGlobals> opaqueShaderGlobalsBuffer;
        HostMappedDataBuffer<glm::vec4> hostVerts;
        GpuDataBuffer deviceVerts;
        HostMappedDataBuffer<uint32_t> hostIndices;
        GpuDataBuffer deviceIndices;
        HostMappedDataBuffer<GPU_ObjectData> perMeshbuffers;
        HostMappedDataBuffer<GPU_Transform> perObjectBuffers;
        HostMappedDataBuffer<GPU_VertexData> hostMesh;
        GpuDataBuffer deviceMesh;

        //Basic data about the light used in all passes 
        HostMappedDataBuffer<GPU_LightData> lightBuffers;
        HostMappedDataBuffer<GPU_perShadowData> shadowDataBuffers;

        //Draw indirect
        HostMappedDataBuffer<drawCommandData> drawBuffers;

        //Draw early draw list
        HostMappedDataBuffer<uint32_t> earlyDrawList;

        //Compute culling for draw indirect 
        HostMappedDataBuffer<glm::vec4> frustumsForCullBuffers;


        std::span<GPU_ObjectData> ObjectDataForFrame;
        std::span<GPU_Transform> ObjectTransformsForFrame;
 
    };


    void initializeWindow();

    void PopulateMeshBuffers();
    void CreateUniformBuffers(size_t subMeshCount, size_t objectsCount, size_t lightCount);

    //Globals per pass, ubos, and lights are updated every frame
    void UpdatePerFrameBuffers(unsigned currentFrame, Array<std::span<glm::mat4>> models, Scene* scene);


    void RecordMipChainCompute(ActiveRenderStepData commandBufferContext, MemoryArena::Allocator* arena,
                               DepthPyramidInfo& pyramidInfo, VkImageView srcView);
    void updateBindingsComputeCullingPreCull(ActiveRenderStepData commandBufferContext,
                                             ArenaAllocator arena);
    void UpdateDrawCommandCopyComputeBindings(ActiveRenderStepData commandBufferContext, ArenaAllocator arena);
    void UpdateComputeCullingBindings(ActiveRenderStepData commandBufferContext, Scene* scene, MemoryArena::Allocator* arena, bool latecull);


    void RecordUtilityPasses(VkCommandBuffer commandBuffer, size_t imageIndex);


    bool hasStencilComponent(VkFormat format);


    void RenderFrame(Scene* scene);

};




#pragma region textures


#pragma endregion
