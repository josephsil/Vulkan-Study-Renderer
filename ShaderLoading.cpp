#include <Windows.h>
#include <string>
#include <iostream>
#include <vector>
#include "dxcapi.h"
#include <map>

#include <d3dcompiler.h>
#include <atlbase.h>
#include <vulkan/vulkan.h>

class ShaderLoader {

public:
	ShaderLoader(VkDevice device)
	{
		device_ = device;
	}

	std::map<const char*, std::vector<VkPipelineShaderStageCreateInfo>> compiledShaders;

	void AddShader(const char* name, std::wstring shaderPath)
	{
		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = shaderLoad(shaderPath, false);
		vertShaderStageInfo.pName = "Vert"; //Entry point name 

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = shaderLoad(shaderPath, true);
		fragShaderStageInfo.pName = "Frag";	//Entry point name   
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		VkPipelineShaderStageCreateInfo test[] = { vertShaderStageInfo, fragShaderStageInfo };
		compiledShaders.insert({name, shaderStages});
		
	}
private:
	VkDevice device_;
	VkShaderModule shaderLoad(std::wstring shaderFilename, bool is_frag)
	{
		HRESULT hres;
		auto filename = shaderFilename;
		// Initialize DXC library
		CComPtr<IDxcLibrary> library;
		hres = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
		if (FAILED(hres)) {
			throw std::runtime_error("Could not init DXC Library");
		}

		// Initialize DXC compiler
		CComPtr<IDxcCompiler3> compiler;
		hres = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
		if (FAILED(hres)) {
			throw std::runtime_error("Could not init DXC Compiler");
		}

		// Initialize DXC utility
		CComPtr<IDxcUtils> utils;
		hres = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
		if (FAILED(hres)) {
			throw std::runtime_error("Could not init DXC Utiliy");
		}

		// Load the HLSL text shader from disk
		uint32_t codePage = DXC_CP_ACP;
		CComPtr<IDxcBlobEncoding> sourceBlob;
		hres = utils->LoadFile(filename.c_str(), &codePage, &sourceBlob);
		if (FAILED(hres)) {
			throw std::runtime_error("Could not load shader file");
		}
		std::wstring extension;
		// Select target profile based on shader file extension
		LPCWSTR targetProfile{};
		size_t idx = filename.rfind('.');
		if (idx != std::string::npos) {
			extension = filename.substr(idx + 1);
			if (is_frag ) {
				targetProfile = L"ps_6_5";
			}
			else {
				targetProfile = L"vs_6_5";
			}
			// Mapping for other file types go here (cs_x_y, lib_x_y, etc.)
		}

		// Configure the compiler arguments for compiling the HLSL shader to SPIR-V
		std::vector<LPCWSTR> arguments = {
			// (Optional) name of the shader file to be displayed e.g. in an error message
			filename.c_str(),
			// Shader main entry point
			L"-E", (is_frag)? L"Frag" : L"Vert",
			// Shader target profile
			L"-T", targetProfile,
			// Compile to SPIRV
			L"-spirv"
		};

		// Compile shader
		DxcBuffer buffer{};
		buffer.Encoding = DXC_CP_ACP;
		buffer.Ptr = sourceBlob->GetBufferPointer();
		buffer.Size = sourceBlob->GetBufferSize();

		CComPtr<IDxcResult> result{ nullptr };
		hres = compiler->Compile(
			&buffer,
			arguments.data(),
			(uint32_t)arguments.size(),
			nullptr,
			IID_PPV_ARGS(&result));

		if (SUCCEEDED(hres)) {
			result->GetStatus(&hres);
		}

		// Output error if compilation failed
		if (FAILED(hres) && (result)) {
			CComPtr<IDxcBlobEncoding> errorBlob;
			hres = result->GetErrorBuffer(&errorBlob);
			if (SUCCEEDED(hres) && errorBlob) {
				std::cerr << "Shader compilation failed :\n\n" << (const char*)errorBlob->GetBufferPointer();
				throw std::runtime_error("Compilation failed");
			}
		}

		// Get compilation result
		CComPtr<IDxcBlob> code;
		result->GetResult(&code);

		// Create a Vulkan shader module from the compilation result
		VkShaderModuleCreateInfo shaderModuleCI{};
		shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleCI.codeSize = code->GetBufferSize();
		shaderModuleCI.pCode = (uint32_t*)code->GetBufferPointer();
		VkShaderModule shaderModule;
		vkCreateShaderModule(device_, &shaderModuleCI, nullptr, &shaderModule);
		return shaderModule;
	}
};