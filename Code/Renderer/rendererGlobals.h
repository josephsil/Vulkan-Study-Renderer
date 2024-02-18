#pragma once
#include <glm/glm.hpp>
#include <span>
#include "VulkanIncludes/forward-declarations-renderer.h"
struct RendererContext;
const static int MAX_SHADOWCASTERS = 8;
const static int CASCADE_CT = 4;
#define MAX_SHADOWMAPS (MAX_SHADOWCASTERS * 8)
const static int MAX_CAMERAS = 1;

const static int MAX_FRAMES_IN_FLIGHT = 3;
const static int MAX_DRAWINDIRECT_COMMANDS = 200000; //Draw commands per frmae
const static int MAX_DRAWS_PER_PIPELINE = 2000; //whatever, probably could be dynamic, will fix later 
const static int MAX_PIPELINES = 80; //whatever, probably could be dynamic, will fix later 

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
    int backingTextureidx;
    bool metallic;
    float roughness;
};
