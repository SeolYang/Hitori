#include <Core.h>
#include <Window.h>
#include <VK/VulkanInstance.h>
#include <VK/Swapchain.h>
#include <VK/CommandBuffer.h>
#include <VK/CommandPool.h>
#include <VK/Semaphore.h>
#include <VK/Fence.h>

namespace sy
{
	VulkanInstance::VulkanInstance(const Window& window) :
		window(window),
		instance(VK_NULL_HANDLE),
		surface(VK_NULL_HANDLE),
		physicalDevice(VK_NULL_HANDLE),
		device(VK_NULL_HANDLE),
		allocator(VK_NULL_HANDLE)
	{
		Startup();
	}

	VulkanInstance::~VulkanInstance()
	{
		Cleanup();
	}

	uint32_t VulkanInstance::GetQueueFamilyIndex(const EQueueType queue) const
	{
		switch (queue)
		{
		case EQueueType::Graphics:
			return graphicsQueueFamilyIdx;
		case EQueueType::Compute:
			return computeQueueFamilyIdx;
		case EQueueType::Transfer:
			return transferQueueFamilyIdx;
		case EQueueType::Present:
			return presentQueueFamilyIdx;
		}

		return graphicsQueueFamilyIdx;
	}

	VkQueue VulkanInstance::GetQueue(EQueueType queue) const
	{
		switch (queue)
		{
		case EQueueType::Graphics:
			return graphicsQueue;
		case EQueueType::Compute:
			return computeQueue;
		case EQueueType::Transfer:
			return transferQueue;
		case EQueueType::Present:
			return presentQueue;
		}

		return graphicsQueue;
	}

	void VulkanInstance::SubmitTo(const EQueueType type, const VkSubmitInfo submitInfo, Fence& fence) const
	{
		const auto queue = GetQueue(type);
		SY_ASSERT(queue != VK_NULL_HANDLE, "Invalid queue submission request.");

		vkQueueSubmit(queue, 1, &submitInfo, fence.GetNativeHandle());
	}

	void VulkanInstance::Startup()
	{
		volkInitialize();

		vkb::InstanceBuilder instanceBuilder;
		auto instanceBuilderRes = instanceBuilder.set_app_name(window.GetTitle().data())
#ifdef _DEBUG
			.request_validation_layers()
			.use_default_debug_messenger()
#endif
			.require_api_version(1, 3, 0)
			.build();

		const auto vkbInstance = instanceBuilderRes.value();
		instance = vkbInstance.instance;
		debugMessenger = vkbInstance.debug_messenger;

		volkLoadInstance(instance);

		SDL_Vulkan_CreateSurface(&window.GetSDLWindow(), instance, &surface);

		vkb::PhysicalDeviceSelector physicalDeviceSelector { vkbInstance };
		auto vkbPhysicalDevice = physicalDeviceSelector.set_minimum_version(1, 3)
			.set_surface(surface)
			.add_required_extension("VK_EXT_descriptor_indexing")
			.add_required_extension("VK_KHR_swapchain")
			.add_required_extension("VK_KHR_dynamic_rendering")
			.select()
			.value();

		physicalDevice = vkbPhysicalDevice.physical_device;
		gpuName = vkbPhysicalDevice.properties.deviceName;

		vkb::DeviceBuilder deviceBuilder{ vkbPhysicalDevice };
		VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
			.pNext = nullptr,
			.dynamicRendering = VK_TRUE
		};

		auto vkbDeviceRes = 
			deviceBuilder.add_pNext(&dynamicRenderingFeatures)
			.build();
		SY_ASSERT(vkbDeviceRes.has_value(), "Failed to create device using GPU {}.", gpuName);
		auto& vkbDevice = vkbDeviceRes.value();
		device = vkbDevice.device;
		spdlog::trace("Succeed to create logical device using GPU {}.", gpuName);

		swapchain = std::make_unique<Swapchain>(window , *this);

		const VmaVulkanFunctions vkFunctions
		{
			.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
			.vkGetDeviceProcAddr = vkGetDeviceProcAddr
		};

