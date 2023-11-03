#include "MeshData.h"
#include "vulkan-tutorial.h"
#define TINYOBJLOADER_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <filesystem>
#include <iostream>
#include "mikktspace.h"
#include "weldmesh.h"
#include "tiny_obj_loader.h"
#include <vector>
#include <unordered_map>

#include "tinygltf/tiny_gltf.h"

struct MeshForMikkt
{
public:
   
    std::vector<glm::vec3> pos;
    std::vector<uint32_t> idx;
    std::vector<glm::vec3> norm;
    std::vector<glm::vec4> tan;
    std::vector<glm::vec3> uv;
    MeshForMikkt(std::vector<Vertex> verts, std::vector<uint32_t> indices)
    {
        idx = indices;
        //expand verts out
        if (verts.size() < indices.size())
        {
            std::vector<Vertex> _verts;
            _verts.resize(indices.size());
            for(int i = 0; i < indices.size(); i++)
            {
                _verts[i] = verts[indices[i]];
            }
            verts = _verts;
        }
        
        pos.resize(verts.size());
        norm.resize(verts.size());
        tan.resize(verts.size());
        uv.resize(verts.size());
        for(int __i = 0; __i < verts.size(); __i++)
        {
            pos[__i] = {verts[__i].pos.x, verts[__i].pos.y, verts[__i].pos.z};
            norm[__i] = {verts[__i].normal.x, verts[__i].normal.y, verts[__i].normal.z};
            uv[__i] = {verts[__i].texCoord.x, verts[__i].texCoord.y, verts[__i].texCoord.z};
        }
        }
    
};
#pragma region mikkt
struct MikktImpl
{

    MikktImpl();
    void calculateTangents(MeshForMikkt* mesh);
    

private:
    SMikkTSpaceInterface interace{};
    SMikkTSpaceContext context{};
    static int face_count(const SMikkTSpaceContext *context);
    static int faceverts(const SMikkTSpaceContext *context, int iFace);
    static void vertpos(const SMikkTSpaceContext *context, float outpos[],
                        int iFace, int iVert);

    static void norm(const SMikkTSpaceContext *context, float outnormal[],
                     int iFace, int iVert);

    static void getUV(const SMikkTSpaceContext *context, float outuv[],
                   int iFace, int iVert);

    static void set_tspace_basic(const SMikkTSpaceContext *context,
                                 const float tangentu[],
                                 float fSign, int iFace, int iVert);
};


int MikktImpl::face_count(const SMikkTSpaceContext *context)
{
    MeshForMikkt* mesh = static_cast<MeshForMikkt*> (context->m_pUserData);

    return (mesh->idx.size() / 3);
}
int MikktImpl::faceverts(const SMikkTSpaceContext *context, int iFace)
{
    return 3;
}
void MikktImpl::vertpos(const SMikkTSpaceContext *context, float outpos[],
                        int iFace, int iVert)
{
    MeshForMikkt* mesh = static_cast<MeshForMikkt*> (context->m_pUserData);
    int idx = (iFace * 3) + iVert;
    outpos[0] = mesh->pos[idx].x;
    outpos[1] = mesh->pos[idx].y;
    outpos[2] = mesh->pos[idx].z;
    
}

void MikktImpl::norm(const SMikkTSpaceContext *context, float outnormal[],
                     int iFace, int iVert)
{
    MeshForMikkt* mesh = static_cast<MeshForMikkt*> (context->m_pUserData);
    int idx = (iFace * 3) + iVert;
    outnormal[0] = mesh->norm[idx].x;
    outnormal[1] = mesh->norm[idx].y;
    outnormal[2] = mesh->norm[idx].z;
}

void MikktImpl::getUV(const SMikkTSpaceContext *context, float outuv[],
              int iFace, int iVert)
{
    MeshForMikkt* mesh = static_cast<MeshForMikkt*> (context->m_pUserData);
    int idx = (iFace * 3) + iVert;
    outuv[0] = (mesh->uv[idx].x);
    outuv[1] = (mesh->uv[idx].y);
}

void MikktImpl::set_tspace_basic(const SMikkTSpaceContext *context,
                            const float tangentu[],
                            float fSign, int iFace, int iVert)
{
    MeshForMikkt* mesh = static_cast<MeshForMikkt*> (context->m_pUserData);
    int idx = (iFace * 3) + iVert;
    mesh->tan[idx] = {tangentu[0],tangentu[1],tangentu[2], fSign};
}

