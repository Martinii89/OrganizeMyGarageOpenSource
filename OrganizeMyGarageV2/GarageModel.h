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
	void UpdateFavorite(const std::string& name, bool isFavorite);
	bool IsFavorite(const std::string& name) const;
	void LoadFavorites();

	bool GetFavoritesEnabled() const;
	bool GetFavoritesShuffle() const;
	bool GetFavoritesNotify() const;

	bool SetFavoritesEnabled(bool enabled);
	bool SetFavoritesShuffle(bool shuffle);
	bool SetFavoritesNotify(bool notify);

	std::shared_ptr<GameWrapper> m_gw;
	std::vector<PresetData> presets;
	size_t equippedPresetIndex = 0;

private:
	void EquipNextFavoritePreset(const char* evName);
	void SaveFavorites() const;
	void EquipPreset(size_t presetIndex);
	void EquipPresetPrivate(const std::string& name) const;
	PresetData GetPresetData(const LoadoutSetWrapper& loadout) const;
	std::vector<OnlineProdData> GetItemData(const LoadoutWrapper& loadout) const;

	std::shared_ptr<InventoryModel> m_inv;
	std::shared_ptr<PersistentStorage> m_ps;
	std::shared_ptr<CVarManagerWrapper> m_cv;
	std::unordered_set<std::string> favorites;
	bool preventSettingNextFavoritePreset{ false };
};
