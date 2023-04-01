#include "MeshData.h"
#include "vulkan-tutorial.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <iostream>

#include "tiny_obj_loader.h"
#include <vector>
#include <unordered_map>

int MESHID =0;
MeshData::MeshData(HelloTriangleApplication* app, std::vector<Vertex> vertices,
           std::vector<uint32_t> indices)
{
    device = app->device;
    this->vertices = vertices;
    this->indices = indices;
    vertBuffer  = this->meshDataCreateVertexBuffer(app);
    indexBuffer = this->meshDataCreateIndexBuffer(app);
    this->id = MESHID++;
}

MeshData::MeshData(HelloTriangleApplication* app, std::string path)
{

  
    std::vector<Vertex> _vertices;
    std::vector<uint32_t> _indices;

    tinyobj::ObjReader reader;
    tinyobj::ObjReaderConfig reader_config;

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& _ = reader.GetMaterials();

    if (!reader.ParseFromFile(path, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        exit(1);
    }
    // De-duplicate vertices
    std::unordered_map<Vertex, uint32_t, VertexHash> unique_vertices;
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
             Vertex vertex = {
                 {
                     attrib.vertices[3 * index.vertex_index + 0],
                     attrib.vertices[3 * index.vertex_index + 1],
                     attrib.vertices[3 * index.vertex_index + 2]
                 },
                 {
                     attrib.colors[3 * index.vertex_index + 0],
                     attrib.colors[3 * index.vertex_index + 1],
                     attrib.colors[3 * index.vertex_index + 2]
                 },
                 {
                     attrib.texcoords[2 * index.texcoord_index + 0],
                     attrib.texcoords[2 * index.texcoord_index + 1]
                 }
            };
            // // attrib.normals[3 * index.normal_index + 0],
            // // attrib.normals[3 * index.normal_index + 1],
            // attrib.normals[3 * index.normal_index + 2],

            auto it = unique_vertices.find(vertex);
            if (it != unique_vertices.end()) {
                // Vertex already exists, add index to index buffer
                _indices.push_back(it->second);
            } else {
                // Vertex doesn't exist, add to vertex buffer and index buffer
                uint32_t index = static_cast<uint32_t>(_vertices.size());
                unique_vertices[vertex] = index;
                _vertices.push_back(vertex);
                _indices.push_back(index);
            }
        }
    }

    std::vector<uint32_t> index_data;

 

    this->device = app->device;
    this->vertices = _vertices;
    this->indices = _indices;
    vertBuffer  = this->meshDataCreateVertexBuffer(app);
    indexBuffer = this->meshDataCreateIndexBuffer(app);
    this->id = MESHID++;

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
