#include "MeshData.h"
#include <Renderer/MikkTImpl.h>
#include <unordered_map>
#include <Renderer/PerThreadRenderContext.h>
#include <General/Array.h>
#include <Renderer/gpu-data-structs.h>
#include <General/MemoryArena.h>
#include <MeshLibraryImplementations.h>

//TODO JS 0: Separate mesh import and mesh data

int MESHID = 0;


InterchangeMesh MeshDataCreation::MeshDataFromSpans(std::span<Vertex> vertices,
                           std::span<uint32_t> indices)
{
    InterchangeMesh m = {};
    m.vertices = vertices;
    m.indices = indices;
    return m;
}

//TODO JS: not sure what the interface should be like, but playing with this idea of a temporary/scratch mesh and then a final mesh
InterchangeMesh MeshDataCreation::FinalizeMeshDataFromTempMesh(MemoryArena::Allocator* outputArena, MemoryArena::Allocator* tempArena,
                                      temporaryloadingMesh tempMesh)
{
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
            expandedVertices = MemoryArena::AllocSpan<Vertex>(tempArena, tempMesh.indices.size());
            for (int i = 0; i < tempMesh.indices.size(); i++)
            {
                assert(tempMesh.indices[i] < tempMesh.vertices.size());
                expandedVertices[i] = tempMesh.vertices[tempMesh.indices[i]];
            }
        }
        //
        auto m = MeshForMikkt(tempArena, expandedVertices, tempMesh.indices);
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
        tempMesh.vertices = tempMesh.vertices.subspan(0, vI);

        // tempMesh.indices = remapvec;
    }

    //TODO: Dedupe verts

    InterchangeMesh m = {};
    //Copy data
    m.vertices = MemoryArena::AllocCopySpan(outputArena, tempMesh.vertices);
    m.indices = MemoryArena::AllocCopySpan(outputArena, tempMesh.indices);

	
    //Compute bounds 
    m.boundsCorners = MemoryArena::AllocSpan<glm::vec3>(outputArena, 2);
    m.boundsCorners[0] = m.vertices[0].pos;
    m.boundsCorners[1] = m.vertices[0].pos;

    for (int i = 1; i < m.vertices.size(); i++)
    {
        auto compPos = glm::vec3(m.vertices[i].pos);
        m.boundsCorners[0].x = glm::min<float>(m.boundsCorners[0].x, compPos.x);
        m.boundsCorners[0].y = glm::min<float>(m.boundsCorners[0].y, compPos.y);
        m.boundsCorners[0].z = glm::min<float>(m.boundsCorners[0].z, compPos.z);
        m.boundsCorners[1].x = glm::max<float>(m.boundsCorners[1].x, compPos.x);
        m.boundsCorners[1].y = glm::max<float>(m.boundsCorners[1].y, compPos.y);
        m.boundsCorners[1].z = glm::max<float>(m.boundsCorners[1].z, compPos.z);
    }


    return m;
}

temporaryloadingMesh geoFromObjPath(MemoryArena::Allocator* tempArena, const char* path)
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
    auto _vertices = Array(MemoryArena::AllocSpan<Vertex>(tempArena, ct));
    auto _indices = Array(MemoryArena::AllocSpan<uint32_t>(tempArena, ct));


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
    return {_vertices.getSpan(), _indices.getSpan(), false};
}

InterchangeMesh MeshDataCreation::MeshDataFromObjFile(PerThreadRenderContext rendererHandles, const char* path)
{
    auto cursor = MemoryArena::GetCurrentOffset(rendererHandles.tempArena);
    const char* ext = strrchr(path, '.');
    assert(strcmp(ext, ".obj") == 0);
    temporaryloadingMesh geo = geoFromObjPath(rendererHandles.tempArena, path);
    InterchangeMesh mesh = FinalizeMeshDataFromTempMesh(rendererHandles.arena, rendererHandles.tempArena, geo);
    MemoryArena::FreeToOffset(rendererHandles.tempArena,cursor);
    return mesh;
}

GPU_BoundingSphere MeshDataCreation::CreateBoundingSphereFromMeshBounds(std::span<glm::vec3> boundsCorners)
{
    return {{(boundsCorners[0] + boundsCorners[1]) / 2.0f, 0.0}, distance(boundsCorners[0], boundsCorners[1]) * 0.5f};
}
