#include "MeshOptimizer.h"

#include "meshData.h"
#include "General/Algorithms.h"
#include "General/MemoryArena.h"
#include "meshoptimizer/src/meshoptimizer.h"
#include "Renderer/rendererGlobals.h"

MeshData MeshOptimizer::RunMeshOptimizer(ArenaAllocator arena, preMeshletMesh input)
{
    const size_t max_vertices = MESHLET_VERTICES;
    const size_t max_triangles = MESHLET_TRIS;
    const float cone_weight = 0.0f;

    size_t max_meshlets = meshopt_buildMeshletsBound(input.indices.size(), max_vertices, max_triangles);
    std::vector<meshopt_Meshlet> meshlets(max_meshlets);
    std::vector<uint32_t> meshlet_vertices(max_meshlets * max_vertices);
    std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);
    std::vector<meshopt_Bounds> bounds;
    
    size_t meshlet_count = meshopt_buildMeshlets(meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(), input.indices.data(),
        input.indices.size(), &input.vertices[0].pos.x, input.vertices.size(), sizeof(Vertex), max_vertices, max_triangles, cone_weight);
    
    const meshopt_Meshlet& last = meshlets[meshlet_count - 1];
    
    meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
    meshlet_triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
    meshlets.resize(meshlet_count);
    
    for (auto m : meshlets)
    {
        bounds.push_back( meshopt_computeMeshletBounds(&meshlet_vertices[m.vertex_offset], &meshlet_triangles[m.triangle_offset],
        m.triangle_count, &input.vertices[0].pos.x, input.vertices.size(), sizeof(Vertex)));
    }
    auto outputVertices = MemoryArena::AllocSpan<Vertex>(arena, meshlet_vertices.size());
    auto outputIndicesForMeshlets =  MemoryArena::AllocSpan<std::span<uint8_t>>(arena, meshlets.size());
    auto outputBoundsForMeshlets =  MemoryArena::AllocSpan<std::span<glm::vec3>>(arena, meshlets.size());
    auto outputIndices = MemoryArena::AllocSpan<uint8_t>(arena, meshlet_triangles.size());
    
    auto outputVertexOffsets  =  MemoryArena::AllocSpan<uint32_t>(arena, meshlets.size());
    for(size_t i = 0; i < meshlets.size(); i++)
    {
        //TODO WIP: For now meshlets (and all other submeshes) have uniue geometry
        //TODO: should probably introduce an update to the add meshes fns which lets multiple submesh index buffers share one backing vertex buffer
        auto& meshlet = meshlets[i];
        auto inputMeshletIndices_ = std::span<uint8_t>(&meshlet_triangles.at(meshlet.triangle_offset), (uint32_t)meshlet.triangle_count * 3);
        auto outputMeshletIndices =  outputIndices.subspan(meshlet.triangle_offset, inputMeshletIndices_.size());
        for(size_t i =0; i < inputMeshletIndices_.size(); i++)
        {
            outputMeshletIndices[i] = static_cast<uint8_t>(inputMeshletIndices_[i]);
        }
        auto& _bounds = bounds[i];
        auto vecCenter = glm::vec3(_bounds.center[0],_bounds.center[1], _bounds.center[2]);
        auto boundsCorners = MemoryArena::AllocSpan<glm::vec3>(arena, 2);
        boundsCorners[0] = vecCenter+ (glm::vec3(1) * (_bounds.radius / 2));
        boundsCorners[1] = vecCenter+ (glm::vec3(-1) * (_bounds.radius / 2));
        outputBoundsForMeshlets[i] = boundsCorners;
        outputIndicesForMeshlets[i] = outputMeshletIndices;
        outputVertexOffsets[i] = meshlet.vertex_offset;

    }
    CopySpanRemappedSRC(input.vertices, outputVertices, std::span(meshlet_vertices.begin(), meshlet_vertices.size()));
    // outputVertices = input.vertices;
    // FillIndicesAsdencing(outputIndices);
    return
            MeshData{
        .vertices =outputVertices,
            .meshletsIndices = outputIndicesForMeshlets,
                .meshletVertexOffsets = outputVertexOffsets,
            .meshletBounds = outputBoundsForMeshlets,
                .meshletCount = (uint32_t)outputBoundsForMeshlets.size()
        };

    
}
