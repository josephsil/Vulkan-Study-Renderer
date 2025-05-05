#include "ShaderLoading.h"
#include <Renderer/VulkanIncludes/Vulkan_Includes.h>
#include <Windows.h>
#include "dxcapi.h"
#include <d3dcompiler.h>
#include <atlbase.h>
#include <cassert>
#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>

#include <General/Array.h>
#include <General/FileCaching.h>
#include <General/MemoryArena.h>

#include "Renderer/rendererGlobals.h"
#ifndef WIN32
#include <unistd.h>
#endif

#ifdef WIN32
#define stat _stat
#endif

struct shaderPaths
{
    std::wstring path;
    std::span<std::span<wchar_t>> includePaths;
};

ShaderLoader::ShaderLoader(VkDevice device)
{
    device_ = device;
}

std::span<std::span<wchar_t>> parseShaderIncludeStrings(MemoryArena::memoryArena* tempArena, std::wstring shaderPath)
{
    uint32_t MAX_INCLUDES = 10; //arbitrary
    FILE* f;
    _wfopen_s(&f, shaderPath.c_str(), L"r");
    
    std::span<std::span<wchar_t>> strings = MemoryArena::AllocSpan<std::span<wchar_t>>(tempArena, MAX_INCLUDES);
    std::span<wchar_t> includeTest = MemoryArena::AllocSpan<wchar_t>(tempArena, 7);

    const char includeTemplate[] = "#include";
    char c;
    
    int i = 0;
    int stringsCt = 0;
    bool scanningInclude = false;
    int quotesCount = 0;
    int includeLength = 0;
    while ((c =  fgetc(f))  != EOF)
    {
        switch (scanningInclude)
        {
        case false:
                    if (c == '#' || i != 0)
                    {
                        //failed 
                        if (c != includeTemplate[i])
                        {
                            i = 0;
                            continue;
                        }
                        includeTest[i] = c;
                        i++;

                        //passed
                        if (i == 7)
                        {
                            scanningInclude = true; 
                            //match -- we're reading an include pragma 
                        }
                    }
                continue;
            case true:
                    if (c == '"')
                    {
                        quotesCount ++;
                        if (quotesCount == 2)
                        {
                            //at end quote
                            strings[stringsCt++] = MemoryArena::AllocSpan<wchar_t>(tempArena, includeLength);
                            fseek(f, -(includeLength +1), SEEK_CUR);
                            for (int j = 0; j < includeLength; j++)
                            {
                                strings[stringsCt -1][j] = fgetc(f);
                            }
                            quotesCount = 0;
                            scanningInclude = false;
                            includeLength = 0;
                        }
                        continue;
                    }
                    if (c == '\n')
                    {
                        //Reset and continue 
                        quotesCount = 0;
                        scanningInclude = false;
                        includeLength = 0;
                    }
                    if (quotesCount > 0)
                    {
                        includeLength ++;
                        //inside quotes
                        
                    }
                    continue;
                
            }
        
    }
    fclose(f);
    return strings.subspan(0, stringsCt); // truncate unused space

}
void copySubstring(std::span<wchar_t> sourceA, std::span<wchar_t> sourceB, std::span<wchar_t> tgt)
{
    assert (tgt.size() <= sourceA.size() + sourceB.size());
    size_t headLength = sourceA.size();
    //Copy head
    for (int j = 0; j < headLength; j++)
    {
        tgt[j] = sourceA[j];
    }
    //copy tail
    for (int j = 0; j < sourceB.size(); j++)
    {
        tgt[headLength + j] = sourceB[j];
    }
        
}

struct shaderIncludeInfo
{
    std::span<wchar_t> path;
    bool visited;
};

