

struct simpleMeshPassInfo
{
    uint32_t firstDraw = 0;
    uint32_t ct;
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix;
};

struct opaqueMeshPassInfo
{
    uint32_t start;
    uint32_t ct;
    glm::mat4 viewMatrix;
    VkPipeline pipeline;
    std::span<uint32_t> overrideIndices;
    
};
struct framePasses
{
    simpleMeshPassInfo hiZPrepassDraw;
    std::span<simpleMeshPassInfo> shadowDraws;
    std::span<simpleMeshPassInfo> computeDraws;
    std::span<opaqueMeshPassInfo> opaqueDraw;
};

struct  semaphoreData
{
    std::span<VkSemaphore> semaphores;
    std::span<VkPipelineStageFlags> waitStages;
};

struct DepthBufferInfo //This is like general image info -- currently only using for depth buffer/etc but need to get away from TextureData.h
{
    VkFormat format;
    VkImage image;
    VkImageView view;
    std::span<VkImageView> viewsForMips;
    VmaAllocation vmaAllocation;
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