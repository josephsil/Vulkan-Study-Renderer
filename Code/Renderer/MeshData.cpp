#include "MeshData.h"
#include "../../mikktspace.h"
#include <unordered_map>
#include "RendererHandles.h"
#include "../General/Array.h"
#include "bufferCreation.h"
#include "gpu-data-structs.h"
#include "../General/MemoryArena.h"
#include "../../tinygltf/tiny_gltf.h"
#include "../../MeshLibraryImplementations.h"
#include "VulkanIncludes/VulkanMemory.h"

//TODO JS 0: Separate mesh import and mesh data
struct MeshForMikkt
{
public:
    std::span<glm::vec3> pos;
    std::span<uint32_t> idx;
    std::span<glm::vec3> norm;
    std::span<glm::vec4> tan;
    std::span<glm::vec3> uv;

    MeshForMikkt(MemoryArena::memoryArena* alloc, std::span<Vertex> verts, std::span<uint32_t> indices)
    {
        
        //TODO JS: pass in temp allocator?
        pos = MemoryArena::AllocSpan<glm::vec3>(alloc, verts.size());
        norm = MemoryArena::AllocSpan<glm::vec3>(alloc, verts.size());
        tan = MemoryArena::AllocSpan<glm::vec4>(alloc, verts.size());
        uv = MemoryArena::AllocSpan<glm::vec3>(alloc, verts.size());
        idx = indices;
        for (int i = 0; i < verts.size(); i++)
        {
            pos[i] = {verts[i].pos.x, verts[i].pos.y, verts[i].pos.z};
            norm[i] = {verts[i].normal.x, verts[i].normal.y, verts[i].normal.z};
            uv[i] = {verts[i].texCoord.x, verts[i].texCoord.y, verts[i].texCoord.z};
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
    static int face_count(const SMikkTSpaceContext* context);
    static int faceverts(const SMikkTSpaceContext* context, int iFace);
    static void vertpos(const SMikkTSpaceContext* context, float outpos[],
                        int iFace, int iVert);

    static void norm(const SMikkTSpaceContext* context, float outnormal[],
                     int iFace, int iVert);

    static void getUV(const SMikkTSpaceContext* context, float outuv[],
                      int iFace, int iVert);

    static void set_tspace_basic(const SMikkTSpaceContext* context,
                                 const float tangentu[],
                                 float fSign, int iFace, int iVert);
};


int MikktImpl::face_count(const SMikkTSpaceContext* context)
{
    auto mesh = static_cast<MeshForMikkt*>(context->m_pUserData);

    return (mesh->idx.size() / 3);
}

int MikktImpl::faceverts(const SMikkTSpaceContext* context, int iFace)
{
    return 3;
}

void MikktImpl::vertpos(const SMikkTSpaceContext* context, float outpos[],
                        int iFace, int iVert)
{
    auto mesh = static_cast<MeshForMikkt*>(context->m_pUserData);
    int idx = (iFace * 3) + iVert;
    outpos[0] = mesh->pos[idx].x;
    outpos[1] = mesh->pos[idx].y;
    outpos[2] = mesh->pos[idx].z;
}

void MikktImpl::norm(const SMikkTSpaceContext* context, float outnormal[],
                     int iFace, int iVert)
{
    auto mesh = static_cast<MeshForMikkt*>(context->m_pUserData);
    int idx = (iFace * 3) + iVert;
    outnormal[0] = mesh->norm[idx].x;
    outnormal[1] = mesh->norm[idx].y;
    outnormal[2] = mesh->norm[idx].z;
}

void MikktImpl::getUV(const SMikkTSpaceContext* context, float outuv[],
                      int iFace, int iVert)
{
    auto mesh = static_cast<MeshForMikkt*>(context->m_pUserData);
    int idx = (iFace * 3) + iVert;
    outuv[0] = (mesh->uv[idx].x);
    outuv[1] = (mesh->uv[idx].y);
}

void MikktImpl::set_tspace_basic(const SMikkTSpaceContext* context,
                                 const float tangentu[],
                                 float fSign, int iFace, int iVert)
{
    auto mesh = static_cast<MeshForMikkt*>(context->m_pUserData);
    int idx = (iFace * 3) + iVert;
    mesh->tan[idx] = {tangentu[0], tangentu[1], tangentu[2], fSign};
}

MikktImpl::MikktImpl()
{
    interace.m_getNumFaces = face_count;
    interace.m_getNumVerticesOfFace = faceverts;

    interace.m_getNormal = norm;
    interace.m_getPosition = vertpos;
    interace.m_getTexCoord = getUV;
    interace.m_setTSpaceBasic = set_tspace_basic;

    context.m_pInterface = &interace;
}

void MikktImpl::calculateTangents(MeshForMikkt* mesh)
{
    context.m_pUserData = mesh;

    genTangSpaceDefault(&this->context);
}


#pragma endreion
int MESHID = 0;

MeshData::MeshData(RendererHandles rendererHandles, std::vector<Vertex> vertices,
                   std::vector<uint32_t> indices)
{
    this->rendererHandles = rendererHandles;
    this->vertices = vertices;
    this->indices = indices;
    vertBuffer = this->meshDataCreateVertexBuffer();
    indexBuffer = this->meshDataCreateIndexBuffer();
    this->vertcount = indices.size();
    this->id = MESHID++;
}

MeshData::MeshData(RendererHandles app, const char* path)
{
    const char* ext = strrchr(path, '.');
    bool tangentsLoaded = false;
    this->rendererHandles = app;
    
    MemoryArena::memoryArena* tempArena = rendererHandles.perframeArena;

    MemoryArena::setCursor(tempArena);
    
    Array<Vertex> _vertices;
    Array<uint32_t> _indices;
    if (strcmp(ext, ".glb") == 0 )
    {
        //
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
        if (!warn.empty())
        {
            printf("Warn: %s\n", warn);
        }

        if (model.meshes.size() > 1)
        {
            printf("NOT IMPLEMENTED -- DON'T SUPPORT MULTIPLE MESHES");
        exit(-1);
        }


        uint32_t indxCt = 0;
        uint32_t vertCt = 0;
        for (const auto mesh : model.meshes)
        {
            for (auto prim : mesh.primitives)
            {
                
                tinygltf::Accessor& accessor = model.accessors[prim.attributes["POSITION"]];

                vertCt += accessor.count;
                
                tinygltf::Accessor& accessor2 = model.accessors[prim.indices > -1 ? prim.indices : 0];
                indxCt += accessor2.count;
            }
        }

        _vertices = MemoryArena::AllocSpan<Vertex>(tempArena, vertCt);
        _indices = MemoryArena::AllocSpan<uint32_t>(tempArena, indxCt);
        for (const auto mesh : model.meshes)
        {
            for (auto prim : mesh.primitives)
            {
                    
                Vertex vert;
                Array<glm::vec4> positionvec = Array(MemoryArena::AllocSpan<glm::vec4>(rendererHandles.perframeArena, indxCt));
                Array<glm::vec4> normalvec = Array(MemoryArena::AllocSpan<glm::vec4>(rendererHandles.perframeArena, indxCt));
                Array<glm::vec4> uvvec = Array(MemoryArena::AllocSpan<glm::vec4>(rendererHandles.perframeArena, indxCt));
                Array<glm::vec4> colorvec = Array(MemoryArena::AllocSpan<glm::vec4>(rendererHandles.perframeArena, indxCt));
                Array<glm::vec4> tangentvec =Array( MemoryArena::AllocSpan<glm::vec4>(rendererHandles.perframeArena, indxCt));
                tinygltf::Accessor& accessor = model.accessors[prim.attributes["POSITION"]];
                tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                auto positions = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.
                    byteOffset]);

                for (size_t i = 0; i < accessor.count; ++i)
                {
                    glm::vec4 position;
                    position.x = positions[i * 3 + 0];
                    position.y = positions[i * 3 + 1];
                    position.z = positions[i * 3 + 2];
                    positionvec.push_back(position);
                }

                if (!prim.attributes.contains("NORMAL"))
                {
                   printf("NOT IMPLEMENTED -- DONT WORK SUPPORT MODELS WITHOUT NORMALS");
                exit(-1);
                }


                accessor = model.accessors[prim.attributes[std::string("NORMAL")]];
                bufferView = model.bufferViews[accessor.bufferView];
                buffer = model.buffers[bufferView.buffer];
                auto normals = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.
                    byteOffset]);
                for (size_t i = 0; i < accessor.count; ++i)
                {
                    glm::vec4 normal;
                    normal.x = normals[i * 3 + 0];
                    normal.y = normals[i * 3 + 1]; //TODO JS NORMAL MYSTERY: Are we loading these wrong?
                    normal.z = normals[i * 3 + 2]; //TODO JS NORMAL MYSTERY: Are we loading these wrong?
                    normalvec.push_back(normal);
                }
                accessor = model.accessors[prim.attributes[std::string("TEXCOORD_0")]];
                bufferView = model.bufferViews[accessor.bufferView];
                buffer = model.buffers[bufferView.buffer];
                auto uvs = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                for (size_t i = 0; i < accessor.count; ++i)
                {
                    glm::vec4 uv;
                    uv.x = uvs[i * 2 + 0];
                    uv.y = uvs[i * 2 + 1];
                    uvvec.push_back(uv);
                }

                accessor = model.accessors[prim.attributes[std::string("COLOR_0")]];
                bufferView = model.bufferViews[accessor.bufferView];
                buffer = model.buffers[bufferView.buffer];
                auto colors = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                for (size_t i = 0; i < accessor.count; ++i)
                {
                    glm::vec4 color;
                    color.x = colors[i * 3 + 0];
                    color.y = colors[i * 3 + 1];
                    color.z = colors[i * 3 + 2];
                    colorvec.push_back(color);
                }

                //TODO JS: Not every prim
                if (prim.attributes.contains("TANGENT"))
                {
                    tangentsLoaded = true;
                    accessor = model.accessors[prim.attributes[std::string("TANGENT")]];
                    bufferView = model.bufferViews[accessor.bufferView];
                    buffer = model.buffers[bufferView.buffer];
                    auto tangents = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.
                        byteOffset]);
                    for (size_t i = 0; i < accessor.count; ++i)
                    {
                        glm::vec4 tangent;
                        tangent.x = tangents[i * 4 + 0];
                        tangent.y = tangents[i * 4 + 1];
                        tangent.z = tangents[i * 4 + 2];
                        tangent.w = tangents[i * 4 + 3];
                        tangentvec.push_back(tangent);
                    }
                }
                accessor = model.accessors[prim.indices > -1 ? prim.indices : 0];
                bufferView = model.bufferViews[accessor.bufferView];
                buffer = model.buffers[bufferView.buffer];
                const uint8_t* indicesData = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    // Indices are stored as unsigned shorts (16 bits)
                    auto indices16 = reinterpret_cast<const uint16_t*>(indicesData);
                    for (size_t i = 0; i < accessor.count; i++)
                    {
                        _indices.push_back(indices16[i]);
                        
                    }
                }
                else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    // Indices are stored as unsigned ints (32 bits)
                    auto indices32 = reinterpret_cast<const uint32_t*>(indicesData);
                    for (size_t i = 0; i < accessor.count; i++)
                    {
                         _indices.push_back(indices32[i]);
                    }
                }
                for (int i = 0; i < positionvec.size(); i++)
                {
                    
                    _vertices.push_back({
                        positionvec[i], colorvec[i], uvvec[i], normalvec[i],
                        tangentsLoaded ? tangentvec[i] : glm::vec4(-1)
                    });
                }

                //TODO JS: should I dedupe verts here if we aren't doing mikkt?
              
                
            }
        }
    }

    
    else if (strcmp(ext, ".obj") == 0)
     {
         
         tinyobj::ObjReader reader;
         tinyobj::ObjReaderConfig reader_config;
         
         auto& attrib = reader.GetAttrib();
         auto& shapes = reader.GetShapes();
         auto& _ = reader.GetMaterials();
         
         if (!reader.ParseFromFile(path, reader_config))
         {
             if (!reader.Error().empty())
             {
                 printf("%s", reader.Error().c_str());
             }
             exit(1);
         }

        int ct = 0;
        for (const auto& shape : shapes)
        {
            for (const auto& index : shape.mesh.indices)
            {
                ct++;
            }
        }
        _vertices = Array(MemoryArena::AllocSpan<Vertex>(tempArena, ct));
        _indices = Array(MemoryArena::AllocSpan<uint32_t>(tempArena, ct));
         
         // De-duplicate vertices
         int idx = 0;
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
         printf("UNSUPPORTED MODEL FORMAT: '%s' IN PATH:%s",ext, path);
     exit(-1);
     }

    //Generate MikkT tangents
    if (!tangentsLoaded)
     {
        std::span<Vertex> expandedVertices;
         if (_vertices.size() == _indices.size())
         {
             expandedVertices = _vertices.getSpan();
         }
         else
         {
             expandedVertices = MemoryArena::AllocSpan<Vertex>(tempArena, _indices.size());
             for (int i = 0; i < _indices.size(); i++)
             {
                 assert(_indices[i] < _vertices.size());
                 expandedVertices[i] = _vertices[_indices[i]];
             }
         }
         //
         auto m = MeshForMikkt(tempArena, expandedVertices, _indices.getSpan());
         auto mikkt = MikktImpl();
         mikkt.calculateTangents(&m);
         for (int i = 0; i < expandedVertices.size(); i++)
         {
             expandedVertices[i].tangent = m.tan[i];
         }
        

        //TODO JS: get rid of?
         std::unordered_map<Vertex, uint32_t, VertexHash> unique_vertices;

        int vI = 0;
        int iI = 0;
         for (int i = 0; i < expandedVertices.size(); i++)
         {
             Vertex v = expandedVertices[i];
             auto it = unique_vertices.find(v);
             if (it != unique_vertices.end())
             {
                 // Vertex already exists, add index to index buffer
                 _indices[iI++] = it->second;
                 
             }
             else
             {
                 // Vertex doesn't exist, add to vertex buffer and index buffer
                 uint32_t index = vI;
                 unique_vertices[v] = index;
                 _vertices[vI++] = v;
                 _indices[iI++] = index;
             }
         }

        //Shrink vertices 
        assert(vI <= _vertices.ct);
        _vertices.ct = vI ;
          
         // _indices = remapvec;
     }

    //

    MemoryArena::memoryArena* globalArena = rendererHandles.arena;
    this->vertices = MemoryArena::copySpan(globalArena, _vertices.getSpan());
    this->indices = MemoryArena::copySpan(globalArena, _indices.getSpan());

    MemoryArena::freeToCursor(tempArena);


    this->boundsCorners = MemoryArena::AllocSpan<glm::vec3>(globalArena, 2);
    boundsCorners[0] = this->vertices[0].pos;
    boundsCorners[1] = this->vertices[0].pos;
    
    for(int i =1; i < this->vertices.size(); i++)
    {
        glm::vec3 compPos = glm::vec3(this->vertices[i].pos);
        boundsCorners[0].x = glm::min<float>(boundsCorners[0].x, compPos.x);
        boundsCorners[0].y = glm::min<float>(boundsCorners[0].y, compPos.y);
        boundsCorners[0].z = glm::min<float>(boundsCorners[0].z, compPos.z);
        boundsCorners[1].x =  glm::max<float>(boundsCorners[1].x, compPos.x);
        boundsCorners[1].y = glm::max<float>(boundsCorners[1].y, compPos.y);
        boundsCorners[1].z = glm::max<float>(boundsCorners[1].z, compPos.z);
    }
 
    //TODO: Dedupe verts
    this->vertBuffer = this->meshDataCreateVertexBuffer();
    this->indexBuffer = this->meshDataCreateIndexBuffer();
    this->vertcount = _indices.size();
    this->id = MESHID++;
}

