

struct simpleMeshPassInfo
{
    uint32_t firstIndex;
    uint32_t ct;
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix;
    VkPipeline pipeline;
    std::span<uint32_t> sortedObjectIDs;
};

struct ComputeCullListInfo 
{
    uint32_t firstDrawIndirectIndex;
    uint32_t drawCount; //always object count?
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix; //not used?
    VkPipelineLayout layout;
    uint32_t pushCosntantSize;
    void* pushConstants;
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