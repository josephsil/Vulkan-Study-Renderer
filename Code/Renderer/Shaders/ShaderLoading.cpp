#include "ShaderLoading.h"
#include <Renderer/VulkanIncludes/Vulkan_Includes.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "dxcapi.h"
#include <cassert>
#include <iostream>
#include <fstream>



#include <General/Array.h>
#include <General/FileCaching.h>
#include <General/MemoryArena.h>


#include <Renderer/rendererGlobals.h>

#ifndef _WIN32 // apple
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#include <codecvt>
#endif

struct shaderPaths
{
    platform_string path;
    std::span<std::span<platform_char>> includePaths;
};

ShaderLoader::ShaderLoader(VkDevice device)
{
    device_ = device;
}






const struct Platform
{
#ifdef _WIN32
    static const platform_char* suffix;
#else
    static const char* suffix;
#endif

    static void OPEN_WSTRING(FILE** f,std::wstring shaderPath)
    {
#ifdef _WIN32
    OPENFN(f, (platform_char*)shaderPath.c_str());
    
#else // apple
        auto convertedStr =convert_platform_str(shaderPath);
        char* _c = const_cast<char*>(convertedStr.c_str());
        OPENFN(f, _c);
#endif
    }
    
#ifdef _WIN32
    static void OPENFN(FILE** f, platform_char* path)
    {
        _wfopen_s(f, path, L"r");
    }
    
    static std::wstring convert_platform_str(std::wstring path) {
        return path;
    }
    
#else
    
    static std::string convert_platform_str(std::wstring path) {

        // setup converter
        using convert_type = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_type, wchar_t> converter;

        // use converter
        return converter.to_bytes(path);
    }
    
    static void OPENFN(FILE** f, platform_char* path)
    {
        FILE* _f = fopen(path, "r");
        f = &_f;
    }
#endif
    
};

#ifdef _WIN32
    const platform_char* Platform::suffix =  L".compiled";
#else
    const char* Platform::suffix  =  ".compiled";
#endif








std::span<std::span<platform_char>> parseShaderIncludeStrings(MemoryArena::Allocator* tempArena, platform_string shaderPath)
{
    uint32_t MAX_INCLUDES = 10; //arbitrary
    FILE* f;
    

    Platform::OPENFN(&f, const_cast<char*>(shaderPath.c_str()));


    std::span<std::span<platform_char>> strings = MemoryArena::AllocSpan<std::span<platform_char>>(tempArena, MAX_INCLUDES);
    std::span<platform_char> includeTest = MemoryArena::AllocSpan<platform_char>(tempArena, 7);

    constexpr char includeTemplate[] = "#include";
    char c;

    int i = 0;
    int stringsCt = 0;
    int scanningInclude = 0;
    int quotesCount = 0;
    int includeLength = 0;
    while ((c = fgetc(f)) != EOF)
    {
        switch (scanningInclude)
        {
        case 0:
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
                    scanningInclude = 1;
                    //match -- we're reading an include pragma 
                }
            }
            continue;
        case 1:
            if (c == '"')
            {
                quotesCount++;
                if (quotesCount == 2)
                {
                    //at end quote
                    strings[stringsCt++] = MemoryArena::AllocSpan<platform_char>(tempArena, includeLength);
                    fseek(f, -(includeLength + 1), SEEK_CUR);
                    for (int j = 0; j < includeLength; j++)
                    {
                        strings[stringsCt - 1][j] = fgetc(f);
                    }
                    quotesCount = 0;
                    scanningInclude = 0;
                    includeLength = 0;
                }
                continue;
            }
            if (c == '\n')
            {
                //Reset and continue 
                quotesCount = 0;
                scanningInclude = 0;
                includeLength = 0;
            }
            if (quotesCount > 0)
            {
                includeLength++;
                //inside quotes
            }
        }
    }
    fclose(f);
    return strings.subspan(0, stringsCt); // truncate unused space
}

void copySubstring(std::span<platform_char> sourceA, std::span<platform_char> sourceB, std::span<platform_char> tgt)
{
    assert(tgt.size() <= sourceA.size() + sourceB.size());
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
    std::span<platform_char> path;
    bool visited;
};

