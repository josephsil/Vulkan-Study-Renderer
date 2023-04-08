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
    if (ext.string() == ".glb")
    {
        //
        std::vector<Vertex> _vertices;
        std::vector<uint32_t> _indices;
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
                    uv.x =  uvs[i * 3 + 0];
                    uv.y =  uvs[i * 3 + 1];
                    uv.z =  uvs[i * 3 + 2];
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
                
                accessor = model.accessors[prim.attributes[std::string("TANGENT")]];
                bufferView = model.bufferViews[accessor.bufferView];
                buffer = model.buffers[bufferView.buffer];
                const float* tangents = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                for (size_t i = 0; i < accessor.count; ++i)
                {
                    glm::vec4 tangent;
                    tangent.x =  tangents[i * 3 + 0];
                    tangent.y =  tangents[i * 3 + 1];
                    tangent.z =  tangents[i * 3 + 2];
                    tangentvec.push_back(tangent);
                    
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
                    _vertices.push_back({positionvec[i], colorvec[i], uvvec[i], normalvec[i], tangentvec[i]});

                }
                for(int i = 0; i < indexvec.size(); i++)
                   { _indices.push_back(indexvec[i]);}
                
            }

        }

        //TODO JS
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

        this->vertices = _vertices;
        this->indices = _indices;
        
   
    }
    else if (ext.string() == ".obj")
    {
        // std::cout << "NOT IMPLEMENTED -- TANGENTS DONT WORK";
        // std::exit(-1);
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
        int idx = 0;
        std::unordered_map<Vertex, uint32_t, VertexHash> unique_vertices;
        for (const auto& shape : shapes)
        {
            for (const auto& index : shape.mesh.indices) {
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
                
                   
                   
                
                auto it = unique_vertices.find(vertex);
                if (it != unique_vertices.end()) {
                    // Vertex already exists, add index to index buffer
                    _indices.push_back(it->second);
                    _vertices.push_back(it->first);
                } else {
                    // Vertex doesn't exist, add to vertex buffer and index buffer
                    uint32_t index = static_cast<uint32_t>(_vertices.size());
                    unique_vertices[vertex] = index;
                    _vertices.push_back(vertex);
                    _indices.push_back(index);
                }
            }
        }


        //reasonably likely the mikkt thing isnt implemented right, i couldnt get normals to render correctly 
        
        MeshForMikkt m =  MeshForMikkt(_vertices, _indices);
        MikktImpl mikkt = MikktImpl();
        mikkt.calculateTangents(&m);
        for(int i = 0; i < _vertices.size(); i++)
        {
            _vertices[i].normal = glm::vec4(m.norm[i].x,m.norm[i].y,m.norm[i].z,0.0);
            _vertices[i].tangent = m.tan[i];
                
        }
            
        std::vector<uint32_t> index_data;

        std::vector<int> remappedIndices;
        remappedIndices.resize(_vertices.size());
        std::vector<float> flatMeshForWelder;
        for(int i = 0; i < _vertices.size(); i++)
        {
            flatMeshForWelder.push_back(m.pos[i].x);
            flatMeshForWelder.push_back(m.pos[i].y);
            flatMeshForWelder.push_back(m.pos[i].z);
            flatMeshForWelder.push_back(m.norm[i].x);
            flatMeshForWelder.push_back(m.norm[i].y);
            flatMeshForWelder.push_back(m.norm[i].z);
            flatMeshForWelder.push_back(m.uv[i].x);
            flatMeshForWelder.push_back(m.uv[i].y);
            flatMeshForWelder.push_back(1.0);
            flatMeshForWelder.push_back(_vertices[i].color.x);
            flatMeshForWelder.push_back(_vertices[i].color.y);
            flatMeshForWelder.push_back(_vertices[i].color.z);
            flatMeshForWelder.push_back(m.tan[i].x);
            flatMeshForWelder.push_back(m.tan[i].y);
            flatMeshForWelder.push_back(m.tan[i].z);
            flatMeshForWelder.push_back(m.tan[i].a);
        
        }
        std::vector<float> welderOutput;
        welderOutput.resize(flatMeshForWelder.size());
        int uniqueverts = WeldMesh(remappedIndices.data(),welderOutput.data(), flatMeshForWelder.data(),_vertices.size(), 16);

        for(int i = 0; i< (uniqueverts ); i++)
        {
            _vertices[i].pos.x = welderOutput[(i*16) +0]; 
            _vertices[i].pos.y = welderOutput[(i*16) +1]; 
            _vertices[i].pos.z = welderOutput[(i*16) +2]; 
            _vertices[i].normal.x = welderOutput[(i*16) +3]; 
            _vertices[i].normal.y = welderOutput[(i*16) +4]; 
            _vertices[i].normal.z = welderOutput[(i*16) +5]; 
            _vertices[i].texCoord.x = welderOutput[(i*16) +6]; 
            _vertices[i].texCoord.y = welderOutput[(i*16) +7]; 
            _vertices[i].texCoord.z = welderOutput[(i*16) +8]; 
            _vertices[i].color.x = welderOutput[(i*16) +9]; 
            _vertices[i].color.y = welderOutput[(i*16) +10]; 
            _vertices[i].color.z = welderOutput[(i*16) +11]; 
            _vertices[i].tangent.x = welderOutput[(i*16) +12]; 
            _vertices[i].tangent.y = welderOutput[(i*16) +13]; 
            _vertices[i].tangent.z = welderOutput[(i*16) +14];
            _vertices[i].tangent.a = welderOutput[(i*16) +15]; 
        
        }

        _indices.resize(remappedIndices.size());
        for (int i = 0; i < remappedIndices.size(); i++)
        {
            _indices[i] = static_cast<uint32_t>(remappedIndices[i]);
        }
    
        this->vertices = _vertices;                             
        this->indices = _indices;
    }
    else
    {
        std::cout << "UNSUPPORTED MODEL FORMAT: '" << ext.string() <<"' IN PATH: " << path;
        std::exit(-1);
    }
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
