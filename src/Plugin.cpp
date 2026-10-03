#include "PCH.h"
#include "Engine.hpp"
#include "BuildIdentity.hpp"
#include <spdlog/sinks/ostream_sink.h>

namespace {
std::ofstream logFile;
void SetupLog() {
    if (auto directory=SKSE::log::log_directory()) {
        std::filesystem::create_directories(*directory);
        logFile.open(*directory / L"FreedomControl.log",std::ios::out|std::ios::trunc);
        if (logFile) {
            auto sink=std::make_shared<spdlog::sinks::ostream_sink_mt>(logFile,true);
            spdlog::set_default_logger(std::make_shared<spdlog::logger>("FreedomControl",sink));
        }
    }
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
}
}
extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version=[] {
    SKSE::PluginVersionData data;
    data.PluginVersion(REL::Version{0,5,5,0});
    data.PluginName("FreedomControl");
    data.AuthorName("FreedomControl project");
    data.UsesAddressLibrary();
    data.UsesUpdatedStructs();
    // This plugin targets the older 1.6.1170 runtime. Do not advertise the newer
    // AddressLibraryV5 metadata bit that current CommonLib defaults to.
    data.versionIndependenceEx=0;
    data.CompatibleVersions({REL::Version{1,6,1170,0}});
    return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if (!skse || skse->IsEditor() || skse->RuntimeVersion()!=REL::Version{1,6,1170,0}) {
        OutputDebugStringW(L"FreedomControl: only Skyrim 1.6.1170.0 is supported by this build.\n");
        return false;
    }
    try {
        SKSE::Init(skse);
        SetupLog();
        spdlog::info("FreedomControl 0.5.5 SexFast16 + COMPILE17 + VMARGS18 zh-CN experimental | runtime 1.6.1170.0 | CommonLib {}",FC_COMMONLIB_COMMIT);
        spdlog::info("Build ID: {}",&FreedomControl_BuildID[0]);
        spdlog::info("Source fingerprint: {}",&FreedomControl_SourceSHA256[0]);
        auto* messaging=SKSE::GetMessagingInterface();
        if (!messaging || !SKSE::GetTaskInterface() || !SKSE::GetSerializationInterface()) {
            spdlog::error("Required SKSE interfaces are unavailable."); return false;
        }
        fc::Engine::Get().Initialize();
        if (!messaging->RegisterListener([](SKSE::MessagingInterface::Message* m) { fc::Engine::Get().Message(m); })) return false;
        return true;
    } catch (const std::exception& e) {
        OutputDebugStringA(e.what());
        return false;
    }
}
