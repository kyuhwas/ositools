#pragma once

#include "ExtensionHelpers.h"
#include "LuaBinding.h"
#include <random>

namespace Json { class Value; }

namespace osidbg
{
	class DamageHelperPool
	{
	public:
		void Clear();
		DamageHelpers * Create();
		bool Destroy(int64_t handle);
		DamageHelpers * Get(int64_t handle) const;

	private:
		std::unordered_map<int64_t, std::unique_ptr<DamageHelpers>> helpers_;
		int64_t nextHelperId_{ 0 };
	};

	class ExtensionState
	{
	public:
		bool EnableExtensions{ false };
		bool EnableLua{ false };
		bool EnableCustomStats{ false };
		bool EnableCustomStatsPane{ false };
		bool EnableFormulaOverrides{ false };
		bool PreprocessStory{ false };
		uint32_t MinimumVersion{ 0 };
		Module const * HighestVersionMod{ nullptr };

		DamageHelperPool DamageHelpers;
		PendingStatuses PendingStatuses;
		OsiArgumentPool OsiArgumentPool;
		std::unique_ptr<LuaState> Lua;
		std::unique_ptr<ObjectSet<eoc::ItemDefinition>> PendingItemClone;
		std::mt19937_64 OsiRng;
		std::unique_ptr<ShootProjectileApiHelper> ProjectileHelper;

		void Reset();
		void LoadConfigs();
		void LoadConfig(Module const & mod, std::string const & config);
		void LoadConfig(Module const & mod, Json::Value & config);

		void LuaReset();
		void LuaStartup();
		void LuaLoadExternalFile(std::string const & path);
		void LuaLoadGameFile(FileReader * reader);
		void LuaLoadGameFile(std::string const & path);
		void LuaCall(char const * func, char const * arg);

		static ExtensionState & Get();
	};

}