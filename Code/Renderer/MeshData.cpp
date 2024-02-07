#include "MeshData.h"
#include "MikkTImpl.h"
#include <unordered_map>
#include "RendererContext.h"
#include "../General/Array.h"
#include "bufferCreation.h"
#include "gpu-data-structs.h"
#include "../General/MemoryArena.h"
#include "../../tinygltf/tiny_gltf.h"
#include "../../MeshLibraryImplementations.h"

//TODO JS 0: Separate mesh import and mesh data

int MESHID = 0;

MeshData MeshDataFromSpans(std::span<Vertex> vertices,
                   std::span<uint32_t> indices)
{
    MeshData m = {};
    m.vertices = vertices;
    m.indices = indices;
    m.id = MESHID++;
    return m;
}

//TODO JS: not sure what the interface should be like, but playing with this idea of a temporary/scratch mesh and then a final mesh
MeshData FinalizeMesh(MemoryArena::memoryArena* outputArena, temporaryloadingMesh tempMesh)
{

    //Temp mesh should be the last thing allocated to scratch arena.
    assert(tempMesh.expectedArenaHead == tempMesh.temporaryArena->head);
    //Generate MikkT tangents
    if (!tempMesh.tangentsLoaded)
     {
        
        std::span<Vertex> expandedVertices;
         if (tempMesh.vertices.size() == tempMesh.indices.size())
         {
             expandedVertices = tempMesh.vertices;
         }
         else
         {
             expandedVertices = MemoryArena::AllocSpan<Vertex>(tempMesh.temporaryArena, tempMesh.indices.size());
             for (int i = 0; i < tempMesh.indices.size(); i++)
             {
                 assert(tempMesh.indices[i] < tempMesh.vertices.size());
                 expandedVertices[i] = tempMesh.vertices[tempMesh.indices[i]];
             }
         }
         //
         auto m = MeshForMikkt(tempMesh.temporaryArena, expandedVertices, tempMesh.indices);
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
                 tempMesh.indices[iI++] = it->second;
                 
             }
             else
             {
                 // Vertex doesn't exist, add to vertex buffer and index buffer
                 uint32_t index = vI;
                 unique_vertices[v] = index;
                 tempMesh.vertices[vI++] = v;
                 tempMesh.indices[iI++] = index;
             }
         }

        //Shrink vertices 
        assert(vI <= tempMesh.vertices.size());
        tempMesh.vertices = tempMesh.vertices.subspan(0,vI);
          
         // tempMesh.indices = remapvec;
     }

    //TODO: Dedupe verts

    MeshData m = {};
    //Copy data
    m.vertices = MemoryArena::copySpan(outputArena, tempMesh.vertices);
    m.indices = MemoryArena::copySpan(outputArena, tempMesh.indices);

   

    //Compute bounds 
    m.boundsCorners = MemoryArena::AllocSpan<glm::vec3>(outputArena, 2);
    m.boundsCorners[0] = m.vertices[0].pos;
    m.boundsCorners[1] = m.vertices[0].pos;
    
    for(int i =1; i < m.vertices.size(); i++)
    {
        glm::vec3 compPos = glm::vec3(m.vertices[i].pos);
        m.boundsCorners[0].x = glm::min<float>(m.boundsCorners[0].x, compPos.x);
        m.boundsCorners[0].y = glm::min<float>(m.boundsCorners[0].y, compPos.y);
        m.boundsCorners[0].z = glm::min<float>(m.boundsCorners[0].z, compPos.z);
        m.boundsCorners[1].x =  glm::max<float>(m.boundsCorners[1].x, compPos.x);
        m.boundsCorners[1].y = glm::max<float>(m.boundsCorners[1].y, compPos.y);
        m.boundsCorners[1].z = glm::max<float>(m.boundsCorners[1].z, compPos.z);
    }
 
   
    m.id = MESHID++;
    return m;

    
}
//TODO JS: move to gltf loader 
temporaryloadingMesh geoFromGLTFMesh(MemoryArena::memoryArena* tempArena, tinygltf::Model model, tinygltf::Mesh mesh)
{
    uint32_t indxCt = 0;
    uint32_t vertCt = 0;
    
    MemoryArena::setCursor(tempArena);
    
    //get count 
    for (auto prim : mesh.primitives)
    {
        tinygltf::Accessor& accessor = model.accessors[prim.attributes["POSITION"]];
        vertCt += accessor.count;
        tinygltf::Accessor& accessor2 = model.accessors[prim.indices > -1 ? prim.indices : 0];
        indxCt += accessor2.count;
    }
    

    Array _vertices = MemoryArena::AllocSpan<Vertex>(tempArena, vertCt);
    Array _indices = MemoryArena::AllocSpan<uint32_t>(tempArena, indxCt);

    bool tangentsLoaded = false;    

       for (auto prim : mesh.primitives)
            {
                    
                Vertex vert;
                Array<glm::vec4> positionvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
                Array<glm::vec4> normalvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
                Array<glm::vec4> uvvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
                Array<glm::vec4> colorvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
                Array<glm::vec4> tangentvec =Array( MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
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

    return {_vertices.getSpan(), _indices.getSpan(), tangentsLoaded, tempArena, tempArena->head};
}

temporaryloadingMesh geoFromObjPath(MemoryArena::memoryArena* tempArena, const char* path)
{
    MemoryArena::setCursor(tempArena);
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
    Array _vertices = Array(MemoryArena::AllocSpan<Vertex>(tempArena, ct));
    Array _indices = Array(MemoryArena::AllocSpan<uint32_t>(tempArena, ct));


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

            _indices.push_back(idx++);
            _vertices.push_back(vertex);
        }
    }
    return {_vertices.getSpan(), _indices.getSpan(), false, tempArena, tempArena->head};
}

MeshData MeshDataFromObjFile(RendererContext rendererHandles, const char* path)
{
    const char* ext = strrchr(path, '.');
    assert(strcmp(ext, ".obj") == 0);
    auto geo = geoFromObjPath(rendererHandles.perframeArena, path);
    return FinalizeMesh(rendererHandles.arena, geo);
}

MeshData MeshDataFromFile(RendererContext rendererHandles, const char* path)
{
    const char* ext = strrchr(path, '.');
    MemoryArena::memoryArena* tempArena = rendererHandles.perframeArena;

    temporaryloadingMesh geo;
    if (strcmp(ext, ".glb") == 0)
    {
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


        geo = geoFromGLTFMesh(tempArena, model, model.meshes[0]);
    }
    else if (strcmp(ext, ".obj") == 0)
    {
        geo = geoFromObjPath(tempArena, path);
    }
    else
    {
        printf("UNSUPPORTED MODEL FORMAT: '%s' IN PATH:%s", ext, path);
        exit(-1);
    }

    return FinalizeMesh(rendererHandles.arena, geo);
}

positionRadius boundingSphereFromMeshBounds(std::span<glm::vec3> boundsCorners)
{
    return { {(boundsCorners[0] +  boundsCorners[1]) / 2.0f, 0.0}, glm::distance(boundsCorners[0], boundsCorners[1]) / 2.0f};
}

void MeshData::cleanup()
{
   // VulkanMemory::DestroyBuffer(rendererHandles.allocator, vertBuffer, vertMemory);

   // VulkanMemory::DestroyBuffer(rendererHandles.allocator, indexBuffer, indexMemory);
}

//