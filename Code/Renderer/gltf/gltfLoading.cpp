#include "gltfLoading.h"

#include <vulkan/vulkan_core.h>

#include <Renderer/MeshData.h>
#include <General/MemoryArena.h>
#include <General/Array.h>
#include "gltf_impl.h"
#include <Renderer/TextureData.h>
#include <General/FileCaching.h>
#include <glm/gtc/type_ptr.hpp>
temporaryloadingMesh geoFromGLTFMesh(MemoryArena::memoryArena* tempArena, tinygltf::Model model, tinygltf::Mesh mesh)
{
 
    
   
    
    //get count

    //TODOS: 1- Something is relaly wrong with memory
    //TODOs: 2- Model can have partial colors -- default fill the rest -- is this cause theyre vec3 instead of vec4?
    //TODOs: 3- What's going on with tangents? Do meshes have them on some prims only? Unless we have tangents on every revert, build 
    //TODOs: 3- The vecs should be the size of the biggest prim -- need to index in to _vertices and _indices correctly from them
    uint32_t indxCt = 0;
    uint32_t vertCt = 0;
    uint32_t largest_prim = 0;
    for (auto prim : mesh.primitives)
    {
        tinygltf::Accessor& accessor = model.accessors[prim.attributes["POSITION"]];
        vertCt += (uint32_t)accessor.count;
        tinygltf::Accessor& accessor2 = model.accessors[prim.indices > -1 ? prim.indices : 0];
        indxCt += (uint32_t)accessor2.count;
        if ((uint32_t)accessor2.count > largest_prim)
        {
            largest_prim = (uint32_t)accessor2.count;
        }
    }
    

    Array _vertices = MemoryArena::AllocSpan<Vertex>(tempArena, indxCt);
    Array _indices = MemoryArena::AllocSpan<uint32_t>(tempArena, indxCt);
    Array<glm::vec4> positionvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, largest_prim));
    Array<glm::vec4> normalvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, largest_prim));
    Array<glm::vec4> uvvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, largest_prim));
    Array<glm::vec4> colorvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, largest_prim));
    Array<glm::vec4> tangentvec =Array( MemoryArena::AllocSpan<glm::vec4>(tempArena, largest_prim));
    bool tangentsLoaded = false;    

    uint32_t primIndexOffset = 0; 
       for (auto prim : mesh.primitives)
            {

           //clear out the vectors
            positionvec.ct = 0;
             normalvec.ct = 0;
             uvvec.ct = 0;
             colorvec.ct = 0;
             tangentvec.ct = 0;

                    
             
                tinygltf::Accessor& accessor = model.accessors[prim.attributes["POSITION"]];
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

                accessor = model.accessors[prim.attributes[std::string("COLOR_0")]];
               
                bufferView = model.bufferViews[accessor.bufferView];
                buffer = model.buffers[bufferView.buffer];
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
                       colorvec.push_back(color);
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
                       colorvec.push_back(color);
                   }
                   break;
               }
           default:
               assert(!"unsupported color type");
           }

                //TODO JS: Not every prim
                if (prim.attributes.contains("TANGENT"))
                {
                    // tangentsLoaded = true;
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
              
                
            }

    return {_vertices.getSpan(), _indices.getSpan(), tangentsLoaded};
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
    
    size_t meshCt = model.meshes.size();
    size_t imageCt = model.images.size();
  
    size_t matCt = model.materials.size();
    size_t nodeCt = model.nodes.size();

    std::span<MeshData> meshes = MemoryArena::AllocSpan<MeshData>(permanentArena, meshCt);
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
        temporaryloadingMesh tempMesh = geoFromGLTFMesh(tempArena, model, model.meshes[i]);
        meshes[i] = FinalizeMeshDataFromTempMesh(permanentArena, tempArena, tempMesh);
		MemoryArena::freeToCursor(tempArena);
    }

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
	    gltfNodes[i] = {node.mesh, model.meshes[node.mesh].primitives[0].material, xform, std::span(node.children)};
	}


    FileCaching::saveAssetChangedTime(gltfpath);
    
    //Not supporting, just for logging
    size_t cameraCt = model.cameras.size();
    size_t animCt = model.animations.size();
    size_t texCt = model.textures.size();
    size_t lightCt = model.lights.size();
    printf("=======GLTF LOAD \nLoaded gltf %s \nLoaded %llu nodes, %llu models, %llu materials, %llu images. Didn't load %llu cameras, %llu animations, %llu textures, and %llu lights\n", gltfpath, nodeCt, meshCt, matCt, imageCt, cameraCt, animCt, texCt, lightCt);

    
    return {meshes, textures, materials,gltfNodes};
    
}