MikktImpl::MikktImpl() {
    interace.m_getNumFaces = face_count;
    interace.m_getNumVerticesOfFace = faceverts;

    interace.m_getNormal = norm;
    interace.m_getPosition = vertpos;
    interace.m_getTexCoord = getUV;
    interace.m_setTSpaceBasic = set_tspace_basic;

    context.m_pInterface = &interace;
}

void MikktImpl::calculateTangents(MeshForMikkt* mesh) {

    context.m_pUserData = mesh;

    genTangSpaceDefault(&this->context);
}


#pragma endreion
int MESHID =0;
MeshData::MeshData(HelloTriangleApplication* app, std::vector<Vertex> vertices,
                   std::vector<uint32_t> indices)
{
    device = app->device;
    this->vertices = vertices;
    this->indices = indices;
    vertBuffer  = this->meshDataCreateVertexBuffer(app);
    indexBuffer = this->meshDataCreateIndexBuffer(app);
    this->vertcount = indices.size();
    this->id = MESHID++;
}

MeshData::MeshData(HelloTriangleApplication* app, std::string path)
{
    auto _path = std::filesystem::path(path);
    auto ext = _path.extension();
    bool tangentsLoaded = false;

    std::vector<Vertex> _vertices;
    std::vector<uint32_t> _indices;

    
    if (ext.string() == ".glb")
    {
        //
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.c_str());
        if (!warn.empty()) {
            printf("Warn: %s\n", warn.c_str());
        }

        if (model.meshes.size() > 1)
        {
            std::cout << "NOT IMPLEMENTED -- DON'T SUPPORT MULTIPLE MESHES";
            std::exit(-1);
        }
        
        for(const auto mesh : model.meshes)
        {
            for( auto prim : mesh.primitives)
            {
                Vertex vert;
                std::vector<glm::vec4> positionvec;
                std::vector<glm::vec4> normalvec;
                std::vector<glm::vec4> uvvec;
                std::vector<glm::vec4> colorvec;
                std::vector<glm::vec4> tangentvec;
                std::vector<uint32_t> indexvec;
                tinygltf::Accessor& accessor = model.accessors[prim.attributes["POSITION"]];
                tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                const float* positions = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                for (size_t i = 0; i < accessor.count; ++i)
                {
                    glm::vec4 position;
                    position.x =  positions[i * 3 + 0];
                    position.y =  positions[i * 3 + 1];
                    position.z =  positions[i * 3 + 2];
                    positionvec.push_back(position);
                    
                }

                if (!prim.attributes.contains("NORMAL"))
                {
                    std::cout << "NOT IMPLEMENTED -- DONT WORK SUPPORT MODELS WITHOUT NORMALS";
                    std::exit(-1);
                }


               accessor = model.accessors[prim.attributes[std::string("NORMAL")]];
               bufferView = model.bufferViews[accessor.bufferView];
               buffer = model.buffers[bufferView.buffer];
               const float* normals = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                for (size_t i = 0; i < accessor.count; ++i)
                {
                    glm::vec4 normal;
                    normal.x =  normals[i * 3 + 0];
                    normal.y =  normals[i * 3 + 1];
                    normal.z =  normals[i * 3 + 2];
                    normalvec.push_back(normal);
                    
                }
                accessor = model.accessors[prim.attributes[std::string("TEXCOORD_0")]];
                bufferView = model.bufferViews[accessor.bufferView];
                buffer = model.buffers[bufferView.buffer];
                const float* uvs = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                for (size_t i = 0; i < accessor.count; ++i)
                {
                    glm::vec4 uv;
                    uv.x =  uvs[i * 2 + 0];
                    uv.y =  uvs[i * 2 + 1];
                    uvvec.push_back(uv);
                    
                }

                accessor = model.accessors[prim.attributes[std::string("COLOR_0")]];
                bufferView = model.bufferViews[accessor.bufferView];
                buffer = model.buffers[bufferView.buffer];
                const float* colors = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                for (size_t i = 0; i < accessor.count; ++i)
                {
                    glm::vec4 color;
                    color.x =  colors[i * 3 + 0];
                    color.y =  colors[i * 3 + 1];
                    color.z =  colors[i * 3 + 2];
                    colorvec.push_back(color);
                    
                }

                //TODO JS: Not every prim
                if (prim.attributes.contains("TANGENT"))
                {
                    tangentsLoaded = true;
                    accessor = model.accessors[prim.attributes[std::string("TANGENT")]];
                    bufferView = model.bufferViews[accessor.bufferView];
                    buffer = model.buffers[bufferView.buffer];
                    const float* tangents = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                    for (size_t i = 0; i < accessor.count; ++i)
                    {
                        glm::vec4 tangent;
                        tangent.x =  tangents[i * 4 + 0];
                        tangent.y =  tangents[i * 4 + 1];
                        tangent.z =  tangents[i * 4 + 2];
                        tangent.w =  tangents[i * 4 + 3];
                        tangentvec.push_back(tangent);
                    
                    }
                }
                accessor = model.accessors[prim.indices > -1 ? prim.indices : 0];
                bufferView = model.bufferViews[accessor.bufferView];
                buffer = model.buffers[bufferView.buffer];
                const float* indexs = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                const uint8_t* indicesData = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    // Indices are stored as unsigned shorts (16 bits)
                    const uint16_t* indices16 = reinterpret_cast<const uint16_t*>(indicesData);
                    for (size_t i = 0; i < accessor.count; i++) {
                        indexvec.push_back(static_cast<uint32_t>(indices16[i]));
                    }
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    // Indices are stored as unsigned ints (32 bits)
                    const uint32_t* indices32 = reinterpret_cast<const uint32_t*>(indicesData);
                    for (size_t i = 0; i < accessor.count; i++) {
                        indexvec.push_back(indices32[i]);
                    }
                }
                for(int i = 0; i < positionvec.size(); i++)
                {
                    _vertices.push_back({positionvec[i], colorvec[i], uvvec[i], normalvec[i], tangentsLoaded? tangentvec[i] :glm::vec4(-1)});

                }
                for(int i = 0; i < indexvec.size(); i++)
                   { _indices.push_back(indexvec[i]);}
                
            }

        }

        //TODO JS - we end up deduping twice if we don't have tangents 
        std::vector<Vertex> indexedVerts;
        std::unordered_map<uint32_t, int> indicesMap;
        for(int i = 0; i < _indices.size(); i++)
        {
            
            auto it = indicesMap.find(_indices[i]);
            if (it != indicesMap.end())
            {
            indexedVerts.push_back(_vertices[_indices[i]]);
            }
        }

        
   
    }
    else if (ext.string() == ".obj")
    {
   
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
        int idx = 0;
        std::unordered_map<Vertex, uint32_t, VertexHash> unique_vertices;
        for (const auto& shape : shapes)
        {
            for (const auto& index : shape.mesh.indices)
            {
                Vertex vertex = {
                    {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2],
                        1
                    },
                    {
                        attrib.colors[3 * index.vertex_index + 0],
                        attrib.colors[3 * index.vertex_index + 1],
                        attrib.colors[3 * index.vertex_index + 2],
                        1
                    },
                    {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        attrib.texcoords[2 * index.texcoord_index + 1],
                        1,
                        1
                    },
                   {
                       attrib.normals[3 * index.normal_index + 0],
                       attrib.normals[3 * index.normal_index + 1],
                       attrib.normals[3 * index.normal_index + 2],      
                       1
                   }
                };
                

                //obj files always go through mikkt below, not bothering deduping verts
                _indices.push_back(idx++);
                _vertices.push_back(vertex);
            }
        }
        
    }
    else
    {
        std::cout << "UNSUPPORTED MODEL FORMAT: '" << ext.string() <<"' IN PATH: " << path;
        std::exit(-1);
    }

    //Generate MikkT tangents
    if (!tangentsLoaded)
    {
        std::vector<Vertex> expandedVertices;
        if (_vertices.size() == _indices.size())
        {
            expandedVertices = _vertices;
        }
        else
        {
            expandedVertices.resize(_indices.size());
            for(int i =0; i < _indices.size(); i++)
            {
                expandedVertices[i] = _vertices[_indices[i]];
            }
        }
        //
        MeshForMikkt m =  MeshForMikkt(expandedVertices, _indices);
        MikktImpl mikkt = MikktImpl();
        mikkt.calculateTangents(&m);
        for(int i = 0; i < expandedVertices.size(); i++)
        {
            expandedVertices[i].tangent = m.tan[i];
                
        }

        _vertices.clear();
        _indices.clear();

        std::unordered_map<Vertex, uint32_t, VertexHash> unique_vertices;

        for(Vertex vertex : expandedVertices)
        {
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
        
        // _indices = remapvec;
    }

    //TODO: Dedupe verts
    this->vertices = _vertices;                             
    this->indices = _indices;
    this->device = app->device;                                  
    this->vertBuffer  = this->meshDataCreateVertexBuffer(app);   
    this->indexBuffer = this->meshDataCreateIndexBuffer(app);    
    this->vertcount = indices.size();                      
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
