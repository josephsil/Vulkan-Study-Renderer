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
#ifndef WIN32
#include <unistd.h>
#endif

#ifdef WIN32
#define stat _stat
#endif


ShaderLoader::ShaderLoader(VkDevice device)
{
    device_ = device;
}

void SaveShaderCompilationTime(std::wstring shaderPath)
{
    struct stat result;
    if (_wstat(shaderPath.c_str(), &result) != 0)
    {
        throw std::runtime_error("Could not read shader file date");
        return;
    }

    auto modifiedTime = result.st_mtime;

    std::wstring shadertimepath = shaderPath + L".modified";

    int size = result.st_size / sizeof(byte);
    std::ofstream myFile(shadertimepath, std::ifstream::in | std::ifstream::out | std::ifstream::trunc);
    myFile << modifiedTime;

    myFile.close();
}

bool ShaderNeedsReciompiled(std::wstring shaderPath)
{
    //Last changed check 
    struct stat result;
    if (_wstat(shaderPath.c_str(), &result) != 0)
    {
        throw std::runtime_error("Could not read shader file date");
        return true;
    }

    auto modifiedTime = result.st_mtime;

    std::wstring shadertimepath = shaderPath + L".modified";


    if (_wstat(shadertimepath.c_str(), &result) != 0)
    {
        return true;
    }

    int size = result.st_size / sizeof(byte);
    std::ifstream myFile(shadertimepath, std::ifstream::in | std::ifstream::out);
    if (!myFile.is_open())
    {
        //Could not open shadertime file, assume new shader
        return true;
    }
    auto buffer = new char [size];

    myFile.read(buffer, size);

    long long savedTime = atoll(buffer);

    myFile.close();

    //TODO JS: update file with current time after compilation finishes

    if (savedTime == modifiedTime)
    {
        return false;
    }
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
    bool needsCompiled = ShaderNeedsReciompiled(shaderPath);
    //TODO JS: if no, load a cached version

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    if (needsCompiled) { shaderCompile(shaderPath, false); }
    vertShaderStageInfo.module = shaderLoad(shaderPath, false);
    vertShaderStageInfo.pName = "Vert"; //Entry point name 

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    if (needsCompiled) { shaderCompile(shaderPath, true); }
    fragShaderStageInfo.module = shaderLoad(shaderPath, true);
    fragShaderStageInfo.pName = "Frag"; //Entry point name   
    std::vector shaderStages = {vertShaderStageInfo, fragShaderStageInfo};
    // VkPipelineShaderStageCreateInfo test[] = { vertShaderStageInfo, fragShaderStageInfo };
    compiledShaders.insert({name, shaderStages});
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
        // Compile to SPIRV
        L"-spirv"
    };

    // Compile shader

    DxcBuffer buffer;
    buffer.Encoding = DXC_CP_ACP;
    buffer.Ptr = sourceBlob->GetBufferPointer();
    buffer.Size = sourceBlob->GetBufferSize();

    CComPtr<IDxcResult> result{nullptr};
    hres = compiler->Compile(
        &buffer,
        arguments.data(),
        static_cast<uint32_t>(arguments.size()),
        nullptr,
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

    SaveShaderCompilationTime(shaderFilename);

    return shaderModule;
}
