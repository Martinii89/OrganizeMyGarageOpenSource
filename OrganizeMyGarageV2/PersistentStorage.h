#pragma once
#include <filesystem>
#include <map>


class PersistentStorage
{
public:

    /// <summary>
    /// PersistentStorage is a convenient class for making sure your cvar values are always loaded, and never lost if the main config is rewritten while your plugin is unloaded. 
    /// </summary>
    /// <param name="plugin">Pointer to the main plugin</param>
    /// <param name="storage_file_name">The file of the config file excluding the file extension</param>
    /// <param name="auto_write">Automatically write cvar value to persistent storage once changed</param>
    /// <param name="auto_load">Automatically load the cfg after 0.1s</param>
    explicit PersistentStorage(BakkesMod::Plugin::BakkesModPlugin* plugin, const std::string& storage_file_name, bool auto_write = false, bool auto_load = false);

    PersistentStorage(const PersistentStorage& other) = delete;
    PersistentStorage(PersistentStorage&& other) noexcept = delete;
    PersistentStorage& operator=(const PersistentStorage& other) = delete;
    PersistentStorage& operator=(PersistentStorage&& other) noexcept = delete;
    ~PersistentStorage();

    /// <summary>
    /// Writes the cvar values to disk
    /// </summary>
    void WritePersistentStorage();

    /// <summary>
    /// Loads the cvar values from disk
    /// </summary>
    void Load();

	/// <summary>
	/// Registers a cvar with bakkesmod in addition to adding it to the list of persistent cvars.
	/// </summary>
	/// <param name="cvar"></param>
	/// <param name="defaultValue"></param>
	/// <param name="desc"></param>
	/// <param name="searchAble"></param>
	/// <param name="hasMin"></param>
	/// <param name="min"></param>
	/// <param name="hasMax"></param>
	/// <param name="max"></param>
	/// <param name="saveToCfg"></param>
	/// <returns>The registered cvar</returns>
	CVarWrapper RegisterPersistentCvar(const std::string& cvar, const std::string& defaultValue, const std::string& desc = "", bool searchAble = true, bool hasMin = false, float min = 0, bool hasMax = false, float max = 0, bool saveToCfg = true);
	
	/// <summary>
	/// Adds cvars to be persistent. The cvars have to be registered first.
	/// </summary>
	/// <param name="cvars">List of cvar names</param>
	void AddCVars(std::initializer_list<std::string> cvars);

    /// <summary>
    /// Add a single cvar to be persistent
    /// </summary>
    /// <param name="s">The cvar name</param>
    void AddCVar(const std::string& s);

	/// <summary>
	/// Adds cvars to be persistent. The cvars have to be registered first.
	/// </summary>
	/// <param name="cvars">List of cvar names</param>
    template <class Iterable>
    void AddCVars(const Iterable cvars)
    {
        for (const auto& cvar_name : cvars)
        {
            AddCVar(cvar_name);
        }
    }

private:
	std::shared_ptr<CVarManagerWrapper> cv_;
    std::filesystem::path storage_file_{ "" };
    bool auto_write_ = false;
	bool loaded_ = false;

	struct CvarCacheItem
	{
		std::string value;
		std::string description;
		explicit CvarCacheItem(CVarWrapper cvar);
	};
	
	std::map<std::string, CvarCacheItem> cvar_cache_;

    static std::filesystem::path GetStorageFilePath(const std::shared_ptr<GameWrapper>& gw, std::string file_name);

	void OnPersistentCvarChanged(const std::string& old, CVarWrapper changed_cvar);


};

