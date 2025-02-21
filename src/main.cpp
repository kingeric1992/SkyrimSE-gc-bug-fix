#include <version.h>
#include "bugfix.h"
#include "gctracehooks.h"
#include "variabletracehooks.h"

static auto iniPath = "Data/SKSE/Plugins/GCBugFix.ini";
static int logFlushTime{};
static int logGCPasses{};
static int traceGCVariables{};
static int applyFix{};

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD ul_reason_for_call,
	LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hModule);

		logFlushTime		= GetPrivateProfileIntA("GC", "LogFlushTime", 5, iniPath);
		logGCPasses			= GetPrivateProfileIntA("GC", "LogPasses", 0, iniPath);
		traceGCVariables	= GetPrivateProfileIntA("GC", "TraceVariables", 0, iniPath);
		applyFix			= GetPrivateProfileIntA("GC", "ApplyFix", 1, iniPath);

#ifndef NDEBUG
		while (!IsDebuggerPresent()) Sleep(100);
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		if (!logGCPasses && !traceGCVariables)
			return true;

		auto path = logger::log_directory();
		if (!path) {
			return false;
		}

		*path /= fmt::format("{}.log", BUILD_PROJECT_NAME);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
		log->set_level(spdlog::level::trace);
#else
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::warn);
#endif

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S:%e TID %5t] %v"s);

		spdlog::info(
			"{} v{}.{}\n"
			"==============================\n"
			"{}\n\nLicense:\n\tGPLv3 with Modding exception.\n{}", 
			BUILD_PROJECT_NAME, BUILD_VERSION_MAJOR, BUILD_VERSION_MINOR, BUILD_DESCRIPTION, BUILD_HOMEPAGE_URL);

	}
	return true;
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* SKSE, SKSE::PluginInfo* Info)
{

	Info->infoVersion = SKSE::PluginInfo::kVersion;
	Info->name = BUILD_PROJECT_NAME;
	Info->version = BUILD_VERSION_MAJOR;

	if (SKSE->IsEditor()) {
		logger::critical("Loaded in editor"sv);
		return false;
	}

	const auto ver = SKSE->RuntimeVersion();

	if (ver < REL::Relocate(SKSE::RUNTIME_SSE_1_5_3, SKSE::RUNTIME_SSE_1_6_317)) {
		SKSE::stl::report_and_fail(
			fmt::format("{} does not support runtime v{}.",
				Info->name,
				ver.string()));
	}

	return true;
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() noexcept {
	SKSE::PluginVersionData data = {};

	data.PluginVersion({ BUILD_VERSION_MAJOR, BUILD_VERSION_MINOR, 0 });
	data.PluginName(BUILD_PROJECT_NAME);
	data.AuthorName("Kingeric1992");
	data.UsesAddressLibrary(true);
	data.UsesSigScanning(true);
	data.HasNoStructUse(true);
	//data.CompatibleVersions({ SKSE::RUNTIME_1_10_162, SKSE::RUNTIME_1_10_163 });

	return data;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* SKSE)
{
	SKSE::Init(SKSE);

	if (logGCPasses != 0 || traceGCVariables != 0)
		spdlog::flush_every(std::chrono::seconds(logFlushTime));

	logger::info("LogFlushTime = {}", logFlushTime);
	logger::info("LogPasses = {}", logGCPasses);
	logger::info("TraceVariables = {}", traceGCVariables);
	logger::info("ApplyFix = {}", applyFix);

	// Set up hooks
	if ((logGCPasses || traceGCVariables) && SKSE::GetTrampoline().empty())
		SKSE::AllocTrampoline(512);

	if (logGCPasses)
		GCTraceHooks::InstallHooks();

	if (traceGCVariables)
		VariableTraceHooks::InstallHooks();

	if (applyFix)
		return Bugfix::InstallHooks();

	return true;
}
