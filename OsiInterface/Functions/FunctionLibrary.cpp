#include <stdafx.h>
#include <OsirisProxy.h>
#include "FunctionLibrary.h"
#include <Version.h>
#include <fstream>
#include "json/json.h"

namespace osidbg
{
	namespace func
	{

		char const * ItemGetStatsIdProxy(char const * itemGuid)
		{
			auto item = FindItemByNameGuid(itemGuid);
			if (item == nullptr) {
				OsiError("Item '" << itemGuid << "' does not exist!");
				return nullptr;
			}

			if (!item->StatsId.Str) {
				OsiError("Item '" << itemGuid << "' has no stats ID!");
				return nullptr;
			} else {
				return item->StatsId.Str;
			}
		}

		void ShowErrorMessage(OsiArgumentDesc const & args)
		{
			auto message = args[0].String;

			auto wmsg = FromUTF8(message);
			gOsirisProxy->GetLibraryManager().ShowStartupError(wmsg, false, false);
		}

		bool IsModLoaded(OsiArgumentDesc & args)
		{
			auto modUuid = ToFixedString(args[0].String);
			auto & loaded = args[1].Int32;

			auto modManager = GetModManager();
			if (modManager == nullptr) {
				OsiError("Mod manager not available");
				return false;
			}

			if (!modUuid) {
				loaded = 0;
				return true;
			}

			loaded = 0;
			auto & mods = modManager->BaseModule.LoadOrderedModules.Set;
			for (uint32_t i = 0; i < mods.Size; i++) {
				auto const & mod = mods.Buf[i];
				if (mod.Info.ModuleUUID == modUuid) {
					loaded = 1;
				}
			}

			return true;
		}

		bool GetVersion(OsiArgumentDesc & args)
		{
			args[0].Int32 = CurrentVersion;
			return true;
		}

		void OsiLuaReset(OsiArgumentDesc const & args)
		{
			auto bootstrapMods = args[0].Int32 == 1;

			auto & ext = ExtensionState::Get();
			ext.LuaReset(bootstrapMods);
		}

		void OsiLuaLoad(OsiArgumentDesc const & args)
		{
			LuaStatePin lua(ExtensionState::Get());
			if (!lua) {
				OsiError("Called when the Lua VM has not been initialized!");
				return;
			}

			auto mod = args[0].String;
			auto fileName = args[1].String;

			if (strstr(fileName, "..") != nullptr) {
				OsiError("Illegal file name");
				return;
			}

			ExtensionState::Get().LuaLoadGameFile(mod, fileName);
		}

		void OsiLuaCall(OsiArgumentDesc const & args)
		{
			LuaStatePin lua(ExtensionState::Get());
			if (!lua) {
				OsiError("Called when the Lua VM has not been initialized!");
				return;
			}

			auto func = args[0].String;
			auto numArgs = args.Count() - 1;
			std::vector<OsiArgumentValue> luaArgs;
			luaArgs.resize(numArgs);

			for (uint32_t i = 0; i < numArgs; i++) {
				luaArgs[i] = args[i + 1];
			}

			lua->Call(func, luaArgs);
		}

#define SAFE_PATH_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_."

		std::optional<std::string> GetPathForScriptIo(char const * scriptPath)
		{
			std::string path = scriptPath;

			if (path.find_first_not_of(SAFE_PATH_CHARS) != std::string::npos
				|| path.find("..") != std::string::npos) {
				OsiError("Illegal file name for external file access: '" << path << "'");
				return {};
			}

			auto storageRoot = gOsirisProxy->GetLibraryManager().ToPath("", PathRootType::GameStorage);
			if (storageRoot.empty()) {
				OsiError("Could not fetch game storage path");
				return {};
			}

			storageRoot += "/Osiris Data";

			auto storageRootWstr = FromUTF8(storageRoot);
			BOOL created = CreateDirectory(storageRootWstr.c_str(), NULL);
			if (created == FALSE) {
				DWORD lastError = GetLastError();
				if (lastError != ERROR_ALREADY_EXISTS) {
					OsiError("Could not create storage root directory: " << storageRoot);
					return {};
				}
			}

			return storageRoot + "/" + path;
		}