		const VmaAllocatorCreateInfo allocatorInfo
		{
			.physicalDevice = physicalDevice,
			.device = device,
			.pVulkanFunctions = &vkFunctions,
			.instance = instance,
		};
		VK_ASSERT(vmaCreateAllocator(&allocatorInfo, &allocator), "Failed to create vulkan memory allocator instance.");
		spdlog::trace("VMA instance successfully created.");

		InitCommandPools(vkbDevice);
	}

	void VulkanInstance::Cleanup()
	{
		{
			std::lock_guard lock(graphicsCmdPoolListMutex);
			graphicsCmdPools.clear();
		}

		{
			std::lock_guard lock(computeCmdPoolListMutex);
			computeCmdPools.clear();
		}

		{
			std::lock_guard lock(transferCmdPoolListMutex);
			transferCmdPools.clear();
		}

		{
			std::lock_guard lock(presentCmdPoolListMutex);
			presentCmdPools.clear();
		}

		vmaDestroyAllocator(allocator);
		allocator = VK_NULL_HANDLE;

		swapchain.reset();

		vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
		vkDestroySurfaceKHR(instance, surface, nullptr);
		surface = VK_NULL_HANDLE;
		vkb::destroy_debug_utils_messenger(instance, debugMessenger, nullptr);
		debugMessenger = VK_NULL_HANDLE;
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}

	void VulkanInstance::InitCommandPools(const vkb::Device& vkbDevice)
	{
		const auto graphicsQueueRes = vkbDevice.get_queue(vkb::QueueType::graphics);
		SY_ASSERT(graphicsQueueRes.has_value(), "Failed to get graphics queue from logical device of vulkan.");
		graphicsQueue = graphicsQueueRes.value();
		graphicsQueueFamilyIdx = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
		spdlog::trace("Graphics Queue successfully acquired. Family Index: {}.", graphicsQueueFamilyIdx);

		const auto computeQueueRes = vkbDevice.get_queue(vkb::QueueType::compute);
		SY_ASSERT(computeQueueRes.has_value(), "Failed to get compute queue from logical device of vulkan.");
		computeQueue = computeQueueRes.value();
		computeQueueFamilyIdx = vkbDevice.get_queue_index(vkb::QueueType::compute).value();
		spdlog::trace("Compute Queue successfully acquired. Family Index: {}.", computeQueueFamilyIdx);

		const auto transferQueueRes = vkbDevice.get_queue(vkb::QueueType::transfer);
		SY_ASSERT(transferQueueRes.has_value(), "Failed to get transfer queue from logical device of vulkan.");
		transferQueue = computeQueueRes.value();
		transferQueueFamilyIdx = vkbDevice.get_queue_index(vkb::QueueType::transfer).value();
		spdlog::trace("Transfer Queue successfully acquired. Family Index: {}.", transferQueueFamilyIdx);

		const auto presentQueueRes = vkbDevice.get_queue(vkb::QueueType::present);
		SY_ASSERT(presentQueueRes.has_value(), "Failed to get present queue from logical device of vulkan.");
		presentQueue = presentQueueRes.value();
		presentQueueFamilyIdx = vkbDevice.get_queue_index(vkb::QueueType::present).value();
		spdlog::trace("Present Queue successfully acquired. Family Index: {}.", presentQueueFamilyIdx);

		auto& test = RequestGraphicsCommandPool();
		test.RequestCommandBuffer("test cmd buffer 0");
		test.RequestCommandBuffer("test cmd buffer 1");
	}

	CommandPool& VulkanInstance::RequestCommandPool(const EQueueType queueType, std::vector<std::unique_ptr<CommandPool>>& poolList, std::mutex& listMutex)
	{
		thread_local CommandPool* threadPool = nullptr;
		if (threadPool == nullptr)
		{
			threadPool = new CommandPool(*this, queueType);
			std::lock_guard<std::mutex> lock(listMutex);
			poolList.push_back(std::unique_ptr<CommandPool>(threadPool));
		}

		return *threadPool;
	}
}
