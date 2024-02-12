#include "gltfLoading.h"

#include <vulkan/vulkan_core.h>

#include <Renderer/MeshData.h>
#include <General/MemoryArena.h>
#include <General/Array.h>
#include "gltf_impl.h"
#include <Renderer/TextureData.h>
#include <General/FileCaching.h>
#include <glm/gtc/type_ptr.hpp>


void loadScalarAttributeshort(Array<glm::vec4>* target, const unsigned short* _shorts, size_t count, uint8_t stride)
{
  
    for(size_t i = 0; i < count; i+= stride)
    {
        glm::vec4 color;
        color.x = _shorts[i * stride + 0];
        color.y = _shorts[i * stride + 1];
        color.z = _shorts[i * stride + 2];
        color.w = stride == 4 ? _shorts[i * stride + 3] : 1.0;
        target->push_back(color);
    }
}

void loadScalarAttributefloat(Array<glm::vec4>* target,const float* _floats, size_t count, uint8_t stride)
{
    
    for(size_t i = 0; i < count; i+= stride)
    {
        glm::vec4 color;
        color.x =  _floats[i * stride + 0];
        color.y =  _floats[i * stride + 1];
        color.z =  _floats[i * stride + 2];
        color.w = stride == 4 ? _floats[i * stride + 3] : 1.0;
        target->push_back(color);
    }
}

void loadAttributeOrDefault(Array<glm::vec4>* target, tinygltf::Model* model, tinygltf::Primitive* prim, uint32_t idxCt, std::string attribute, glm::vec4 _default)
{
    if(!prim->attributes.contains(attribute))
    {
        for(int i =0; i < idxCt; i++)
        {
            target->push_back(_default);
        }
        return;
        
    }
    tinygltf::Accessor accessor = model->accessors[prim->attributes[attribute]];

    tinygltf::BufferView& bufferView = model->bufferViews[accessor.bufferView];
    tinygltf::Buffer& buffer = model->buffers[bufferView.buffer];
    
            bufferView = model->bufferViews[accessor.bufferView];
            buffer = model->buffers[bufferView.buffer];
           uint8_t stride =  0;
           // assert (accessor.type == TINYGLTF_TYPE_VEC3);
           if (accessor.type == TINYGLTF_TYPE_SCALAR)
           {
               int countPerIDX = accessor.count / idxCt;
               assert(countPerIDX == 1 || countPerIDX == 3 || countPerIDX == 4);
               stride = countPerIDX;
               switch (accessor.componentType)
               {
               case (TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT):
                   {
                       loadScalarAttributefloat(target,reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]), accessor.count, stride );
                       break;
                   }
               case (TINYGLTF_COMPONENT_TYPE_FLOAT):
                   {
                       loadScalarAttributeshort(target,reinterpret_cast<const unsigned short*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]), accessor.count, stride );
                       break;
                   }
               default:
                   assert(!"unsupported attribute type on attribute", attribute);
               }
           }
           switch (accessor.componentType)
           {
              case (TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT):
               {
                   auto colors = reinterpret_cast<const unsigned short*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                   for (size_t i = 0; i < accessor.count; ++i)
                   {
                       glm::vec4 color;
                       color.x = colors[i * 3 + 0];
                       color.y = colors[i * 3 + 1];
                       color.z = colors[i * 3 + 2];
                       target->push_back(color);
                   }
                      break;
               }
               case (TINYGLTF_COMPONENT_TYPE_FLOAT):
               {
                   auto colors = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                   for (size_t i = 0; i < accessor.count; ++i)
                   {
                       glm::vec4 color;
                       color.x = colors[i * 3 + 0];
                       color.y = colors[i * 3 + 1];
                       color.z = colors[i * 3 + 2];
                       target->push_back(color);
                   }
                   break;
               }
           default:
               assert(!"unsupported attribute type on attribute", attribute);
           }
    
}
temporaryloadingMesh geoFromGLTFMesh(MemoryArena::memoryArena* tempArena, tinygltf::Model model, tinygltf::Primitive prim)
{
 
    
   
    
    //get count

    //TODOS: 1- Something is relaly wrong with memory <- need to scrub back the cursor after every prim
                                                    // Need a stack of cursors I guess
    //TODOS: 2- Refactor the rest of the attributes like color?
    //TODOS: 3- Pass matrix in to scene -- make sur eits transposed right or whatever. 
    uint32_t indxCt = 0;
    uint32_t vertCt = 0;

    tinygltf::Accessor& accessor = model.accessors[prim.attributes["POSITION"]];
    vertCt += (uint32_t)accessor.count;
    tinygltf::Accessor& accessor2 = model.accessors[prim.indices > -1 ? prim.indices : 0];
    indxCt += (uint32_t)accessor2.count;



    Array _vertices = MemoryArena::AllocSpan<Vertex>(tempArena, indxCt);
    Array _indices = MemoryArena::AllocSpan<uint32_t>(tempArena, indxCt);
    Array<glm::vec4> positionvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
    Array<glm::vec4> normalvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
    Array<glm::vec4> uvvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
    Array<glm::vec4> colorvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
    Array<glm::vec4> tangentvec =Array( MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
    bool tangentsLoaded = false;    

    uint32_t primIndexOffset = 0; 

                accessor = model.accessors[prim.attributes["POSITION"]];
                tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                uint32_t primIdxCount  = accessor.count;
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

                assert(prim.attributes.contains("NORMAL"), "NOT IMPLEMENTED -- DONT WORK SUPPORT MODELS WITHOUT NORMALS");
            
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

           //COLOR
           loadAttributeOrDefault(&colorvec, &model, &prim, primIdxCount, "COLOR_0", glm::vec4(1));
            
                //TODO JS: Not every prim
                if (prim.attributes.contains("TANGENT"))
                {
                    // tangentsLoaded = true;
                    accessor = model.accessors[prim.attributes[std::string("TANGENT")]];

                    assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "tanget is not float!");
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
                        _indices.push_back(indices16[i] + primIndexOffset);
                        
                    }
                }
                else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    // Indices are stored as unsigned ints (32 bits)
                    auto indices32 = reinterpret_cast<const uint32_t*>(indicesData);
                    for (size_t i = 0; i < accessor.count; i++)
                    {
                         _indices.push_back(indices32[i] + primIndexOffset);
                    }
                }
                for (int i = 0; i < positionvec.size(); i++)
                {
                    
                    _vertices.push_back({
                        positionvec[i], colorvec[i], uvvec[i], normalvec[i],
                        tangentsLoaded ? tangentvec[i] : glm::vec4(-1)
                    });
                }

           primIndexOffset += primIdxCount;

                //TODO JS: should I dedupe verts here if we aren't doing mikkt?
              
                

    return {_vertices.getSpan(), _indices.getSpan(), tangentsLoaded};
}


