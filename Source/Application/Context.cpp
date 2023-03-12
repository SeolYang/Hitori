#include <PCH.h>
#include <Application/Context.h>
#include <Core/CommandLineParser.h>
#include <Window/Window.h>
#include <Core/Utils.h>
#include <Core/ResourceCache.h>
#include <VK/VulkanContext.h>
#include <VK/ResourceStateTracker.h>
#include <VK/Texture.h>
#include <VK/TextureView.h>
#include <VK/Sampler.h>
#include <VK/DescriptorManager.h>
#include <VK/VulkanRHI.h>
#include <Render/Renderer.h>
#include <Render/Material.h>
#include <Asset/AssetConverter.h>
#include <Game/World.h>
#include <Game/GameContext.h>

#include "VK/TextureBuilder.h"

namespace sy::app
{
	Context::Context(const int argc, char** argv)
	{
		Startup(argc, argv);
	}

	Context::~Context()
	{
		Cleanup();
	}

	void Context::Startup(const int argc, char** argv)
	{
		InitializeLogger();
		InitializeCommandLineParser(argc, argv);

		spdlog::info("Initializing SDL.");
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
		{
			spdlog::critical("Failed to initialize SDL!");
		}

		spdlog::info("Initializing Timer sub-context.");
		timer = std::make_unique<Timer>();
		spdlog::info("Initializing Window sub-context.");
		window = std::make_unique<window::Window>("Test", Extent2D<uint32_t>{ 1280, 720 });
		spdlog::info("Initializing Resource Cache.");
		resourceCache = std::make_unique<ResourceCache>();
		spdlog::info("Initializing Vulkan context.");
		vulkanContext = std::make_unique<vk::VulkanContext>(*window);
		spdlog::info("Initializing Resource State Tracker.");
		resourceStateTracker = std::make_unique<vk::ResourceStateTracker>(*resourceCache);
		spdlog::info("Initializing Default engine resources.");
		InitDefaultEngineResources();
		spdlog::info("Initializing Renderer sub-context.");
		renderer = std::make_unique<render::Renderer>(*window, *vulkanContext, *resourceStateTracker, *resourceCache);
	}

	void Context::InitializeLogger()
	{
		const auto currentTime = std::chrono::system_clock::now();
		const auto localTime = std::chrono::current_zone()->to_local(currentTime);
		const std::string fileName = std::format("LOG_{:%F_%H_%M_%S}.log", localTime);

		fs::path logFilePath = "Logs";
		logFilePath /= fileName;
		const auto consoleSink = std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>();
		const auto logFilePathAnsi = WStringToAnsi(logFilePath.c_str());
		const auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePathAnsi, true);

		const auto sinksInitList =
		{
			std::static_pointer_cast<spdlog::sinks::sink>(consoleSink),
			std::static_pointer_cast<spdlog::sinks::sink>(fileSink)
		};

		spdlog::set_default_logger(std::make_unique<spdlog::logger>("Core", sinksInitList));

#if defined(_DEBUG) || defined(DEBUG)
		spdlog::set_level(spdlog::level::trace);
#else
		spdlog::set_level(spdlog::level::warn);
#endif

		spdlog::info("Logger initialized: output : {}", logFilePathAnsi);
	}

	void Context::InitializeCommandLineParser(const int argc, char** argv)
	{
		spdlog::info("Initializing Command Line Parser sub-context.");
		cmdLineParser = std::make_unique<CommandLineParser>(argc, argv);
		if (cmdLineParser->ShouldConvertAssets())
		{
			asset::ConvertAssets(cmdLineParser->GetAssetPath());
		}
	}

	void Context::InitDefaultEngineResources()
	{
		const std::array white{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
		auto defaultTexBuilder = vk::TextureBuilder::Texture2DShaderResourceTemplate(*vulkanContext)
			.SetName("DefaultWhite")
			.SetExtent(Extent2D<uint32_t>{ 2, 2 })
			.SetFormat(VK_FORMAT_R8G8B8A8_SRGB);

		const auto defaultWhiteTex = resourceCache->Add(defaultTexBuilder.SetDataToTransfer(std::span{ white.data(), white.size() }).Build());
		resourceCache->SetAlias(vk::DefaultWhiteTexture, defaultWhiteTex);

		constexpr std::array<uint32_t, 4> black = { 0x000000ff , 0x000000ff , 0x000000ff , 0x000000ff };
		const auto defaultBlackTex = resourceCache->Add(defaultTexBuilder.SetName("DefaultBlack").SetDataToTransfer(std::span{white.data(), white.size()}).Build());
		resourceCache->SetAlias(vk::DefaultBlackTexture, defaultBlackTex);

		const auto linearSampler = resourceCache->Add<vk::Sampler>(vk::LinearSamplerRepeat, vulkanContext->GetVulkanRHI(), vk::SamplerInfo{});
		resourceCache->SetAlias(vk::LinearSamplerRepeat, linearSampler);

		auto& defaultWhiteTexRef = Unwrap(resourceCache->Load(defaultWhiteTex));
		const auto defaultWhiteTexView = resourceCache->Add<vk::TextureView>(std::format("{}_View", vk::DefaultWhiteTexture), vulkanContext->GetVulkanRHI(), defaultWhiteTexRef, VK_IMAGE_VIEW_TYPE_2D);

		auto& descriptorManager = vulkanContext->GetDescriptorManager();
		const auto defaultMaterial = resourceCache->Add<render::Material>(resourceCache->Add<vk::Descriptor>(descriptorManager.RequestDescriptor(*resourceCache, defaultWhiteTex, defaultWhiteTexView, linearSampler, vk::ETextureState::AnyShaderReadSampledImage)));
		resourceCache->SetAlias(render::DefaultMaterial, defaultMaterial);
	}

	void Context::Cleanup()
	{
		spdlog::info("Clean-up Resource Cache.");
		resourceCache->Clear();

		spdlog::info("Clean-up Renderer sub-context.");
		renderer.reset();

		spdlog::info("Clean-up Resource State Tracker.");
		resourceStateTracker.reset();
		spdlog::info("Clean-up Vulkan context.");
		vulkanContext.reset();

		spdlog::info("Clean-up Window sub-context");
		window.reset();

		spdlog::info("Clean-up Cmd Line Parser sub-context");
		cmdLineParser.reset();
	}

	void Context::Run()
	{
		spdlog::info("Startup main loop.");
        SDL_Event ev;
        bool bExit = false;

        while (!bExit)
        {
			timer->Begin();
			vulkanContext->BeginFrame();

            while (SDL_PollEvent(&ev) != 0)
            {
                if (ev.type == SDL_QUIT)
                {
                    bExit = true;
                }
            }

			vulkanContext->BeginRender();
			renderer->Render();
			vulkanContext->EndRender();

			vulkanContext->EndFrame();
			timer->End();
        }
		spdlog::info("Main loop finished.");
	}
}
