#include "gltfLoading.h"
#include "Renderer/VulkanIncludes/Vulkan_Includes.h"

#include <Renderer/MeshCreation/meshData.h>
#include <General/MemoryArena.h>
#include <General/Array.h>
#include <General/GLM_impl.h>
#include <General/FileCaching.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>
#define TINYGLTF_NO_STB_IMAGE
#include "gltf_impl.h"
#include <Renderer/CommandPoolManager.h>
#include <Renderer/RendererContext.h>
#include <Renderer/TextureCreation/TextureData.h>

//Default tinyobj load image fn
bool LoadImageData(tinygltf::Image* image, const int image_idx, std::string* err,
                   std::string* warn, int req_width, int req_height,
                   const unsigned char* bytes, int size, void* user_data)
{
    (void)warn;

    int w = 0, h = 0, comp = 0, req_comp = 0;

    unsigned char* data = nullptr;

    // preserve_channels true: Use channels stored in the image file.
    // false: force 32-bit textures for common Vulkan compatibility. It appears
    // that some GPU drivers do not support 24-bit images for Vulkan
    req_comp = 4;
    int bits = 8;
    int pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;

    // It is possible that the image we want to load is a 16bit per channel image
    // We are going to attempt to load it as 16bit per channel, and if it worked,
    // set the image data accordingly. We are casting the returned pointer into
    // unsigned char, because we are representing "bytes". But we are updating
    // the Image metadata to signal that this image uses 2 bytes (16bits) per
    // channel:
    if (stbi_is_16_bit_from_memory(bytes, size))
    {
        data = reinterpret_cast<unsigned char*>(
            stbi_load_16_from_memory(bytes, size, &w, &h, &comp, req_comp));
        if (data)
        {
            bits = 16;
            pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
        }
    }

    // at this point, if data is still NULL, it means that the image wasn't
    // 16bit per channel, we are going to load it as a normal 8bit per channel
    // image as we used to do:
    // if image cannot be decoded, ignore parsing and keep it by its path
    // don't break in this case
    // FIXME we should only enter this function if the image is embedded. If
    // image->uri references
    // an image file, it should be left as it is. Image loading should not be
    // mandatory (to support other formats)
    if (!data) data = stbi_load_from_memory(bytes, size, &w, &h, &comp, req_comp);
    if (!data)
    {
        // NOTE: you can use `warn` instead of `err`
        if (err)
        {
            (*err) +=
                "Unknown image format. STB cannot decode image data for image[" +
                std::to_string(image_idx) + "] name = \"" + image->name + "\".\n";
        }
        return false;
    }

    if ((w < 1) || (h < 1))
    {
        stbi_image_free(data);
        if (err)
        {
            (*err) += "Invalid image data for image[" + std::to_string(image_idx) +
                "] name = \"" + image->name + "\"\n";
        }
        return false;
    }

    if (req_width > 0)
    {
        if (req_width != w)
        {
            stbi_image_free(data);
            if (err)
            {
                (*err) += "Image width mismatch for image[" +
                    std::to_string(image_idx) + "] name = \"" + image->name +
                    "\"\n";
            }
            return false;
        }
    }

    if (req_height > 0)
    {
        if (req_height != h)
        {
            stbi_image_free(data);
            if (err)
            {
                (*err) += "Image height mismatch. for image[" +
                    std::to_string(image_idx) + "] name = \"" + image->name +
                    "\"\n";
            }
            return false;
        }
    }

    if (req_comp != 0)
    {
        // loaded data has `req_comp` channels(components)
        comp = req_comp;
    }

    image->width = w;
    image->height = h;
    image->component = comp;
    image->bits = bits;
    image->pixel_type = pixel_type;
    image->image.resize(static_cast<size_t>(w * h * comp) * static_cast<size_t>(bits / 8));
    std::copy(data, data + w * h * comp * (bits / 8), image->image.begin());
    stbi_image_free(data);

    return true;
}

bool LoadImageDataNoop(tinygltf::Image* image, const int image_idx, std::string* err,
                       std::string* warn, int req_width, int req_height,
                       const unsigned char* bytes, int size, void* user_data)
{
    int w = 0, h = 0, comp = 0, req_comp = 0;
    unsigned char* data = nullptr;
    req_comp = 4;
    int bits = 8;
    return true;
}

void loadScalarAttributeshort(Array<glm::vec4>* target, const unsigned short* _shorts, size_t count, uint8_t stride)
{
    for (size_t i = 0; i < count; i += stride)
    {
        glm::vec4 color;
        color.x = _shorts[i * stride + 0];
        color.y = _shorts[i * stride + 1];
        color.z = _shorts[i * stride + 2];
        color.w = stride == 4 ? _shorts[i * stride + 3] : 1.0f;
        target->push_back(color);
    }
}

