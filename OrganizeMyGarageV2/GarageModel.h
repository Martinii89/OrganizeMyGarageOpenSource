#pragma once
#include "ItemStructs.h"
#include <vector>
#include <memory>

class InventoryModel;

class GarageModel
{
public:
	explicit GarageModel(std::shared_ptr<GameWrapper> gw, std::shared_ptr<InventoryModel> inventory);

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

	std::shared_ptr<GameWrapper> gw;
	std::vector<PresetData> presets;
	size_t equippedPresetIndex = 0;

private:
	void EquipPresetPrivate(const std::string& name) const;
	PresetData GetPresetData(const LoadoutSetWrapper& loadout) const;
	std::vector<OnlineProdData> GetItemData(const LoadoutWrapper& loadout) const;
	std::shared_ptr<InventoryModel> m_inventory;
};