		bool LoadFile(OsiArgumentDesc & args)
		{
			auto path = args[0].String;
			auto & contents = args[1].String;

			auto absolutePath = GetPathForScriptIo(path);
			if (!absolutePath) return false;

			std::ifstream f(absolutePath->c_str(), std::ios::in | std::ios::binary);
			if (!f.good()) {
				OsiError("Could not open file: '" << path << "'");
				return false;
			}

			std::string body;
			f.seekg(0, std::ios::end);
			body.resize(f.tellg());
			f.seekg(0, std::ios::beg);
			f.read(body.data(), body.size());

			// FIXME - who owns the string?
			contents = _strdup(body.c_str());

			return true;
		}

		void SaveFile(OsiArgumentDesc const & args)
		{
			auto path = args[0].String;
			auto contents = args[1].String;

			auto absolutePath = GetPathForScriptIo(path);
			if (!absolutePath) return;

			std::ofstream f(absolutePath->c_str(), std::ios::out | std::ios::binary);
			if (!f.good()) {
				OsiError("Could not open file: '" << path << "'");
				return;
			}

			f.write(contents, strlen(contents));
		}

		void BreakOnCharacter(OsiArgumentDesc const & args)
		{
			auto character = FindCharacterByNameGuid(args[0].String);
			OsiWarn("GotIt");
		}

		void BreakOnItem(OsiArgumentDesc const & args)
		{
			auto item = FindItemByNameGuid(args[0].String);
			OsiWarn("GotIt");
		}

		void DoExperiment(OsiArgumentDesc const & args)
		{
			auto character = FindCharacterByNameGuid(args[0].String);
			if (character) {
				auto stats = character->Stats->DynamicStats[1];
				stats->PoisonResistance += 50;
				stats->APStart += 3;
				stats->APRecovery += 3;

				auto stats0 = character->Stats->DynamicStats[0];
				stats0->PoisonResistance += 50;
				OsiError("DoExperiment(): Applied to character");
			}

			auto item = FindItemByNameGuid(args[1].String);
			if (item) {
				auto stats = item->StatsDynamic->DynamicAttributes_Start[1];
				stats->FireResistance += 50;

				auto stats0 = item->StatsDynamic->DynamicAttributes_Start[0];
				stats0->FireResistance += 50;
				OsiError("DoExperiment(): Applied to item");
			}

			OsiError("Nothing to see here");
		}
	}

	void ExtensionState::Reset()
	{
		DamageHelpers.Clear();

		time_t tm;
		OsiRng.seed(time(&tm));
	}

	void ExtensionState::LoadConfigs()
	{
		auto modManager = GetModManager();
		if (modManager == nullptr) {
			OsiError("Mod manager not available");
			return;
		}

		auto & mods = modManager->BaseModule.LoadOrderedModules.Set;

		unsigned numConfigs{ 0 };
		for (uint32_t i = 0; i < mods.Size; i++) {
			auto const & mod = mods.Buf[i];
			auto dir = ToUTF8(mod.Info.Directory.GetPtr());
			auto configFile = "Mods/" + dir + "/OsiToolsConfig.json";
			auto reader = gOsirisProxy->GetLibraryManager().MakeFileReader(configFile);

			if (reader.IsLoaded()) {
				LoadConfig(mod, reader.ToString());
				numConfigs++;
			}
		}

		if (numConfigs > 0) {
			Debug("%d mod configuration(s) configuration loaded.", numConfigs);
			Debug("Extensions=%d, Lua=%d, CustomStats=%d, CustomStatsPane=%d, FormulaOverrides=%d, MinVersion=%d",
				EnableExtensions, EnableLua, EnableCustomStats, EnableCustomStatsPane,
				EnableFormulaOverrides, MinimumVersion);
		}

		if (CurrentVersion < MinimumVersion && HighestVersionMod != nullptr) {
			std::wstringstream msg;
			msg << L"Module \"" << HighestVersionMod->Info.Name.GetPtr() << "\" requires extension version "
				<< MinimumVersion << "; current version is v" << CurrentVersion;
			gOsirisProxy->GetLibraryManager().ShowStartupError(msg.str(), false, true);
		}
	}