std::span<std::span<wchar_t>>  findShaderIncludes(MemoryArena::memoryArena* allocator, std::wstring shaderPath)
{
   
    size_t filenameStart = 0;
    for(int i = (int)shaderPath.length();i > 0; i --)
    {
        if (shaderPath[i] == L'\\' || shaderPath[i] == L'/') //At last delimeter
        {
            if (i + 1 >= shaderPath.length())
            {
                break; //ends in a slash -- no valid slash
            }
        filenameStart = i + 1;
            break;
        }
    }
    const int MAX_INCLUDES = 30;
    std::span<std::span<wchar_t>>  outputIncludes = MemoryArena::AllocSpan<std::span<wchar_t>>(allocator, MAX_INCLUDES);
    Array<shaderIncludeInfo> allIncludes = Array(MemoryArena::AllocSpan<shaderIncludeInfo>(allocator, MAX_INCLUDES));
    allIncludes.push_back({shaderPath, false});
    int idx = 0;

    //Recursively gather includes 
    while(idx < allIncludes.ct && allIncludes[idx].visited == false)
    {
        std::span<std::span<wchar_t>> includes = parseShaderIncludeStrings(allocator, allIncludes[idx].path.data());
        idx++;
        for(int i =0; i < includes.size(); i++)
        {
            bool alreadyVisited = false;

            std::span<wchar_t> newPath = MemoryArena::AllocSpan<wchar_t>(allocator, filenameStart + includes[i].size());

            copySubstring(std::span<wchar_t>(shaderPath).subspan(0, filenameStart), includes[i], newPath);
            includes[i] = newPath;
            for (int j = 0; j < allIncludes.ct; j++)
            {
                if (includes[i].data() == allIncludes[j].path.data())
                {
                    alreadyVisited = true;
                    break;
                }
            }
            if (alreadyVisited)
            {
                MemoryArena::freeLast(allocator);
                continue;
            }
            allIncludes.push_back({includes[i], false});
        }
    }
    for(int i = 0; i < allIncludes.ct; i++)
    {
       outputIncludes[i] = allIncludes[i].path;
        //we have includes, now we need to look up files by them
    }
    return outputIncludes.subspan(1,allIncludes.size() - 1); //clip off the first one -- it's the shader itself
}
//TODO JS: Do includes -- parse includes and check modifieds for them too
bool ShaderNeedsReciompiled(shaderPaths shaderPath)
{
    //was this shader modified?
    if (FileCaching::assetOutOfDate(shaderPath.path))
    {
        return true;
    }

    //were its includes?
    for (int i = 0; i <  shaderPath.includePaths.size(); i++)
    {
        if (FileCaching::assetOutOfDate( shaderPath.includePaths[i].data()))
        {
            FileCaching::saveAssetChangedTime(shaderPath.includePaths[i].data()); //TODO JS: recursively walk includes
            //TODO JS: would be way easier just to hoist the includes over and hash the file ???
        }
    }
    for (int i = 0; i <  shaderPath.includePaths.size(); i++)
    {
        if (!FileCaching::compareAssetAge(shaderPath.path.data(), shaderPath.includePaths[i].data()))
        {
            printf("Compiling shader %ls -- #include %ls updated \n", shaderPath.path.data(), shaderPath.includePaths[i].data());
            
            //touch shader to update time
            FileCaching::touchFile(shaderPath.path);
            
            return true;
        }
    }

    //all good
    return false;
   
}

bool SaveBlobToDisk(std::wstring shaderPath, SIZE_T size, uint32_t* buffer)
{
    std::wstring compiledShaderPath = shaderPath + L".compiled";

    std::ofstream myFile(compiledShaderPath,
                         std::ifstream::in | std::ifstream::out | std::ifstream::trunc | std::ios::binary);

    myFile.write(reinterpret_cast<char*>(buffer), (size));


    myFile.close();
    return true;
}

struct loadedBlob
{
    uint32_t size;
    std::unique_ptr<uint32_t[]> buffer;

    loadedBlob(SIZE_T size, std::unique_ptr<uint32_t[]> _buffer)
    {
        this->size = (uint32_t)size; //TODO JS: magic number i dont understand
        this->buffer = std::move(_buffer);
    }
};