std::span<std::span<platform_char>> findShaderIncludes(MemoryArena::Allocator* allocator, platform_string shaderPath)
{
    size_t filenameStart = 0;
    for (int i = static_cast<int>(shaderPath.length()); i > 0; i--)
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
    constexpr int MAX_INCLUDES = 30;
    std::span<std::span<platform_char>> outputIncludes = MemoryArena::AllocSpan<std::span<platform_char>>(allocator, MAX_INCLUDES);
    auto allIncludes = Array(MemoryArena::AllocSpan<shaderIncludeInfo>(allocator, MAX_INCLUDES));
    allIncludes.push_back({shaderPath, false});
    int idx = 0;

    //Recursively gather includes 
    while (idx < allIncludes.ct && allIncludes[idx].visited == false)
    {
        std::span<std::span<platform_char>> includes = parseShaderIncludeStrings(allocator, allIncludes[idx].path.data());
        idx++;
        for (int i = 0; i < includes.size(); i++)
        {
            bool alreadyVisited = false;

            auto cursor = MemoryArena::GetCurrentOffset(allocator);
            std::span<platform_char> newPath = MemoryArena::AllocSpan<platform_char>(allocator, filenameStart + includes[i].size());

            copySubstring(std::span<platform_char>(shaderPath).subspan(0, filenameStart), includes[i], newPath);
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
                MemoryArena::FreeToOffset(allocator,cursor);
                continue;
            }
            allIncludes.push_back({includes[i], false});
        }
    }
    for (int i = 0; i < allIncludes.ct; i++)
    {
        outputIncludes[i] = allIncludes[i].path;
        //we have includes, now we need to look up files by them
    }
    return outputIncludes.subspan(1, allIncludes.size() - 1); //clip off the first one -- it's the shader itself
}

//TODO JS: Do includes -- parse includes and check modifieds for them too
bool ShaderNeedsReciompiled(shaderPaths shaderPath)
{
    //was this shader modified?
    if (FileCaching::IsAssetOutOfDate(shaderPath.path))
    {
        return true;
    }

    //were its includes?
    for (int i = 0; i < shaderPath.includePaths.size(); i++)
    {
        if (FileCaching::IsAssetOutOfDate(shaderPath.includePaths[i].data()))
        {
            FileCaching::SaveAssetChangedTime(shaderPath.includePaths[i].data()); //TODO JS: recursively walk includes
            //TODO JS: would be way easier just to hoist the includes over and hash the file ???
        }
    }
    for (int i = 0; i < shaderPath.includePaths.size(); i++)
    {
        if (!FileCaching::CompareAssetAge(shaderPath.path.data(), shaderPath.includePaths[i].data()))
        {
            printf("Compiling shader %ls -- #include %ls updated \n", shaderPath.path.data(),
                   shaderPath.includePaths[i].data());

            //touch shader to update time
            FileCaching::UpdateModified(shaderPath.path);

            return true;
        }
    }

    //all good
    return false;
}

bool SaveBlobToDisk(platform_string shaderPath, SIZE_T size, uint32_t* buffer)
{
    

    auto compiledShaderPath = (shaderPath)  + Platform::suffix;


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
        this->size = static_cast<uint32_t>(size); //TODO JS: magic number i dont understand
        this->buffer = std::move(_buffer);
    }
};

loadedBlob LoadBlobFromDisk(platform_string shaderPath)
{
    auto compiledShaderPath = (shaderPath) + Platform::suffix;

    struct stat result;
    if (STAT(compiledShaderPath.c_str(), &result) != 0)
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

void ShaderLoader::AddShader(const char* name, platform_string shaderPath, bool compute)
{
    MemoryArena::Allocator scratch;
    MemoryArena::Initialize(&scratch, 80000);

    //TODO JS: get all includes recursively 
    shaderPaths shaderPaths = {.path = shaderPath, .includePaths = findShaderIncludes(&scratch, shaderPath)};
    bool needsCompiled = ShaderNeedsReciompiled(shaderPaths);
    //TODO JS: if no, load a cached version
    
    if (!compute)
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
                SetDebugObjectNameS(device_, VK_OBJECT_TYPE_SHADER_MODULE, name, (uint64_t)(vertShaderStageInfo.module));
            }
            fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            if (needsCompiled) { shaderCompile(shaderPaths.path, frag); }
            fragShaderStageInfo.module = shaderLoad(shaderPaths.path, frag);
            fragShaderStageInfo.pName = "Frag"; //Entry point name   
            std::vector shaderStages = {vertShaderStageInfo, fragShaderStageInfo};
            if (fragShaderStageInfo.module != VK_NULL_HANDLE)
            {//
                SetDebugObjectNameS(device_, VK_OBJECT_TYPE_SHADER_MODULE, name, (uint64_t)fragShaderStageInfo.module);
            }
            VkPipelineShaderStageCreateInfo test[] = {vertShaderStageInfo, fragShaderStageInfo};
            compiledShaders.insert({name, shaderStages});
        }
    else
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
                SetDebugObjectNameS(device_, VK_OBJECT_TYPE_SHADER_MODULE, name, (uint64_t)computeShaderStage.module);
            }
        }
    if (needsCompiled)
    {
        FileCaching::SaveAssetChangedTime(shaderPaths.path);
    }
}

