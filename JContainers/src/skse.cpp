#include "skse.h"

#include <ShlObj.h>

#include "skse/PluginAPI.h"
#include "skse/skse_version.h"
#include "skse/PapyrusVM.h"
#include "skse/PapyrusNativeFunctions.h"
#include "skse/GameForms.h"

#include "gtest.h"
#include "tes_context.h"
#include "form_handling.h"
#include "plugin_info.h"

class VMClassRegistry;

extern bool registerAllFunctions(VMClassRegistry *registry);

namespace skse { namespace {

    static PluginHandle					g_pluginHandle = kPluginHandle_Invalid;

    static SKSESerializationInterface	* g_serialization = NULL;
    static SKSEPapyrusInterface			* g_papyrus = NULL;

    void Serialization_Revert(SKSESerializationInterface * intfc) {
        collections::tes_context::instance().clearState();
    }

    void Serialization_Save(SKSESerializationInterface * intfc) {
        if (intfc->OpenRecord(kJStorageChunk, kJSerializationCurrentVersion)) {
            auto data = collections::tes_context::instance().saveToArray();
            intfc->WriteRecordData(data.data(), data.size());
        }
    }

    void Serialization_Load(SKSESerializationInterface * intfc) {
        UInt32	type;
        UInt32	version;
        UInt32	length;

        std::string data;

        while (intfc->GetNextRecordInfo(&type, &version, &length)) {

            if (type == kJStorageChunk && length > 0) {
                data.resize(length, '\0');
                intfc->ReadRecordData((void *)data.data(), data.size());
                break;
            }
        }

        collections::tes_context::instance().loadAll(data, version);
    }

    extern "C" {

        bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
        {
            gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim\\SKSE\\JContainers.log");
            gLog.SetPrintLevel(IDebugLog::kLevel_Error);
            gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);

            // populate info structure
            info->infoVersion = PluginInfo::kInfoVersion;
            info->name = "JContainers";
            info->version = 1;

            // store plugin handle so we can identify ourselves later
            g_pluginHandle = skse->GetPluginHandle();

            _MESSAGE("JContainers %u.%u\n", kJVersionMajor, kJVersionMinor);

            if (skse->isEditor) {
                _MESSAGE("loaded in editor, marking as incompatible");
                return false;
            }
            else if (skse->runtimeVersion != RUNTIME_VERSION_1_9_32_0) {
                _MESSAGE("unsupported runtime version %08X", skse->runtimeVersion);
                return false;
            }

            // get the serialization interface and query its version
            g_serialization = (SKSESerializationInterface *)skse->QueryInterface(kInterface_Serialization);
            if (!g_serialization) {
                _MESSAGE("couldn't get serialization interface");
                return false;
            }

            if (g_serialization->version < SKSESerializationInterface::kVersion) {
                _MESSAGE("serialization interface too old (%d expected %d)", g_serialization->version, SKSESerializationInterface::kVersion);
                return false;
            }

            g_papyrus = (SKSEPapyrusInterface *)skse->QueryInterface(kInterface_Papyrus);

            if (!g_papyrus) {
                _MESSAGE("couldn't get papyrus interface");
                return false;
            }

            return true;
        }

        bool SKSEPlugin_Load(const SKSEInterface * skse)
        {
            _MESSAGE("plugin loaded");

            g_serialization->SetUniqueID(g_pluginHandle, kJStorageChunk);

            g_serialization->SetRevertCallback(g_pluginHandle, Serialization_Revert);
            g_serialization->SetSaveCallback(g_pluginHandle, Serialization_Save);
            g_serialization->SetLoadCallback(g_pluginHandle, Serialization_Load);

            g_papyrus->Register(registerAllFunctions);

            return true;
        }

        __declspec(dllexport) void launchShityTest() {
            testing::runTests(meta<testing::TestInfo>::getListConst());
        }
    };

}

}

namespace skse {

    uint32_t resolve_handle(uint32_t handle) {
        if (g_serialization) {
            UInt64 newId = handle;
            g_serialization->ResolveHandle(handle, &newId);
            return newId;
        }

        return handle;
    }

    const char * fake_plugin_name = "Fake.esm";

    const char * modname_from_index(uint8_t idx) {
        if (g_serialization) {
            DataHandler * dhand = DataHandler::GetSingleton();
            ModInfo * modInfo = idx < _countof(dhand->modList.loadedMods) ? dhand->modList.loadedMods[idx] : nullptr;
            return modInfo ? modInfo->name : nullptr;
        }
        else {
            return fake_plugin_name;
        }
    }

    uint8_t modindex_from_name(const char * name) {
        return g_serialization ? DataHandler::GetSingleton()->GetModIndex(name) : fake_plugin_index;
    }

    TESForm* lookup_form(uint32_t handle) {
        return g_serialization ? LookupFormByID(handle) : nullptr;
    }
}
