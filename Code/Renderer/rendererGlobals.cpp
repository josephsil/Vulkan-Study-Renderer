#include "rendererGlobals.h"
#include "VulkanIncludes/Vulkan_Includes.h"

PFN_vkSetDebugUtilsObjectNameEXT FP_vkSetDebugUtilsObjectNameEXT;

void registerDebugUtilsFn(PFN_vkSetDebugUtilsObjectNameEXT ptr)
{
    FP_vkSetDebugUtilsObjectNameEXT = ptr;
}

void setDebugObjectName(VkDevice device, VkObjectType type, std::string name, uint64_t handle)
{
    
    VkDebugUtilsObjectNameInfoEXT DebugUtilsObjectNameInfo{};
    DebugUtilsObjectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    DebugUtilsObjectNameInfo.pNext = nullptr;
    DebugUtilsObjectNameInfo.objectType = type;
    DebugUtilsObjectNameInfo.objectHandle = handle;
    DebugUtilsObjectNameInfo.pObjectName = name.c_str();
    
    FP_vkSetDebugUtilsObjectNameEXT(device,&DebugUtilsObjectNameInfo);
}
