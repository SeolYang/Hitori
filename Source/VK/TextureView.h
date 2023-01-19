#pragma once
#include <PCH.h>

namespace sy::vk
{
	class VulkanContext;
	class Texture;
	class TextureView : public VulkanWrapper<VkImageView>
	{
	public:
		TextureView(std::string_view name, const VulkanContext& vulkanContext, const Texture& texture, VkImageViewType viewType, TextureSubResourceRange subResourceRange);
		TextureView(std::string_view name, const VulkanContext& vulkanContext, const Texture& texture, VkImageViewType viewType);

		virtual ~TextureView() override = default;

	private:
		const VkImageViewType viewType;
		const TextureSubResourceRange subResourceRange;

	};
}