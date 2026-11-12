#include "DialogueCamera.h"

#include <spdlog/sinks/basic_file_sink.h>

namespace
{
	[[nodiscard]] std::string FormatRuntimeVersion(std::uint32_t version)
	{
		return std::format("{}.{}.{}.{}", version >> 24, (version >> 16) & 0xFF, (version >> 4) & 0xFFF, version & 0xF);
	}

	void InitializeLog()
	{
		const auto directory = SFSE::GetLogDirectory();
		if (!directory) {
			return;
		}
		std::error_code error;
		std::filesystem::create_directories(*directory, error);
		if (error) {
			logger::warn("Could not create the SFSE log directory: {}", error.message());
			return;
		}
		const auto path = *directory / (std::string(PLUGIN_NAME) + ".log");
		std::shared_ptr<spdlog::logger> output;
		try {
			output = spdlog::basic_logger_mt(PLUGIN_DLL_NAME, path.string(), true);
		} catch (const spdlog::spdlog_ex& failure) {
			logger::warn("Could not open the plugin log: {}", failure.what());
			return;
		}
#ifdef NDEBUG
		output->set_level(spdlog::level::info);
		output->flush_on(spdlog::level::info);
#else
		output->set_level(spdlog::level::debug);
		output->flush_on(spdlog::level::debug);
#endif
		output->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] %v");
		spdlog::set_default_logger(std::move(output));
	}

	// ReSharper disable once CppParameterMayBeConstPtrOrRef
	void OnMessage(SFSE::MessagingInterface::Message* a_message)
	{
		if (!a_message || a_message->type != SFSE::MessagingInterface::kPostDataLoad)
		{
			return;
		}

		logger::info("SFSE post-data-load received; installing dialogue camera support.");
		if (!ToggleDialogueCameraSF::DialogueCamera::Install())
		{
			logger::critical("Dialogue camera installation failed.");
		}
	}
}

extern "C" __declspec(dllexport) constinit SFSE::PluginVersionData SFSEPlugin_Version{
	1,
	PLUGIN_PACKED_VERSION,
	PLUGIN_NAME,
	PLUGIN_AUTHOR,
	1U << 2,
	1U << 3,
	{ SFSE::RUNTIME_SF_1_16_244 },
	0, 0, 0
};

extern "C" __declspec(dllexport) bool SFSEPlugin_Load(const SFSE::LoadInterface* a_sfse)
{
	if (!a_sfse)
	{
		return false;
	}

	InitializeLog();
	const auto runtime = a_sfse->RuntimeVersion();
	if (runtime != SFSE::RUNTIME_SF_1_16_244)
	{
		logger::critical(
			"Unsupported Starfield runtime {}. This build requires {}.",
			FormatRuntimeVersion(runtime),
			FormatRuntimeVersion(SFSE::RUNTIME_SF_1_16_244));
		return false;
	}

	if (!SFSE::Init(*a_sfse, 0)) {
		logger::critical("Could not initialize SFSE and the Address Library.");
		return false;
	}
	const auto messaging = SFSE::GetMessagingInterface();
	if (!messaging)
	{
		logger::critical("Required SFSE MessagingInterface is unavailable.");
		return false;
	}
	if (!messaging->RegisterListener(OnMessage))
	{
		logger::critical("Could not register the SFSE lifecycle listener.");
		return false;
	}

	logger::info("Initialized on Starfield {}; waiting for post-data-load.", FormatRuntimeVersion(runtime));
	return true;
}
