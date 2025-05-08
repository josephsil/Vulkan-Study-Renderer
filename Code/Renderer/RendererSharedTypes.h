#pragma once
#include <Renderer/rendererGlobals.h>
#include <General/GLM_impl.h>
#include <Renderer/VulkanIncludes/vmaImplementation.h>
enum lightType
{
    LIGHT_DIR = 0,
    LIGHT_POINT = 1,
    LIGHT_SPOT = 2,
};

struct viewProj
{
    glm::mat4 view;
    glm::mat4 proj;
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

struct depthBiasSettng
{
    bool use = false;
    float depthBias;
    float slopeBias;
};


struct renderPassAttatchmentInfo
{
    VkRenderingAttachmentInfoKHR* colorDraw;
    VkRenderingAttachmentInfoKHR* depthDraw;
    VkExtent2D extents;
};

struct RenderTextureFormat
{
    VkFormat ColorFormat;
    VkFormat DepthFormat;
};

struct descriptorUpdateData
{
    VkDescriptorType type;
    void* ptr;
    uint32_t count = 1;
};

struct dataBuffer
{
    VkBuffer data;
    uint32_t size;
    void* mapped;

    VkDescriptorBufferInfo getBufferInfo();
};


struct dataBufferAllocationsView
{
    dataBuffer buffer;
    VmaAllocation allocation;
};


struct inputData
{
    glm::vec3 keyboardMove;
    glm::vec2 mouseRot;
};

struct Transform
{
    glm::mat4 translation;
    glm::mat4 rot;
};


struct Material
{
    uint32_t shaderGroupIndex;
    uint32_t diffuseIndex;
    uint32_t specIndex;
    uint32_t normalIndex;
    float metallic;
    float roughness;
    glm::vec3 color;
};

struct extent
{
    uint32_t width;
    uint32_t height;
};

struct cameraData
{
    glm::vec3 eyePos = glm::vec3(-4.0f, 0.4f, 1.0f);
    glm::vec2 eyeRotation = glm::vec2(55.0f, -22.0f); //yaw, pitch
    float nearPlane = 0.1f;
    float farPlane = 35.0f;

    extent extent;
    float fov = 70;

    glm::mat4 debug_frozen_culling_v;
    glm::mat4 debug_frozen_culling_p;
};
