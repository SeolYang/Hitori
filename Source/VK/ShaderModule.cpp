#include <Core.h>
#include <VK/ShaderModule.h>
#include <VK/VulkanInstance.h>

namespace sy
{
	ShaderModule::ShaderModule(const std::string_view name, const VulkanInstance& vulkanInstance, const std::string_view filePath, const VkShaderStageFlags shaderType, const std::string_view entryPoint) :
		VulkanWrapper(name, vulkanInstance, VK_DESTROY_LAMBDA_SIGNATURE(VkShaderModule)
		{
			vkDestroyShaderModule(vulkanInstance.GetLogicalDevice(), handle, nullptr);
		}), 
		path(filePath),
		entryPoint(entryPoint),
		shaderType(shaderType)
	{
		SY_ASSERT(!filePath.empty(), "Empty shader file path!");
		SY_ASSERT(!entryPoint.empty(), "Empty shader entry point!");

        std::ifstream file(path, std::ios::ate | std::ios::binary);
        SY_ASSERT(file.is_open(), "Failed to open shader binary from {}.", path);

        const size_t fileSize = file.tellg();
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        const VkShaderModuleCreateInfo createInfo
        {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .codeSize = buffer.size() * sizeof(uint32_t),
            .pCode = buffer.data()
        };

        spdlog::trace("Creating shader module from {}...", path);
        VK_ASSERT(vkCreateShaderModule(vulkanInstance.GetLogicalDevice(), &createInfo, nullptr, &handle), "Failed to create shader module from {}.", path);
	}
}
