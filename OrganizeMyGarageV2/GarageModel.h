#pragma once
#include "ItemStructs.h"
#include <vector>
#include <memory>
#include <random>
#include <unordered_set>
#include "PersistentStorage.h"

namespace {
	constexpr char kCvarGlobalNotificationsEnabled[] = "cl_notifications_enabled_beta"; // at least it's out of alpha phase
	constexpr char kCvarCycleFavEnabled[] = "omg_cycle_favorite_presets_enabled";
	constexpr char kCvarCycleFavShuffle[] = "omg_cycle_favorite_presets_shuffle";
	constexpr char kCvarCycleFavNotify[] = "omg_cycle_favorite_presets_notify";
	constexpr char kCvarCycleFavList[] = "omg_cycle_favorites";
	constexpr char kCvarRandomGoalExplosion[] = "omg_random_goal_explosion";
	constexpr char kCvarCycleFavListDelimiter = '\f'; // hope noone uses this
}

class InventoryModel;

class GarageModel
{
public:
	explicit GarageModel(std::shared_ptr<GameWrapper> gw, std::shared_ptr<InventoryModel> inv, std::shared_ptr<PersistentStorage> ps, std::shared_ptr<CVarManagerWrapper> cv);

	void RefreshPresets();
	void RefreshEquippedIndex();
	void EquipPreset(const std::string& name) const;
	[[nodiscard]] int GetEquippedIndex() const;
	[[nodiscard]] std::vector<PresetData> GetCurrentPresets() const;
	void SwapPreset(size_t a, size_t b) const;
	void MovePreset(size_t from, size_t to) const;
	void SwapPresetPrivate(const std::string& a, const std::string& b) const;
	[[nodiscard]] LoadoutSetWrapper AddPreset() const;
	void CopyPreset(const std::string& presetName) const;
	void DeletePreset(const std::string& presetName) const;
	void RenamePreset(size_t presetIndex, const std::string& newName) const;
	void EquipItem(size_t presetIndex, const OnlineProdData& itemData, int teamIndex) const;
	const std::vector<PresetData>& GetPresets() const;
	size_t GetCurrentPresetIndex() const;


	void UpdateFavorite(const std::string& name, bool isFavorite);
	bool IsFavorite(const std::string& name) const;
	void LoadFavorites();
	bool GetFavoritesEnabled() const;
	bool SetFavoritesEnabled(bool enabled);
	bool GetFavoritesShuffle() const;
	bool SetFavoritesShuffle(bool shuffle);
	bool GetFavoritesNotify() const;
	bool SetFavoritesNotify(bool notify);
	bool GetRandomGoalExplosionEnabled() const;
	bool SetRandomGoalExplosionEnabled(bool enabled);

private:
	void SaveFavorites() const;
	void EquipNextFavoritePreset(const char* evName);
	void RandomGoalExplosion(const char* evName);
	void EquipPreset(size_t presetIndex);
	void EquipPresetPrivate(const std::string& name) const;
	PresetData GetPresetData(const LoadoutSetWrapper& loadout) const;
	std::vector<OnlineProdData> GetItemData(const LoadoutWrapper& loadout) const;

	size_t equippedPresetIndex = 0;
	std::shared_ptr<InventoryModel> m_inv;
	std::shared_ptr<PersistentStorage> m_ps;
	std::shared_ptr<CVarManagerWrapper> m_cv;
	std::shared_ptr<GameWrapper> m_gw;
	std::vector<PresetData> presets;
	std::unordered_set<std::string> favorites;
	bool preventSettingNextFavoritePreset{ false };
};