//TODO: Objects+Transforms from nodes
//TODO: Textures
//TODO: Cameras? Probably not
//TODO:
gltfdata GltfLoadMeshes(RendererContext handles, const char* gltfpath)
{
    MemoryArena::memoryArena loadingArena = {};
    MemoryArena::initialize(&loadingArena, 1000000 * 500); //TODO JS: right size this to the gltf size;
    
    gltfdata output = {};
    const char* ext = strrchr(gltfpath, '.');
    assert (strcmp(ext, ".glb") == 0);
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

   
    MemoryArena::memoryArena* tempArena = &loadingArena; //TODO JS; use a loading arena 
    MemoryArena::memoryArena* permanentArena = handles.arena;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, gltfpath);
    
    size_t meshCt = model.meshes.size();
    size_t imageCt = model.images.size();
  
    size_t matCt = model.materials.size();
    size_t nodeCt = model.nodes.size();

    //TODO JS: leaking some extra gltf loading data for now
    std::span<gltfMesh> meshes = MemoryArena::AllocSpan<gltfMesh>(permanentArena, meshCt);
    std::span<TextureData> textures = MemoryArena::AllocSpan<TextureData>(permanentArena, imageCt); //These are what I call textures, what vulkan calls images
    std::span<material> materials = MemoryArena::AllocSpan<material>(permanentArena, matCt); //These are what I call textures, what vulkan calls images
    std::span<gltfNode> gltfNodes = MemoryArena::AllocSpan<gltfNode>(permanentArena, nodeCt); //These are what I call textures, what vulkan calls images 
    if (!warn.empty())
    {
        printf("GLTF LOADER WARNING: %s\n", warn.data());
    }
    assert(err.empty());
    
    for(int i = 0; i < meshCt; i++)
    {
		MemoryArena::setCursor(tempArena);
        uint32_t submeshCt = model.meshes[i].primitives.size();
        std::span<MeshData> submeshes = MemoryArena::AllocSpan<MeshData>(permanentArena, submeshCt); 
        std::span<uint32_t> submeshMats = MemoryArena::AllocSpan<uint32_t>(permanentArena, submeshCt); 
        for(int j = 0; j <submeshCt; j++)
        {
            temporaryloadingMesh tempMesh = geoFromGLTFMesh(tempArena, model, model.meshes[i].primitives[j]);
            submeshes[j] = FinalizeMeshDataFromTempMesh(permanentArena, tempArena, tempMesh);
            submeshMats[j] = model.meshes[i].primitives[j].material;
            MemoryArena::freeToCursor(tempArena);
        }
        meshes[i].submeshes = submeshes;
        meshes[i].materialIndices = submeshMats;
    }

    //TODO NEXT: submesh concept, real submeshes
    //TODO NEXT: Import matrices 
    //Supporting: Textures using first tex coord, meshes with one tex coord
    //TODO support: multiple materials per mesh (prims), multiple tex coords, , lights
    //Won't do currently: any kinds of animation, cameras, samplers 
    //For models, going to go with one tex coord for now


	for (int i = 0; i < matCt; i++)
	{
	    tinygltf::Material gltfmaterial = model.materials[i];
	    materials[i] = {};
	    //TODO JS: if these don't exist they're -1 -- otherwise they're 1 indexed 
	    materials[i].diffIndex =  gltfmaterial.pbrMetallicRoughness.baseColorTexture.index -1;
	    materials[i].specIndex = gltfmaterial.pbrMetallicRoughness.metallicRoughnessTexture.index -1;
	    materials[i].normIndex =  gltfmaterial.normalTexture.index -1;
		materials[i].occlusionIndex = gltfmaterial.occlusionTexture.index;
		materials[i].baseColorFactor = {gltfmaterial.pbrMetallicRoughness.baseColorFactor[0], gltfmaterial.pbrMetallicRoughness.baseColorFactor[1], gltfmaterial.pbrMetallicRoughness.baseColorFactor[2]};
		materials[i].metallicFactor = gltfmaterial.pbrMetallicRoughness.metallicFactor;
		materials[i].roughnessFactor = gltfmaterial.pbrMetallicRoughness.roughnessFactor;
		materials[i].normalStrength = gltfmaterial.normalTexture.scale;
		materials[i].occlusionStrength = gltfmaterial.occlusionTexture.strength;
	}

    for (int i = 0; i < imageCt; i++)
    {
        tinygltf::Image image = model.images[i];
        assert( image.name.empty());
        std::string name = image.name.empty() ? std::to_string(i) : image.name;

        //TODO JS: No gaurantee pixels data is interpreted correctly -- do I need to pass in type?
        assert(image.component == 4);
        assert(image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);
        assert(image.bits == 8);
        textures[i] = TextureData(gltfpath,name.data(),  (VkFormat)VK_FORMAT_R8G8B8A8_SRGB,   VK_SAMPLER_ADDRESS_MODE_REPEAT, image.image.data(), image.width, image.height, 6,  handles);
    }

	for(int i = 0; i < nodeCt; i++)
	{
		tinygltf::Node node = model.nodes[i];
		auto childIndices = node.children;
	    glm::mat4 xform = {};
	    if(node.matrix.size() == 16)
	    {
	        xform = glm::make_mat4<double>(node.matrix.data()); //TODO JS: do i need to transpose?
	    }
	    else
	    {
	        xform = glm::mat4(1);
	    }
	    gltfNodes[i] = {node.mesh, xform, std::span(node.children)};
	}


    FileCaching::saveAssetChangedTime(gltfpath);
    
    //Not supporting, just for logging
    size_t cameraCt = model.cameras.size();
    size_t animCt = model.animations.size();
    size_t texCt = model.textures.size();
    size_t lightCt = model.lights.size();
    printf("=======GLTF LOAD \nLoaded gltf %s \nLoaded %llu nodes, %llu models, %llu materials, %llu images. Didn't load %llu cameras, %llu animations, %llu textures, and %llu lights\n", gltfpath, nodeCt, meshCt, matCt, imageCt, cameraCt, animCt, texCt, lightCt);


    MemoryArena::RELEASE(&loadingArena);
    return {meshes, textures, materials,gltfNodes};
    
}