std::wstring _WidenString(std::string s)
{
    return std::wstring(CA2W(std::string(s).c_str()));
}

void ShaderLoader::shaderCompile(platform_string shaderFilename, shaderType stagetype)
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
    hres = utils->LoadFile(_WidenString(filename).c_str(), &codePage, &sourceBlob);
    if (FAILED(hres))
    {
        throw std::runtime_error("Could not load shader file");
    }
    platform_string extension;
    // Select target profile based on shader file extension
    const platform_char* targetProfile{};
    const platform_char* entryPoint{};
    const platform_char* suffix{};
    size_t idx = filename.rfind('.');
    if (idx != std::string::npos)
    {
        extension = filename.substr(idx + 1);
        if (stagetype == frag)
        {
            targetProfile = LITSTRING("ps_6_5");
            entryPoint = LITSTRING("Frag");
            suffix = LITSTRING(".frag");
        }
        if (stagetype == vert)
        {
            targetProfile = LITSTRING("vs_6_5");
            entryPoint = LITSTRING("Vert");
            suffix = LITSTRING(".vert");
        }
        if (stagetype == comp)
        {
            targetProfile = LITSTRING("cs_6_5");
            entryPoint = LITSTRING("Main");
            suffix = LITSTRING(".comp");
        }
        // Mapping for other file types go here (cs_x_y, lib_x_y, etc.)
    }


    // Configure the compiler arguments for compiling the HLSL shader to SPIR-V
    std::vector arguments = {
        // (Optional) name of the shader file to be displayed e.g. in an error mes`ge
        filename.c_str(),
        // Shader main entry point
        LITSTRING("-E"), entryPoint,
        // Shader target profile
        LITSTRING("-T"), targetProfile,
        LITSTRING("-I"), LITSTRING("./Shaders/Includes"),
        // Compile to SPIRV
        LITSTRING("-spirv"),
#if _DEBUG
         LITSTRING("-D"),
         LITSTRING("_DEBUG"), //todo js: make sure to recompile shaders when this changes
#endif
     LITSTRING("-fvk-support-nonzero-base-instance"),
     LITSTRING("-Zi"),
     LITSTRING("-fspv-debug=vulkan-with-source"),
    };


#ifndef _WIN32
    std::vector<std::wstring> convertedArgs = {};
    std::vector<const wchar_t*> convertedArgsData = {};
for(int i = 0; i < arguments.size(); i++)
{
    convertedArgs.push_back(_WidenString(arguments[i]));
    const wchar_t* cstr = convertedArgsData[i];
    convertedArgsData.push_back(cstr);
}

#endif //!_WIN32
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
#ifndef _WIN32
        convertedArgsData.data(),
#else  //!_WIN32
        arguments.data(),
#endif //_WIN32
        static_cast<uint32_t>(arguments.size()),
        includeHandler,
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

VkShaderModule ShaderLoader::shaderLoad(platform_string shaderFilename, shaderType stagetype)
{
#if _WIN32
    LPCWSTR suffix{};
#else
    const char* suffix;
#endif
    if (stagetype == frag)
    {
#if _WIN32
        suffix = L".frag";
#else
        suffix = ".frag";
#endif
    }
    if (stagetype == vert)
    {
#if _WIN32
        suffix = L".vert";
#else
        suffix = ".vert";
#endif
    }
    if (stagetype == comp)
    {
#if _WIN32
        suffix = L".comp";
#else
        suffix = ".comp";
#endif
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