	void ExtensionState::LoadConfig(Module const & mod, std::string const & config)
	{
		Json::CharReaderBuilder factory;
		auto reader = factory.newCharReader();

		Json::Value root;
		std::string errs;
		if (!reader->parse(config.c_str(), config.c_str() + config.size(), &root, &errs)) {
			OsiError("Unable to parse configuration for mod '" << ToUTF8(mod.Info.Name.GetPtr()) << "': " << errs);
			return;
		}

		LoadConfig(mod, root);

		delete reader;
	}

	std::optional<bool> GetConfigBool(Json::Value & config, std::string const & key)
	{
		auto value = config[key];
		if (!value.isNull()) {
			if (value.isBool()) {
				return value.asBool();
			} else {
				OsiError("Config option '" << key << "' should be a boolean.");
				return {};
			}
		} else {
			return {};
		}
	}

	std::optional<int32_t> GetConfigInt(Json::Value & config, std::string const & key)
	{
		auto value = config[key];
		if (!value.isNull()) {
			if (value.isInt()) {
				return value.asInt();
			} else {
				OsiError("Config option '" << key << "' should be an integer.");
				return {};
			}
		} else {
			return {};
		}
	}

	void ExtensionState::LoadConfig(Module const & mod, Json::Value & config)
	{
		auto extendOsiris = GetConfigBool(config, "ExtendOsiris");
		if (extendOsiris && *extendOsiris) {
			EnableExtensions = true;
		}

		auto lua = GetConfigBool(config, "Lua");
		if (lua && *lua) {
			EnableLua = true;
		}

		auto customStats = GetConfigBool(config, "UseCustomStats");
		if (customStats && *customStats) {
			EnableCustomStats = true;
		}

		auto customStatsPane = GetConfigBool(config, "UseCustomStatsPane");
		if (customStatsPane && *customStatsPane) {
			EnableCustomStatsPane = true;
		}

		auto formulaOverrides = GetConfigBool(config, "FormulaOverrides");
		if (formulaOverrides && *formulaOverrides) {
			EnableFormulaOverrides = true;
		}

		auto preprocessStory = GetConfigBool(config, "PreprocessStory");
		if (preprocessStory && *preprocessStory) {
			PreprocessStory = true;
		}

		auto version = GetConfigInt(config, "RequiredExtensionVersion");
		if (version && MinimumVersion < (uint32_t)*version) {
			MinimumVersion = (uint32_t)*version;
			HighestVersionMod = &mod;
		}
	}

	CustomFunctionLibrary::CustomFunctionLibrary(class OsirisProxy & osiris)
		: osiris_(osiris)
	{}

