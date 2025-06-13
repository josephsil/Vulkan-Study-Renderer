#include "MikkTImpl.h"
#include <General/MemoryArena.h>
#include <span>
#include "Vertex.h"


MeshForMikkt::MeshForMikkt(MemoryArena::Allocator* alloc, std::span<Vertex> verts, std::span<uint32_t> indices)
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
#pragma region mikkt

int MikktImpl::face_count(const SMikkTSpaceContext* context)
{
    auto mesh = static_cast<MeshForMikkt*>(context->m_pUserData);

    return (static_cast<int>(mesh->idx.size()) / 3);
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


#pragma endregion
