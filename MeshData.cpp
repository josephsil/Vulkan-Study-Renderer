#include "MeshData.h"
#include "vulkan-tutorial.h"


MeshData::MeshData(HelloTriangleApplication* app, std::vector<Vertex> vertices,
           std::vector<uint32_t> indices)
{
    device = app->device;
    this->vertices = vertices;
    this->indices = indices;
    vertBuffer  = this->meshDataCreateVertexBuffer(app);
    indexBuffer = this->meshDataCreateIndexBuffer(app);
}
void MeshData::cleanup()
{
    vkDestroyBuffer(device, vertBuffer, nullptr);
    vkDestroyBuffer(device, vertBuffer, nullptr);
    vkFreeMemory(device, vertMemory, nullptr);

    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexMemory, nullptr);
}

VkBuffer MeshData::meshDataCreateVertexBuffer(HelloTriangleApplication* renderer)
{
    VkBuffer vertexBuffer;
    auto bufferSize = sizeof(this->vertices[0]) * this->vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    renderer->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           stagingBuffer, stagingBufferMemory);

    //Bind to memory buffer
    // TODO JS - Use amd memory allocator
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, this->vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    renderer->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,


                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, this->vertMemory);

    renderer->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    return vertexBuffer;
}

VkBuffer MeshData::meshDataCreateIndexBuffer(HelloTriangleApplication* renderer)
{
    VkBuffer indexBuffer;
    auto bufferSize = sizeof(this->indices[0]) * this->indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    renderer->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           stagingBuffer, stagingBufferMemory);

    //Bind to memory buffer
    // TODO JS - Use amd memory allocator
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, this->indices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    renderer->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,


                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, this->indexMemory);

    renderer->copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    return indexBuffer;
}