	void CustomFunctionLibrary::Register()
	{
		auto & functionMgr = osiris_.GetCustomFunctionManager();
		functionMgr.BeginStaticRegistrationPhase();

		RegisterHelperFunctions();
		RegisterMathFunctions();
		RegisterStatFunctions();
		RegisterStatusFunctions();
		RegisterGameActionFunctions();
		RegisterProjectileFunctions();
		RegisterHitFunctions();
		RegisterPlayerFunctions();
		RegisterItemFunctions();
		RegisterCharacterFunctions();
		RegisterCustomStatFunctions();

		auto breakOnCharacter = std::make_unique<CustomCall>(
			"NRD_BreakOnCharacter",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In }
			},
			&func::BreakOnCharacter
		);
		functionMgr.Register(std::move(breakOnCharacter));

		auto breakOnItem = std::make_unique<CustomCall>(
			"NRD_BreakOnItem",
			std::vector<CustomFunctionParam>{
				{ "Item", ValueType::ItemGuid, FunctionArgumentDirection::In }
			},
			&func::BreakOnItem
		);
		functionMgr.Register(std::move(breakOnItem));

		auto experiment = std::make_unique<CustomCall>(
			"NRD_Experiment",
			std::vector<CustomFunctionParam>{
				{ "Arg1", ValueType::String, FunctionArgumentDirection::In },
				{ "Arg2", ValueType::String, FunctionArgumentDirection::In },
				{ "Arg3", ValueType::String, FunctionArgumentDirection::In },
			},
			&func::DoExperiment
		);
		functionMgr.Register(std::move(experiment));

		auto showError = std::make_unique<CustomCall>(
			"NRD_ShowErrorMessage",
			std::vector<CustomFunctionParam>{
				{ "Message", ValueType::String, FunctionArgumentDirection::In }
			},
			&func::ShowErrorMessage
		);
		functionMgr.Register(std::move(showError));

		auto isModLoaded = std::make_unique<CustomQuery>(
			"NRD_IsModLoaded",
			std::vector<CustomFunctionParam>{
				{ "ModUuid", ValueType::GuidString, FunctionArgumentDirection::In },
				{ "IsLoaded", ValueType::Integer, FunctionArgumentDirection::Out }
			},
			&func::IsModLoaded
		);
		functionMgr.Register(std::move(isModLoaded));

		auto getVersion = std::make_unique<CustomQuery>(
			"NRD_GetVersion",
			std::vector<CustomFunctionParam>{
				{ "Version", ValueType::Integer, FunctionArgumentDirection::Out }
			},
			&func::GetVersion
		);
		functionMgr.Register(std::move(getVersion));

		auto luaReset = std::make_unique<CustomCall>(
			"NRD_LuaReset",
			std::vector<CustomFunctionParam>{
				{ "BootstrapMods", ValueType::Integer, FunctionArgumentDirection::In }
			},
			&func::OsiLuaReset
		);
		functionMgr.Register(std::move(luaReset));

		auto luaLoad = std::make_unique<CustomCall>(
			"NRD_LuaLoad",
			std::vector<CustomFunctionParam>{
				{ "ModNameGuid", ValueType::GuidString, FunctionArgumentDirection::In },
				{ "FileName", ValueType::String, FunctionArgumentDirection::In }
			},
			&func::OsiLuaLoad
		);
		functionMgr.Register(std::move(luaLoad));

		auto luaCall0 = std::make_unique<CustomCall>(
			"NRD_LuaCall",
			std::vector<CustomFunctionParam>{
				{ "Func", ValueType::String, FunctionArgumentDirection::In }
			},
			&func::OsiLuaCall
		);
		functionMgr.Register(std::move(luaCall0));

		auto luaCall1 = std::make_unique<CustomCall>(
			"NRD_LuaCall",
			std::vector<CustomFunctionParam>{
				{ "Func", ValueType::String, FunctionArgumentDirection::In },
				{ "Arg1", ValueType::None, FunctionArgumentDirection::In }
			},
			&func::OsiLuaCall
		);
		functionMgr.Register(std::move(luaCall1));

		auto luaCall2 = std::make_unique<CustomCall>(
			"NRD_LuaCall",
			std::vector<CustomFunctionParam>{
				{ "Func", ValueType::String, FunctionArgumentDirection::In },
				{ "Arg1", ValueType::None, FunctionArgumentDirection::In },
				{ "Arg2", ValueType::None, FunctionArgumentDirection::In }
			},
			&func::OsiLuaCall
		);
		functionMgr.Register(std::move(luaCall2));
		
		auto luaCall3 = std::make_unique<CustomCall>(
			"NRD_LuaCall",
			std::vector<CustomFunctionParam>{
				{ "Func", ValueType::String, FunctionArgumentDirection::In },
				{ "Arg1", ValueType::None, FunctionArgumentDirection::In },
				{ "Arg2", ValueType::None, FunctionArgumentDirection::In },
				{ "Arg3", ValueType::None, FunctionArgumentDirection::In }
			},
			&func::OsiLuaCall
		);
		functionMgr.Register(std::move(luaCall3));
		
		auto luaCall4 = std::make_unique<CustomCall>(
			"NRD_LuaCall",
			std::vector<CustomFunctionParam>{
				{ "Func", ValueType::None, FunctionArgumentDirection::In },
				{ "Arg1", ValueType::None, FunctionArgumentDirection::In },
				{ "Arg2", ValueType::None, FunctionArgumentDirection::In },
				{ "Arg3", ValueType::None, FunctionArgumentDirection::In },
				{ "Arg4", ValueType::None, FunctionArgumentDirection::In }
			},
			&func::OsiLuaCall
		);
		functionMgr.Register(std::move(luaCall4));
		
		auto luaCall5 = std::make_unique<CustomCall>(
			"NRD_LuaCall",
			std::vector<CustomFunctionParam>{
				{ "Func", ValueType::String, FunctionArgumentDirection::In },
				{ "Arg1", ValueType::None, FunctionArgumentDirection::In },
				{ "Arg2", ValueType::None, FunctionArgumentDirection::In },
				{ "Arg3", ValueType::None, FunctionArgumentDirection::In },
				{ "Arg4", ValueType::None, FunctionArgumentDirection::In },
				{ "Arg5", ValueType::None, FunctionArgumentDirection::In }
			},
			&func::OsiLuaCall
		);
		functionMgr.Register(std::move(luaCall5));

		auto loadFile = std::make_unique<CustomQuery>(
			"NRD_LoadFile",
			std::vector<CustomFunctionParam>{
				{ "Path", ValueType::String, FunctionArgumentDirection::In },
				{ "Contents", ValueType::String, FunctionArgumentDirection::Out }
			},
			&func::LoadFile
		);
		functionMgr.Register(std::move(loadFile));

		auto saveFile = std::make_unique<CustomCall>(
			"NRD_SaveFile",
			std::vector<CustomFunctionParam>{
				{ "Path", ValueType::String, FunctionArgumentDirection::In },
				{ "Contents", ValueType::String, FunctionArgumentDirection::In }
			},
			&func::SaveFile
		);
		functionMgr.Register(std::move(saveFile));

		functionMgr.EndStaticRegistrationPhase();
	}

	void CustomFunctionLibrary::PostStartup()
	{
		if (!ExtensionState::Get().EnableExtensions) {
			return;
		}

		if (PostLoaded) {
			return;
		}

		using namespace std::placeholders;

		osiris_.GetLibraryManager().StatusHitEnter.AddPreHook(
			std::bind(&CustomFunctionLibrary::OnStatusHitEnter, this, _1)
		);
		osiris_.GetLibraryManager().StatusHealEnter.AddPreHook(
			std::bind(&CustomFunctionLibrary::OnStatusHealEnter, this, _1)
		);
		osiris_.GetLibraryManager().CharacterHitHook.SetWrapper(
			std::bind(&CustomFunctionLibrary::OnCharacterHit, this, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13)
		);
		osiris_.GetLibraryManager().ApplyStatusHook.SetWrapper(
			std::bind(&CustomFunctionLibrary::OnApplyStatus, this, _1, _2, _3)
		);

		PostLoaded = true;
	}

	void CustomFunctionLibrary::OnBaseModuleLoaded()
	{
		Debug("CustomFunctionLibrary::OnBaseModuleLoaded(): Re-initializing module state.");
		auto & functionMgr = osiris_.GetCustomFunctionManager();
		functionMgr.ClearDynamicEntries();

		// FIXME - move extension state here?
		gCharacterStatsGetters.ResetExtension();

		ExtensionState::Get().LuaReset(true);
	}

	ExtensionState & ExtensionState::Get()
	{
		return gOsirisProxy->GetExtensionState();
	}
}
