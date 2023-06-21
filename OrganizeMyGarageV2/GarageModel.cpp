#include <unordered_set>
#include "pch.h"
#include "GarageModel.h"

#include "HardcodedColors.h"
#include "InventoryModel.h"


GarageModel::GarageModel(std::shared_ptr<GameWrapper> gw, std::shared_ptr<InventoryModel> im)
	: m_gw(std::move(gw)), m_im(std::move(im)) {}

std::vector<PresetData> GarageModel::GetCurrentPresets() const
{
	std::vector<PresetData> res;
	try
	{
		if (const auto presetWrapper = m_gw->GetUserLoadoutSave())
		{
			for (auto p : presetWrapper.GetPresets())
			{
				res.push_back(GetPresetData(p));
			}
		}
	}
	catch (const std::exception& e)
	{
		LOG("{}", e.what());
	}
	return res;
}

void GarageModel::SwapPreset(size_t a, size_t b) const
{
	if (a == b)
	{
		return;
	}
	auto max = presets.size();
	if (a >= max || b >= max)
	{
		return;
	}
	SwapPresetPrivate(presets[a].name, presets[b].name);
}

void GarageModel::MovePreset(size_t from, size_t to) const
{
	if (from == to)
	{
		return;
	}

	const int di = (from > to) ? -1 : 1;

	for (size_t i = from; i != to; i += di)
	{
		SwapPreset(i, i + di);
	}
}

LoadoutSetWrapper GarageModel::AddPreset() const
{
	try
	{
		auto presetWrapper = m_gw->GetUserLoadoutSave();
		auto newPreset = presetWrapper.AddPreset();
		return newPreset;
	}
	catch (const std::exception& e)
	{
		LOG("{}", e.what());
	}
	return {0};
}

void GarageModel::CopyPreset(const std::string& presetName) const
{
	try
	{
		auto presetWrapper = m_gw->GetUserLoadoutSave();
		auto presetA = presetWrapper.GetPreset(presetName);
		auto newPreset = presetWrapper.AddPreset();
		newPreset.Rename(presetName + "(1)");
		newPreset.CopyFrom(presetA);
		presetWrapper.EquipPreset(newPreset);
		int originalIndex = presetWrapper.GetIndex(presetA);
		int copyIndex = presetWrapper.GetIndex(newPreset);
		MovePreset(originalIndex + 1, copyIndex);
	}
	catch (const std::exception& e)
	{
		LOG("{}", e.what());
	}
}

void GarageModel::DeletePreset(const std::string& presetName) const
{
	try
	{
		auto presetWrapper = m_gw->GetUserLoadoutSave();
		auto preset = presetWrapper.GetPreset(presetName);
		presetWrapper.DeletePreset(preset);
	}
	catch (const std::exception& e)
	{
		LOG("{}", e.what());
	}
}

void GarageModel::RenamePreset(size_t presetIndex, const std::string& newName) const
{
	const auto save = m_gw->GetUserLoadoutSave();
	if (!save)
	{
		LOG("Error: Failed to rename preset with index {} to {} (loadout save null)", presetIndex, newName);
		return;
	}
	auto preset = save.GetPreset(static_cast<int>(presetIndex));
	if (!preset)
	{
		LOG("Error: Failed to rename preset with index {} to {} (preset null)", presetIndex, newName);
		return;
	}
	preset.Rename(newName);
}

void GarageModel::EquipItem(size_t presetIndex, const OnlineProdData& itemData, int teamIndex) const
{
	auto save = m_gw->GetUserLoadoutSave();
	auto preset = save.GetPreset(static_cast<int>(presetIndex));
	preset.EquipProduct(itemData.instanceId, itemData.slot, teamIndex);
	if (itemData.slot == static_cast<int>(ItemSlots::Body))
	{
		auto [blue, orange] = preset.GetLoadoutData();
		const int otherBody = (teamIndex ? blue : orange).GetLoadout().Get(0);
		if (otherBody != itemData.prodId)
		{
			const int otherTeamIndex = 1 - teamIndex;
			preset.EquipProduct(itemData.instanceId, itemData.slot, otherTeamIndex);
		}
	}
	if (equippedPresetIndex == presetIndex)
	{
		save.SetPreviewTeam(teamIndex);
	}
}

