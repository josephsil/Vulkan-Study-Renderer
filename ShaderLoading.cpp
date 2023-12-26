#include "ShaderLoading.h"
#include "Vulkan_Includes.h"
#include <Windows.h>
#include "dxcapi.h"
#include <d3dcompiler.h>
#include <atlbase.h>
#include <cassert>
#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>

#include "FileCaching.h"
#include "Memory.h"
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
    uint32_t arenaSize = 40000;
    MemoryArena::initialize(tempArena, arenaSize);
    std::span<std::span<wchar_t>> strings = MemoryArena::AllocSpan<std::span<wchar_t>>(tempArena, MAX_INCLUDES);
    std::span<wchar_t> includeTest = MemoryArena::AllocSpan<wchar_t>(tempArena, 7);
    char includeTemplate[] = "#include";
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

std::span<std::span<wchar_t>>  findShaderIncludes(MemoryArena::memoryArena* allocator, std::wstring shaderPath)
{
   
    size_t filenameStart = 0;
    for(int i = shaderPath.length();i > 0; i --)
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
    std::span<std::span<wchar_t>> includes = parseShaderIncludeStrings(allocator, shaderPath);
    for(int i = 0; i < includes.size(); i++)
    {
        std::span<wchar_t> newPath = MemoryArena::AllocSpan<wchar_t>(allocator, filenameStart + includes[i].size());

        //Copy head
        for (int j = 0; j < filenameStart; j++)
        {
            newPath[j] = shaderPath[j];
        }
        //copy tail
        for (int j = 0; j < includes[i].size(); j++)
        {
            newPath[filenameStart + j] = includes[i][j];
        }
       
        includes[i] = newPath;
        //we have includes, now we need to look up files by them
    }
    return includes;
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
        this->size = size; //TODO JS: magic number i dont understand
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

void ShaderLoader::AddShader(const char* name, std::wstring shaderPath)
{
    MemoryArena::memoryArena scratch;
    MemoryArena::initialize(&scratch, 4000);
    shaderPaths shaderPaths = {.path = shaderPath, .includePaths = findShaderIncludes(&scratch, shaderPath)};
    bool needsCompiled = ShaderNeedsReciompiled(shaderPaths);
    //TODO JS: if no, load a cached version

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    if (needsCompiled) { shaderCompile(shaderPaths.path, false); }
    vertShaderStageInfo.module = shaderLoad(shaderPaths.path, false);
    vertShaderStageInfo.pName = "Vert"; //Entry point name 

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    if (needsCompiled) { shaderCompile(shaderPaths.path, true); }
    fragShaderStageInfo.module = shaderLoad(shaderPaths.path, true);
    fragShaderStageInfo.pName = "Frag"; //Entry point name   
    std::vector shaderStages = {vertShaderStageInfo, fragShaderStageInfo};
    // VkPipelineShaderStageCreateInfo test[] = { vertShaderStageInfo, fragShaderStageInfo };
    compiledShaders.insert({name, shaderStages});
    if (needsCompiled)
    {
         FileCaching::saveAssetChangedTime(shaderPaths.path);
    }
    
  

}

void ShaderLoader::shaderCompile(std::wstring shaderFilename, bool is_frag)
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
    size_t idx = filename.rfind('.');
    if (idx != std::string::npos)
    {
        extension = filename.substr(idx + 1);
        if (is_frag)
        {
            targetProfile = L"ps_6_5";
        }
        else
        {
            targetProfile = L"vs_6_5";
        }
        // Mapping for other file types go here (cs_x_y, lib_x_y, etc.)
    }

    // Configure the compiler arguments for compiling the HLSL shader to SPIR-V
    std::vector arguments = {
        // (Optional) name of the shader file to be displayed e.g. in an error message
        filename.c_str(),
        // Shader main entry point
        L"-E", (is_frag) ? L"Frag" : L"Vert",
        // Shader target profile
        L"-T", targetProfile,
        L"-I", L"./Shaders/Includes",
        // Compile to SPIRV
        L"-spirv"
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

    SaveBlobToDisk(filename + (is_frag ? L".frag" : L".vert"), code->GetBufferSize(),
                   static_cast<uint32_t*>(code->GetBufferPointer()));
}

VkShaderModule ShaderLoader::shaderLoad(std::wstring shaderFilename, bool is_frag)
{
    loadedBlob blob = LoadBlobFromDisk(shaderFilename + (is_frag ? L".frag" : L".vert"));

    // Create a Vulkan shader module from the compilation result
    VkShaderModuleCreateInfo shaderModuleCI{};
    shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCI.codeSize = blob.size;
    shaderModuleCI.pCode = blob.buffer.get();
    VkShaderModule shaderModule;
    vkCreateShaderModule(device_, &shaderModuleCI, nullptr, &shaderModule);

    return shaderModule;
}
