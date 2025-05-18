#define _AMD64_
#include "rendererGlobals.h"
#include "VulkanIncludes/Vulkan_Includes.h"
#include <Superluminal/PerformanceAPI_capi.h>
#include "minwindef.h"
#include "libloaderapi.h"
PFN_vkSetDebugUtilsObjectNameEXT FP_vkSetDebugUtilsObjectNameEXT;

void registerDebugUtilsFn(PFN_vkSetDebugUtilsObjectNameEXT ptr)
{
    FP_vkSetDebugUtilsObjectNameEXT = ptr;
}

void SetDebugObjectName(VkDevice device, VkObjectType type, std::string name, uint64_t handle)
{
    VkDebugUtilsObjectNameInfoEXT DebugUtilsObjectNameInfo{};
    DebugUtilsObjectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    DebugUtilsObjectNameInfo.pNext = nullptr;
    DebugUtilsObjectNameInfo.objectType = type;
    DebugUtilsObjectNameInfo.objectHandle = handle;
    DebugUtilsObjectNameInfo.pObjectName = name.c_str();

    FP_vkSetDebugUtilsObjectNameEXT(device, &DebugUtilsObjectNameInfo);
}



PFN_vkTransitionImageLayoutEXT FP_vkTransitionImageLayoutEXT;

void registerTransitionImagefn(PFN_vkTransitionImageLayoutEXT ptr)
{
    FP_vkTransitionImageLayoutEXT = ptr;
}


void vkTransitionImageLayout(VkDevice device, uint32_t transitionCount,
    const VkHostImageLayoutTransitionInfoEXT* pTransitions)
{
    assert(FP_vkTransitionImageLayoutEXT != VK_NULL_HANDLE);
    VK_CHECK(FP_vkTransitionImageLayoutEXT(device, transitionCount, pTransitions));
}



PFN_vkCopyImageToMemoryEXT FP_vkCopyImageToMemoryEXT;

void registerCopyImageToMemoryFn(PFN_vkCopyImageToMemoryEXT ptr)
{
    FP_vkCopyImageToMemoryEXT = ptr;
}


void vkCopyImageToMemory(VkDevice device, void*targetHostPointer, VkImage sourceImage,
    VkExtent3D extent, uint32_t mipLevel, uint32_t baseArrayLayer, uint32_t mipCt, uint32_t layerCt)
{
    
    assert(FP_vkCopyImageToMemoryEXT != VK_NULL_HANDLE);
    VkImageSubresourceLayers subresource = {VK_IMAGE_ASPECT_COLOR_BIT,
        static_cast<uint32_t>(mipLevel),
        baseArrayLayer,
        layerCt};
    VkImageToMemoryCopyEXT region = {
        VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT,
        VK_NULL_HANDLE,
        targetHostPointer,
        0,
        0,
        subresource,
        {0, 0},
        extent
    };
    VkCopyImageToMemoryInfoEXT info = {
        VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT,
        VK_NULL_HANDLE,
        0, 
        sourceImage,
        VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        1,
        &region
    };
    VK_CHECK(FP_vkCopyImageToMemoryEXT(device, &info));
}


PerformanceAPI_Functions SuperluminalFns;

int PerformanceAPI_Load(const wchar_t* inPathToDLL, PerformanceAPI_Functions* outFunctions)
{

    HMODULE module = LoadLibraryW(inPathToDLL);
    if (module == NULL)
        return 0;

    PerformanceAPI_GetAPI_Func getAPI = (PerformanceAPI_GetAPI_Func)((void*)GetProcAddress(module, "PerformanceAPI_GetAPI"));
    if (getAPI == NULL)
    {
        FreeLibrary(module);
        return 0;
    }

    if (getAPI(PERFORMANCEAPI_VERSION, outFunctions) == 0)
    {
        FreeLibrary(module);
        return 0;
    }

    return 1;
}
void registerSuperluminal()
{
    assert(PerformanceAPI_Load(L"./dll/PerformanceAPI.dll",&SuperluminalFns));
    // PerformanceAPI_GetAPI_Func
    // PerformanceAPI_LoadFrom(L"./dll/PerformanceAPI.dll",SuperluminalFns);
}

void superLuminalEnd()
{
    SuperluminalFns.EndEvent();
}

void superLuminalAdd(const char* inID)
{
   SuperluminalFns.BeginEvent(inID, NULL, PERFORMANCEAPI_MAKE_COLOR(128, 128, 255 ));
}