const std::vector<PresetData>& GarageModel::GetPresets() const
{
	return presets;
}

size_t GarageModel::GetCurrentPresetIndex() const {
	return equippedPresetIndex;
}

int GarageModel::GetEquippedIndex() const
{
	try
	{
		const auto presetWrapper = m_gw->GetUserLoadoutSave();
		if (!presetWrapper)
		{
			return -1;
		}
		const auto equippedPreset = presetWrapper.GetEquippedLoadout();
		if (!equippedPreset)
		{
			return -1;
		}
		auto presetsArray = presetWrapper.GetPresets();
		for (int i = 0; i < presetsArray.Count(); i++)
		{
			if (equippedPreset.memory_address == presetsArray.Get(i).memory_address)
			{
				return i;
			}
		}
	}
	catch (const std::exception& e)
	{
		LOG(e.what());
	}
	return -1;
}

void GarageModel::RefreshPresets()
{
	presets = GetCurrentPresets();
}

void GarageModel::RefreshEquippedIndex()
{
	equippedPresetIndex = GetEquippedIndex();
}

void GarageModel::EquipPreset(const std::string& name) const
{
	try
	{
		EquipPresetPrivate(name);
	}
	catch (const std::exception& e)
	{
		LOG("{}", e.what());
	}
	// If sitting in the garage menu you have to equip twice for some reason..
	if (const auto menuStack = m_gw->GetMenuStack())
	{
		if (menuStack.GetTopMenu() == "GarageMainMenuMovie")
		{
			m_gw->SetTimeout([this, name](...)
			{
				EquipPresetPrivate(name);
			}, 0.5);
		}
	}
}

void GarageModel::EquipPresetPrivate(const std::string& name) const
{
	const auto presetWrapper = m_gw->GetUserLoadoutSave();
	const auto preset = presetWrapper.GetPreset(name);
	presetWrapper.EquipPreset(preset);
}

PresetData GarageModel::GetPresetData(const LoadoutSetWrapper& loadout) const
{
	PresetData res;
	res.name = loadout.GetName();
	auto [blue, orange] = loadout.GetLoadoutData();

	res.color1.primaryId = blue.GetPrimaryPaintColorId();
	res.color1.accentId = blue.GetAccentPaintColorId();
	res.color1.primaryColor = HardcodedColors::blueCarColors[res.color1.primaryId];
	res.color1.accentColor = HardcodedColors::customCarColors[res.color1.accentId];

	res.color2.primaryId = orange.GetPrimaryPaintColorId();
	res.color2.accentId = orange.GetAccentPaintColorId();
	res.color2.primaryColor = HardcodedColors::orangeCarColors[res.color2.primaryId];
	res.color2.accentColor = HardcodedColors::customCarColors[res.color2.accentId];

	res.loadout = GetItemData(blue);
	res.loadout2 = GetItemData(orange);
	return res;
}

std::vector<OnlineProdData> GarageModel::GetItemData(const LoadoutWrapper& loadout) const
{
	std::vector<OnlineProdData> res;
	for (auto item : loadout.GetOnlineLoadoutV2())
	{
		auto data = m_im->GetProdData(item);
		DEBUGLOG("InstanceId: {}:{}. slot: {}", item.lower_bits, item.upper_bits, data.slot);
		res.push_back(data);
	}
	return res;
}

void GarageModel::SwapPresetPrivate(const std::string& a, const std::string& b) const
{
	try
	{
		auto presetWrapper = m_gw->GetUserLoadoutSave();
		auto presetA = presetWrapper.GetPreset(a);
		auto presetB = presetWrapper.GetPreset(b);
		presetWrapper.SwapPreset(presetA, presetB);
	}
	catch (const std::exception& e)
	{
		LOG("{}", e.what());
	}
}


void GarageModel::EquipPreset(size_t presetIndex) {
	EquipPreset(presets[presetIndex].name);
	
	// safe to do because we are not changing presets.
	equippedPresetIndex = presetIndex;
}