loadedBlob LoadBlobFromDisk(std::wstring shaderPath)
{
    std::wstring compiledShaderPath = shaderPath + L".compiled";

    struct stat result;
    if (_wstat(compiledShaderPath.c_str(), &result) != 0)
    {
        throw std::runtime_error("Could not read shader file ");
    }

    int size = result.st_size;

    std::ifstream myFile(compiledShaderPath, std::ifstream::in | std::ios::binary);
    if (!myFile.is_open())
    {
        throw std::runtime_error("Could not read shader file ");
    }

    auto buffer = std::unique_ptr<uint32_t[]>(new uint32_t[size]);

    myFile.read(reinterpret_cast<char*>(buffer.get()), size);
    auto blob = loadedBlob(size, std::move(buffer));
    myFile.close();

    return blob;
}

void ShaderLoader::AddShader(const char* name, std::wstring shaderPath, bool compute)
{
    MemoryArena::memoryArena scratch;
    MemoryArena::initialize(&scratch, 80000);

    //TODO JS: get all includes recursively 
    shaderPaths shaderPaths = {.path = shaderPath, .includePaths = findShaderIncludes(&scratch, shaderPath)};
    bool needsCompiled = ShaderNeedsReciompiled(shaderPaths);
    needsCompiled = true;
    //TODO JS: if no, load a cached version
    
    switch (compute)
    {
    case false:
        {
            VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
            vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            if (needsCompiled) { shaderCompile(shaderPaths.path, vert); }
            vertShaderStageInfo.module = shaderLoad(shaderPaths.path, vert);
            vertShaderStageInfo.pName = "Vert"; //Entry point name 
            VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
            if (vertShaderStageInfo.module != VK_NULL_HANDLE)
            {
                setDebugObjectName(device_, VK_OBJECT_TYPE_SHADER_MODULE, name, (uint64_t)(vertShaderStageInfo.module));
            }
            fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            if (needsCompiled) { shaderCompile(shaderPaths.path, frag); }
            fragShaderStageInfo.module = shaderLoad(shaderPaths.path, frag);
            fragShaderStageInfo.pName = "Frag"; //Entry point name   
            std::vector shaderStages = {vertShaderStageInfo, fragShaderStageInfo};
             if (fragShaderStageInfo.module != VK_NULL_HANDLE)
            {
            setDebugObjectName(device_, VK_OBJECT_TYPE_SHADER_MODULE, name, (uint64_t)fragShaderStageInfo.module);
            }
            VkPipelineShaderStageCreateInfo test[] = { vertShaderStageInfo, fragShaderStageInfo };
            compiledShaders.insert({name, shaderStages});
            break;
        }
    case true:
        {
            VkPipelineShaderStageCreateInfo computeShaderStage{};
            computeShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            computeShaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            if (needsCompiled) { shaderCompile(shaderPaths.path, comp); }
            computeShaderStage.module = shaderLoad(shaderPaths.path, comp);
            computeShaderStage.pName = "Main"; //Entry point name
            std::vector shaderStages = {computeShaderStage};
            compiledShaders.insert({name, shaderStages});
             if (computeShaderStage.module != VK_NULL_HANDLE)
            {
            setDebugObjectName(device_, VK_OBJECT_TYPE_SHADER_MODULE, name, (uint64_t)computeShaderStage.module);
            }
            break;
        }
    }
        if (needsCompiled)
        {
            FileCaching::saveAssetChangedTime(shaderPaths.path);
        }
}



