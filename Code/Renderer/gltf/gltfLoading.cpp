#include "gltfLoading.h"
#include "../MeshData.h"
#include "../../General/MemoryArena.h"
#include "../../General/Array.h"
#include "gltf_impl.h"

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


//TODO: Objects+Transforms from nodes
//TODO: Textures
//TODO: Cameras? Probably not
//TODO:
gltfdata GltfLoadMeshes(RendererContext handles, const char* gltfpath)
{
    //TODO JS: idea here is parse out te gltf to formats I can use, then pass it to the scene somehow
    //Currently using the gltf native version of light and camera, but will probably refactor to use my own so I can pass the struct out elsewhere
    //Ideally nowhere outside of here should have a tinygltf dependency 
    gltfdata output = {};
    const char* ext = strrchr(gltfpath, '.');
    assert (strcmp(ext, ".glb") == 0);
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    MemoryArena::memoryArena* tempArena = handles.perframeArena; //TODO JS; use a loading arena 
    MemoryArena::memoryArena* permanentArena = handles.arena;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, gltfpath);
    
    int meshCt = model.meshes.size();

    std::span<MeshData> meshes = MemoryArena::AllocSpan<MeshData>(permanentArena, meshCt);
    if (!warn.empty())
    {
        printf("Warn: %s\n", warn);
    }
    
    for(int i = 0; i < meshCt; i++)
    {
        temporaryloadingMesh tempMesh = geoFromGLTFMesh(tempArena, model, model.meshes[i]);
        meshes[i] = FinalizeMeshDataFromTempMesh(permanentArena, tempMesh);
    }
    //Supporting: Textures using first tex coord, meshes with one tex coord, lights
    //TODO support: multiple materials per mesh (prims), multiple tex coords
    //Won't do currently: any kinds of animation, cameras, samplers 
    //For models, going to go with one tex coord for now
    int imageCt = model.images.size();
    int texCt = model.textures.size();
    int matCt = model.materials.size();
    int nodeCt = model.nodes.size();


    //Not supporting, just for logging
    int cameraCt = model.cameras.size();
    int animCt = model.animations.size();

    
    return {meshes, {}, {},  {}};
    
}