positionRadius MeshData::getBoundingSphere()
{
    return { {(boundsCorners[0] +  boundsCorners[1]) / 2.0f, 0.0}, glm::distance(boundsCorners[0], boundsCorners[1]) / 2.0f};
}
//TODO JS 0: Separate memory ownership? give it to the renderer?
void MeshData::cleanup()
{
   VulkanMemory::DestroyBuffer(rendererHandles.allocator, vertBuffer, vertMemory);

   VulkanMemory::DestroyBuffer(rendererHandles.allocator, indexBuffer, indexMemory);
}

VkBuffer MeshData::meshDataCreateVertexBuffer()
{
    VkBuffer vertexBuffer;
    auto bufferSize = sizeof(this->vertices[0]) * this->vertices.size();

    BufferUtilities::stageVertexBuffer(rendererHandles, bufferSize, vertexBuffer, vertMemory,this->vertices.data());

    return vertexBuffer;
}

VkBuffer MeshData::meshDataCreateIndexBuffer()
{
    VkBuffer indexBuffer;
    auto bufferSize = sizeof(this->indices[0]) * this->indices.size();
    BufferUtilities::stageIndexBuffer(rendererHandles, bufferSize, indexBuffer, indexMemory,this->indices.data());
    

    return indexBuffer;
}
