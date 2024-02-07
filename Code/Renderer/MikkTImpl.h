#pragma once
#include <span>
#include <glm/glm.hpp>
#include "../../mikktspace.h"
struct Vertex;

namespace MemoryArena
{
    struct memoryArena;
}

struct MeshForMikkt
{
    std::span<glm::vec3> pos;
    std::span<uint32_t> idx;
    std::span<glm::vec3> norm;
    std::span<glm::vec4> tan;
    std::span<glm::vec3> uv;

    MeshForMikkt(MemoryArena::memoryArena* alloc, std::span<Vertex> verts, std::span<uint32_t> indices);
};


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
