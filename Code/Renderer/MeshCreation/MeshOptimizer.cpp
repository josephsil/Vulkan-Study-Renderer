#include "MeshOptimizer.h"

#include <Renderer/MeshCreation/MeshData.h>
#include <General/Array.h>
#include <General/MemoryArena.h>
#include <meshoptimizer/src/meshoptimizer.h>
#include <Renderer/rendererGlobals.h>
#include <Scene/AssetManager.h>

ImportMeshData  MeshOptimizer::RunMeshOptimizer(ArenaAllocator arena, AssetManager& rendererData, preMeshletMesh input)
{
    const size_t max_vertices = MESHLET_VERTICES;
    const size_t max_triangles = MESHLET_TRIS;
    const float cone_weight = 0.0f;
   
    size_t max_meshlets = meshopt_buildMeshletsBound(input.indices.size(), max_vertices, max_triangles);
    std::vector<meshopt_Meshlet> meshlets(max_meshlets);
    std::vector<uint32_t> meshlet_vertices(max_meshlets * max_vertices);
    std::vector<uint8_t> meshlet_triangles(max_meshlets * max_triangles * 3);
    std::vector<meshopt_Bounds> bounds;
    
    size_t meshlet_count = meshopt_buildMeshlets(meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(), input.indices.data(),
        input.indices.size(), &input.vertices[0].pos.x, input.vertices.size(), sizeof(Vertex), max_vertices, max_triangles, cone_weight);
    
    const meshopt_Meshlet& last = meshlets[meshlet_count - 1];
    
    meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
    meshlet_triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
    meshlets.resize(meshlet_count);

    auto outputStorage = rendererData.RequestMeshMemory((uint32_t)meshlet_vertices.size(), (uint32_t)meshlet_triangles.size());/*todo js*/
    for (auto m : meshlets)
    {
        bounds.push_back( meshopt_computeMeshletBounds(&meshlet_vertices[m.vertex_offset], &meshlet_triangles[m.triangle_offset],
        m.triangle_count, &input.vertices[0].pos.x, input.vertices.size(), sizeof(Vertex)));
    }
    auto outputBoundsForMeshlets =  MemoryArena::AllocSpan<GPU_BoundingSphere>(arena, meshlets.size());
    auto outputVertexOffsets  =  MemoryArena::AllocSpan<uint32_t>(arena, meshlets.size());
    Array _outputIndexCounts = MemoryArena::AllocSpan<uint32_t>(arena, meshlets.size());
    for(size_t i = 0; i < meshlets.size(); i++)
    {
        //Ingest meshlet bounds
        auto boundsCorners = MemoryArena::AllocSpan<glm::vec3>(arena, 2);
        auto& _bounds = bounds[i];
     
        outputBoundsForMeshlets[i] =
            {glm::vec4(_bounds.center[0],_bounds.center[1], _bounds.center[2], 1) , _bounds.radius};

        //Read meshlet index data
        auto& meshlet = meshlets[i];
        outputVertexOffsets[i] = meshlet.vertex_offset;
        uint32_t indexCt = (meshlet.triangle_count * 3 + 3) & ~3;
        _outputIndexCounts.push_back(indexCt);
    }

    //Copy the actual geometry over to the assetmaanger buffers
    //indices
    std::copy(meshlet_triangles.begin(), meshlet_triangles.end(), outputStorage.vertIndices.begin());

    //verts
    for(int i =0; i < meshlet_vertices.size(); i++)
    {
        auto& vertex = input.vertices[meshlet_vertices[i]];
        GpuVertexFromImportVertex(vertex,  outputStorage.vertPositions[i], outputStorage.vertData[i]);
        
    }
    
    int idx = 0;
    return
            ImportMeshData{
                .vertexct = (uint32_t)meshlet_vertices.size(),
                .indexCounts = _outputIndexCounts.getSpan(),
                .meshletVertexOffsets = outputVertexOffsets,
            .meshletBounds = outputBoundsForMeshlets,
                .meshletCount = (uint32_t)outputBoundsForMeshlets.size()
        };

    
}
