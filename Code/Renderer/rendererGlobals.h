#pragma once
#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_GTC_quaternion

#include <glm/glm.hpp>
#include <span>
#include <string_view>

#include "PipelineDataObject.h"
#include "RendererContext.h"
#include "VulkanIncludes/forward-declarations-renderer.h"
struct RendererContext;
const static int MAX_SHADOWCASTERS = 8;
const static int CASCADE_CT = 6;
#define MAX_SHADOWMAPS (MAX_SHADOWCASTERS * 8)
const static int MAX_CAMERAS = 1;

const static int MAX_FRAMES_IN_FLIGHT = 2;
const static int SWAPCHAIN_SIZE = 3;
const static int MAX_DRAWINDIRECT_COMMANDS = 200000; //Draw commands per frmae
const static int MAX_DRAWS_PER_PIPELINE = 2000; //whatever, probably could be dynamic, will fix later 
const static int MAX_PIPELINES = 80; //whatever, probably could be dynamic, will fix later

//deletio nqueue stuff

enum class deletionType
{
    vmaBuffer,
    descriptorPool,
    Semaphore,
    Fence,
    CommandPool,
    ImageView,
    Image,
    VkMemory,
    VmaImage,
    Sampler,
    
};

    
struct deleteableResource
{
    deletionType type;
    void* handle;
    VmaAllocation vmaAllocation; // usually not defined 
};

// const static VkIndexType INDEX_BUFFER_FORMAT = VK_INDEX_TYPE_UINT32;

enum lightType
{
    LIGHT_DIR = 0,
    LIGHT_POINT = 1,
    LIGHT_SPOT = 2,
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
    // layoutInfo* layout = NULL; //TODO JS: set this at creation time
};

struct dataBuffer
{
    VkBuffer data;
    uint32_t size;
    void* mapped;

    VkDescriptorBufferInfo getBufferInfo();
    void updateMappedMemory(void* data, size_t size);
    void allocateVulkanMemory(RendererContext h, VmaAllocation* allocation, VkBufferUsageFlags usage);
};

template < typename T >
struct dataBufferObject //todo js: name 
{
    dataBuffer _buffer;
    VmaAllocation allocation;
    std::span<T> getMappedSpan();
    void updateMappedMemory(std::span<T> source);
    uint32_t count();
};

struct dataBufferAllocationsView
{
    dataBuffer _buffer;
    VmaAllocation allocation;
};


template < typename T >
VmaAllocation dataBuffer_getAllocation(dataBufferObject<T> d)
{
    return d.allocation;
}

template < typename T >
VkBuffer dataBuffer_getVKBuffer(dataBufferObject<T> d)
{
    return d._buffer.data;
}


template < typename T >
VkDescriptorBufferInfo getDescriptorBufferInfo(dataBufferObject<T> d)
{
    return d._buffer.getBufferInfo();
}
template <typename T>
std::span<T> dataBufferObject<T>::getMappedSpan()
{
    return  std::span<T>(static_cast<T*>(_buffer.mapped), _buffer.size / sizeof(T));
}

template <typename T>
void dataBufferObject<T>::updateMappedMemory(std::span<T> source)
{
    _buffer.updateMappedMemory(source.data(), source.size_bytes());
}

template <typename T>
uint32_t dataBufferObject<T>::count()
{
    return _buffer.size / sizeof(T); 
}

template<typename T> dataBufferObject<T> createDataBuffer(RendererContext* h, uint32_t size, VkBufferUsageFlags usage)
{

  dataBufferObject<T> buffer{};
  buffer._buffer.size = sizeof(T) * size;
  buffer._buffer.allocateVulkanMemory(*h, &buffer.allocation,usage);

    //add to deletion queue
   
  return buffer;
}

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
public:
    uint32_t pipelineidx;
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
    float nearPlane = 0.01f;
    float farPlane = 35.0f;

    extent extent;
    float fov = 70;
        
    glm::mat4 debug_frozen_culling_v;
    glm::mat4 debug_frozen_culling_p;
};


void registerDebugUtilsFn(PFN_vkSetDebugUtilsObjectNameEXT ptr);
void setDebugObjectName(VkDevice device, VkObjectType type, std::string name, uint64_t handle);