void ShaderLoader::shaderCompile(std::wstring shaderFilename, shaderType stagetype)
{
    HRESULT hres;
    auto filename = shaderFilename;
    // Initialize DXC library
    CComPtr<IDxcLibrary> library;
    hres = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
    if (FAILED(hres))
    {
        throw std::runtime_error("Could not init DXC Library");
    }

    // Initialize DXC compiler
    CComPtr<IDxcCompiler3> compiler;
    hres = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    if (FAILED(hres))
    {
        throw std::runtime_error("Could not init DXC Compiler");
    }

    // Initialize DXC utility
    CComPtr<IDxcUtils> utils;
    hres = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
    if (FAILED(hres))
    {
        throw std::runtime_error("Could not init DXC Utiliy");
    }

    // Load the HLSL text shader from disk
    uint32_t codePage = DXC_CP_ACP;
    CComPtr<IDxcBlobEncoding> sourceBlob;
    hres = utils->LoadFile(filename.c_str(), &codePage, &sourceBlob);
    if (FAILED(hres))
    {
        throw std::runtime_error("Could not load shader file");
    }
    std::wstring extension;
    // Select target profile based on shader file extension
    LPCWSTR targetProfile{};
    LPCWSTR entryPoint{};
    LPCWSTR suffix{};
    size_t idx = filename.rfind('.');
    if (idx != std::string::npos)
    {
        extension = filename.substr(idx + 1);
        if (stagetype == frag)
        {
            targetProfile = L"ps_6_5";
            entryPoint = L"Frag";
            suffix = L".frag";
        }
        if (stagetype == vert)
        {
            targetProfile = L"vs_6_5";
            entryPoint = L"Vert";
            suffix = L".vert";
        }
        if (stagetype == comp)
        {
            targetProfile = L"cs_6_5";
            entryPoint = L"Main";
            suffix = L".comp";
        }
        // Mapping for other file types go here (cs_x_y, lib_x_y, etc.)
    }

    // Configure the compiler arguments for compiling the HLSL shader to SPIR-V
    std::vector arguments = {
        // (Optional) name of the shader file to be displayed e.g. in an error mes`ge
        filename.c_str(),
        // Shader main entry point
        L"-E", entryPoint,
        // Shader target profile
        L"-T", targetProfile,
        L"-I", L"./Shaders/Includes",
        // Compile to SPIRV
        L"-spirv",
        L"-fvk-support-nonzero-base-instance",
        L"-Zi",
        L"-fspv-debug=vulkan-with-source"
    };

    // Compile shader

    DxcBuffer buffer;
    buffer.Encoding = DXC_CP_ACP;
    buffer.Ptr = sourceBlob->GetBufferPointer();
    buffer.Size = sourceBlob->GetBufferSize();
    IDxcIncludeHandler* includeHandler;
    utils->CreateDefaultIncludeHandler(&includeHandler);
    CComPtr<IDxcResult> result{nullptr};
        hres = compiler->Compile(
        &buffer,
        arguments.data(),
        static_cast<uint32_t>(arguments.size()),
        includeHandler ,
        IID_PPV_ARGS(&result));

    if (SUCCEEDED(hres))
    {
        result->GetStatus(&hres);
    }

    // Output error if compilation failed
    if (FAILED(hres) && (result))
    {
        CComPtr<IDxcBlobEncoding> errorBlob;
        hres = result->GetErrorBuffer(&errorBlob);
        if (SUCCEEDED(hres) && errorBlob)
        {
            std::cerr << "Shader compilation failed :\n\n" << static_cast<const char*>(errorBlob->GetBufferPointer());
            throw std::runtime_error("Compilation failed");
        }
    }

    // Get compilation result
    CComPtr<IDxcBlob> code;
    auto utf = library->GetBlobAsUtf8(code.p, nullptr);

    result->GetResult(&code);

    SaveBlobToDisk(filename + (suffix), code->GetBufferSize(),
                   static_cast<uint32_t*>(code->GetBufferPointer()));
}

VkShaderModule ShaderLoader::shaderLoad(std::wstring shaderFilename, shaderType  stagetype)
{
    LPCWSTR suffix{};
    if (stagetype == frag)
    {
        suffix = L".frag";
    }
    if (stagetype == vert)
    {
        suffix = L".vert";
    }
    if (stagetype == comp)
    {
        suffix = L".comp";
    }
    
    loadedBlob blob = LoadBlobFromDisk(shaderFilename + (suffix));

    // Create a Vulkan shader module from the compilation result
    VkShaderModuleCreateInfo shaderModuleCI{};
    shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCI.codeSize = blob.size;
    shaderModuleCI.pCode = blob.buffer.get();
    VkShaderModule shaderModule;
    vkCreateShaderModule(device_, &shaderModuleCI, nullptr, &shaderModule);

    return shaderModule;
}
