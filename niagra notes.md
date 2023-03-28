Setup (not per frame)

...

Load mesh
Process for a mesh: 

1- get verts/indices (duh) 
2- You need buffer(s) 
    Buffer needs VKBuffer and VkDeviceMemory
    3- To set up a buffer, 
    you need CreateInfo
    you need to call VkBuffer
    You need to figure out memory requirements
    You need to allocate memory 
    You need to Bind memory
            If you're binding multiple buffers to one set of memory, you need to supply offset
    You need to Map Memory for a cpu visible pointer to the memory

    Freeing requires VKFreeMemory
        Memory is "persistently mapped", dont have to unmap
        Need to VKDestroy Buffer

    Arseny memcpys mesh data directly into that buffer. VK tutorials do something else right?

Adding mesh data to pipeline
    Arseny clals this the "old" ffp way
        WHILE CREATING PIPELIEN 
            AFter stages,
            Before statecreateinfo 
            Configure VkPipelineVertexInputStateCreateInfo 
            Which requires VkPipelineVertexInputBindingDescription and VkPipelineVertexInputAttrributeDescription
            Binding description contains some info about the formatting of data, unclear to me  

    Modern way: Bindless?

        Arseny uses a NON-AMD extension called "VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME" 
        "Use push descriptors to just send data to the shader without managing descriptor sets ourselves"

        Arseny ran into an issue where he needed vkphysicaldevicefeatures.vertexpipelinestoresandatomics

        Bindings for the pipeline:
        While creating grapics pipeline (vkpipelinelayoutcreateinfo), we need a descriptorsetlayout and a descriptorsetlayoutcreateinfo to pass in. The descriptorsetlayout needs flags to support push, and an array of descriptorsetlayoutbinding, one for eac thing we can bind(???). 
        A descriptorsetlayoutbinding needs descriptor type and offset, stage flags (VK_SHADER_STAGE_) and descriptortype -- VK_DESCRIPTION_TYPE_STORAGE_BUFFER.
        The descriptorsetlayoutcreateinfo is constructed like this:
            VkDescriptorSetLayoutInfo setCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}
            setCreateInfo.flags = VK_DESCRIPTOR_SET_PUSH_DESCRKIPTOR_BIT_KHR 
            setinfo.binding count = number of bindings
            setcreateinfo.pbindings = setlayoutbinding 

        Can destroy set layout right aftercalling create pipeline  


Per frame (?)

Drawing mesh 
    Old Way
        After binding pipeline, (naturally) before draw
        VKCmdBindVertexBuffers
        VkCmdBindIndexBuffer
        VkCMdDrawIndexed

    Modern way: Bindless?
        Remove bindvertexbuffer 
            //This is our extension I guess
        Create vkDescriptorBufferInfo 
            takes buffer (this is our vertex buffer we created with the mesh)
            offset and range (size of vbuffer)
        Create vkWriteDescriptorSet
            Takes type, binding (offset?), count, descriptortype, and bfuferinfo
        Call vkCmdPushDescriptorSetKHR 
            Takes bind poijnt (graphics), layout, count/etc info, and descriptorset

    This draws mesh.


    Arseny does uint_8_t normals and takes (normal / 127.0 - 1) for more compact/less precise normals
    on GLSL this requires extensions: GL_EXT_shader_16bit_storage, GL_EXT_shader_8bit_storage, GL_KHX_shader_explicit_arithmetic_types
    And some vulkan extensions: VK_KHR_16BIT_STORAGE_EXTENSION_NAME, VK_KHR_8BIT_STORAGE_EXTENSION_NAME 
        THis requires a physicaldevice feature which isn't included in features -- he does a pattern with "features2". Look at code if needed.
    

NOTE: niagra mesh shader extension is the old nvidia specific one -- modern is this:
https://www.khronos.org/blog/mesh-shading-for-vulkan
https://github.com/nvpro-samples/gl_vk_meshlet_cadscene
    Mesh Shaders