void loadScalarAttributefloat(Array<glm::vec4>* target, const float* _floats, size_t count, uint8_t stride)
{
    for (size_t i = 0; i < count; i += stride)
    {
        glm::vec4 color;
        color.x = _floats[i * stride + 0];
        color.y = _floats[i * stride + 1];
        color.z = _floats[i * stride + 2];
        color.w = stride == 4 ? _floats[i * stride + 3] : 1.0f;
        target->push_back(color);
    }
}

void loadAttributeOrDefault(Array<glm::vec4>* target, tinygltf::Model* model, tinygltf::Primitive* prim, size_t idxCt,
                            std::string attribute, glm::vec4 _default)
{
    if (!prim->attributes.contains(attribute))
    {
        for (size_t i = 0; i < idxCt; i++)
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
    uint8_t stride = 0;
    // assert (accessor.type == TINYGLTF_TYPE_VEC3);
    if (accessor.type == TINYGLTF_TYPE_SCALAR)
    {
        size_t countPerIDX = accessor.count / idxCt;
        assert(countPerIDX == 1 || countPerIDX == 3 || countPerIDX == 4);
        stride = (uint8_t)countPerIDX;
        switch (accessor.componentType)
        {
        case (TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT):
            {
                loadScalarAttributefloat(
                    target, reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]),
                    accessor.count, stride);
                break;
            }
        case (TINYGLTF_COMPONENT_TYPE_FLOAT):
            {
                loadScalarAttributeshort(
                    target,
                    reinterpret_cast<const unsigned short*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]),
                    accessor.count, stride);
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
            auto colors = reinterpret_cast<const unsigned short*>(&buffer.data[bufferView.byteOffset + accessor.
                byteOffset]);
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

temporaryloadingMesh geoFromGLTFMesh(MemoryArena::memoryArena* tempArena, tinygltf::Model* model,
                                     tinygltf::Primitive prim)
{
    //TODOS: 2- Refactor the rest of the attributes like color?

    uint32_t indxCt = 0;
    uint32_t vertCt = 0;

    tinygltf::Accessor& accessor = model->accessors[prim.attributes["POSITION"]];
    vertCt += static_cast<uint32_t>(accessor.count);
    tinygltf::Accessor& accessor2 = model->accessors[prim.indices > -1 ? prim.indices : 0];
    indxCt += static_cast<uint32_t>(accessor2.count);


    Array _vertices = MemoryArena::AllocSpan<Vertex>(tempArena, indxCt);
    Array _indices = MemoryArena::AllocSpan<uint32_t>(tempArena, indxCt);
    auto positionvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
    auto normalvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
    auto uvvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
    auto colorvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
    auto tangentvec = Array(MemoryArena::AllocSpan<glm::vec4>(tempArena, indxCt));
    bool tangentsLoaded = false;

    size_t primIndexOffset = 0;

    accessor = model->accessors[prim.attributes["POSITION"]];
    tinygltf::BufferView& bufferView = model->bufferViews[accessor.bufferView];
    tinygltf::Buffer& buffer = model->buffers[bufferView.buffer];
    size_t primIdxCount = accessor.count;
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

    accessor = model->accessors[prim.attributes[std::string("NORMAL")]];
    bufferView = model->bufferViews[accessor.bufferView];
    buffer = model->buffers[bufferView.buffer];
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
    accessor = model->accessors[prim.attributes[std::string("TEXCOORD_0")]];
    bufferView = model->bufferViews[accessor.bufferView];
    buffer = model->buffers[bufferView.buffer];
    auto uvs = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
    for (size_t i = 0; i < accessor.count; ++i)
    {
        glm::vec4 uv;
        uv.x = uvs[i * 2 + 0];
        uv.y = uvs[i * 2 + 1];
        uvvec.push_back(uv);
    }

    //COLOR
    loadAttributeOrDefault(&colorvec, model, &prim, primIdxCount, "COLOR_0", glm::vec4(1));

    //TODO JS: Not every prim
    if (prim.attributes.contains("TANGENT"))
    {
        tangentsLoaded = true;

        accessor = model->accessors[prim.attributes[std::string("TANGENT")]];

        assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "tanget is not float!");
        bufferView = model->bufferViews[accessor.bufferView];
        buffer = model->buffers[bufferView.buffer];
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
    accessor = model->accessors[prim.indices > -1 ? prim.indices : 0];
    bufferView = model->bufferViews[accessor.bufferView];
    buffer = model->buffers[bufferView.buffer];
    const uint8_t* indicesData = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
    {
        // Indices are stored as unsigned shorts (16 bits)
        auto indices16 = reinterpret_cast<const uint16_t*>(indicesData);
        for (size_t i = 0; i < accessor.count; i++)
        {
            _indices.push_back((uint32_t)indices16[i] + (uint32_t)primIndexOffset);
        }
    }
    else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
    {
        // Indices are stored as unsigned ints (32 bits)
        auto indices32 = reinterpret_cast<const uint32_t*>(indicesData);
        for (size_t i = 0; i < accessor.count; i++)
        {
            _indices.push_back((uint32_t)indices32[i] + (uint32_t)primIndexOffset);
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


//TODO: Cameras? Probably not
//TODO: More speed improvements: don't re-calculate tangents every load
gltfdata GltfLoadMeshes(RendererContext handles, const char* gltfpath)
{
    bool gltfOutOfdate = FileCaching::assetOutOfDate(gltfpath);


    MemoryArena::memoryArena loadingArena = {};
    initialize(&loadingArena, 100000 * 500); //TODO JS: right size this to the gltf size;

    gltfdata output = {};
    const char* ext = strrchr(gltfpath, '.');
    assert(strcmp(ext, ".glb") == 0);
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    if (gltfOutOfdate) loader.SetImageLoader(LoadImageData, nullptr);
    else loader.SetImageLoader(LoadImageDataNoop, nullptr);

    MemoryArena::memoryArena* tempArena = &loadingArena; //TODO JS; use a loading arena 
    MemoryArena::memoryArena* permanentArena = handles.arena;


    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, gltfpath);

    size_t meshCt = model.meshes.size();
    size_t imageCt = model.images.size();

    size_t matCt = model.materials.size();
    size_t nodeCt = model.nodes.size();

    //TODO JS: leaking some extra gltf loading data for now
    std::span<gltfMesh> meshes = MemoryArena::AllocSpan<gltfMesh>(permanentArena, meshCt);
    std::span<TextureData> textures = MemoryArena::AllocSpan<TextureData>(permanentArena, imageCt);
    //These are what I call textures, what vulkan calls images
    std::span<material> materials = MemoryArena::AllocSpan<material>(permanentArena, matCt);
    std::span<gltfNode> gltfNodes = MemoryArena::AllocSpan<gltfNode>(permanentArena, nodeCt);
    if (!warn.empty())
    {
        printf("GLTF LOADER WARNING: %s\n", warn.data());
    }
    assert(err.empty());

    for (int i = 0; i < meshCt; i++)
    {
        setCursor(tempArena);
        size_t submeshCt = model.meshes[i].primitives.size();
        std::span<MeshData> submeshes = MemoryArena::AllocSpan<MeshData>(permanentArena, submeshCt);
        std::span<uint32_t> submeshMats = MemoryArena::AllocSpan<uint32_t>(permanentArena, submeshCt);
        for (size_t j = 0; j < submeshCt; j++)
        {
            temporaryloadingMesh tempMesh = geoFromGLTFMesh(tempArena, &model, model.meshes[i].primitives[j]);

            //TODO JS: at some point move this out to run on the whole mesh, rather than submeshes 
            submeshes[j] = MeshDataCreation::FinalizeMeshDataFromTempMesh(permanentArena, tempArena, tempMesh);
            submeshMats[j] = model.meshes[i].primitives[j].material;
            assert(
                model.meshes[i].primitives[j].material != -1 && "-1 Material index (no material) not supported. TODO.");
            //TODO JS
            freeToCursor(tempArena);
        }
        meshes[i].submeshes = submeshes;
        meshes[i].materialIndices = submeshMats;
    }

    //TODO NEXT: submesh concept, real submeshes
    //TODO support: lights
    //Won't do currently: any kinds of animation, cameras, samplers 


    for (int i = 0; i < matCt; i++)
    {
        tinygltf::Material gltfmaterial = model.materials[i];
        materials[i] = {};
        //TODO JS: if these don't exist they're -1 -- otherwise they're 1 indexed 
        materials[i].diffIndex = gltfmaterial.pbrMetallicRoughness.baseColorTexture.index;
        materials[i].specIndex = gltfmaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
        materials[i].normIndex = gltfmaterial.normalTexture.index;
        materials[i].occlusionIndex = gltfmaterial.occlusionTexture.index;
        materials[i].baseColorFactor = {
            gltfmaterial.pbrMetallicRoughness.baseColorFactor[0], gltfmaterial.pbrMetallicRoughness.baseColorFactor[1],
            gltfmaterial.pbrMetallicRoughness.baseColorFactor[2]
        };
        materials[i].metallicFactor = static_cast<glm::float32_t>(gltfmaterial.pbrMetallicRoughness.metallicFactor);
        materials[i].roughnessFactor = static_cast<glm::float32_t>(gltfmaterial.pbrMetallicRoughness.roughnessFactor);
        materials[i].normalStrength =  static_cast<glm::float32_t>(gltfmaterial.normalTexture.scale);
        materials[i].occlusionStrength = static_cast<glm::float32_t>(gltfmaterial.occlusionTexture.strength);
    }

    auto textureImportCommandBuffer = handles.textureCreationcommandPoolmanager->beginSingleTimeCommands(true);
    for (int i = 0; i < imageCt; i++)
    {
        tinygltf::Image image = model.images[i];
        assert(image.name.empty());
        std::string name = image.name.empty() ? std::to_string(i) : image.name;


        //Get image name 
        auto gltfPathSpan = std::span(gltfpath, strlen(gltfpath));
        std::span<const char> textureNameSpan = std::span(name.data(), strlen(name.data()));
        size_t len = gltfPathSpan.size() + textureNameSpan.size() + 5; //4 for ".ktx" and 1 for null terminated
        auto newName = MemoryArena::AllocSpan<char>(handles.tempArena, len);
        memcpy(newName.data(), gltfPathSpan.data(), gltfPathSpan.size_bytes());
        memcpy(newName.data() + gltfPathSpan.size_bytes(), textureNameSpan.data(), textureNameSpan.size_bytes());
        memcpy(newName.data() + gltfPathSpan.size_bytes() + textureNameSpan.size_bytes(), ".ktx\0", 5 * sizeof(char));

        auto cachedImagePath = std::string_view(newName.data(), newName.size());

        bool cachetexture = true;
        //Don't regenerate ktx if image modified time is older than last ktx 
        if (FileCaching::fileExists(cachedImagePath) && !gltfOutOfdate)
        {
            cachetexture = false;
        }


        if (cachetexture)
        {
            assert(image.component == 4);
            assert(image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);
            assert(image.bits == 8);

            textures[i] = TextureCreation::CreateTextureFromArgs(TextureCreation::MakeTextureCreationArgsFromGLTFArgs(handles, cachedImagePath.data(), VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                                 image.image.data(), image.width, image.height, 6,
                                                                 &textureImportCommandBuffer, true));
        }
        else
        {
            textures[i] = TextureCreation::CreateTextureFromArgs(TextureCreation::MakeTextureCreationArgsFromCachedGLTFArgs(handles, cachedImagePath.data(), VK_SAMPLER_ADDRESS_MODE_REPEAT, &textureImportCommandBuffer));
        }
    }

    handles.textureCreationcommandPoolmanager->endSingleTimeCommands(textureImportCommandBuffer);

    for (int i = 0; i < nodeCt; i++)
    {
        auto scale = glm::vec3(1.0);
        auto rotation = glm::quat();
        auto translation = glm::vec3(0);

        tinygltf::Node node = model.nodes[i];
        std::span<int> childIndices = MemoryArena::AllocSpan<int>(permanentArena, node.children.size());
        if (node.children.size() > 0)
        {
            for (int i = 0; i < node.children.size(); i++)
            {
                childIndices[i] = node.children[i];
            }
        }
        //TODO JS: leaking some extra gltf loading data for now
        glm::mat4 xform = {};
        if (node.matrix.size() == 16)
        {
            xform = glm::make_mat4<double>(node.matrix.data()); //TODO JS: do i need to transpose?

            glm::vec3 _1;
            glm::vec4 _2;
            decompose(xform, scale, rotation, translation, _1, _2);
            rotation = conjugate(rotation);
            //TODO JS: conjugate rot?
        }
        else
        {
            if (node.translation.size() == 3) translation = glm::make_vec3(node.translation.data());
            if (node.scale.size() == 3) scale = glm::make_vec3(node.scale.data());
            if (node.rotation.size() == 4) rotation = glm::make_quat(node.rotation.data());
            //
        }
        gltfNodes[i] = {node.mesh, childIndices, scale, rotation, translation};
    }

    FileCaching::saveAssetChangedTime(gltfpath);

    //Not supporting, just for logging
    size_t cameraCt = model.cameras.size();
    size_t animCt = model.animations.size();
    size_t texCt = model.textures.size();
    size_t lightCt = model.lights.size();
    printf(
        "=======GLTF LOAD \nLoaded gltf %s \nLoaded %llu nodes, %llu models, %llu materials, %llu images. Didn't load %llu cameras, %llu animations, %llu textures, and %llu lights\n",
        gltfpath, nodeCt, meshCt, matCt, imageCt, cameraCt, animCt, texCt, lightCt);


    RELEASE(&loadingArena);
    return {meshes, textures, materials, gltfNodes};
}
