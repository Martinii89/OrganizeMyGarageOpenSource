#include "pch.h"
#include "GarageModel.h"

#include "HardcodedColors.h"
#include "InventoryModel.h"

namespace {
bool GetCvarBoolOrFalse(CVarManagerWrapper* cv, const char* cvarName) {
	if (!cv) 
		return false;
	auto cvarWrap = cv->getCvar(cvarName);
	if (cvarWrap)
		return cvarWrap.getBoolValue();
	return false;
}

bool SetCvarVariable(CVarManagerWrapper* cv, const char* cvarName, bool value) {
	if (!cv)
		return false;
	CVarWrapper cvarWrap = cv->getCvar(cvarName);
	if (cvarWrap) {
		cvarWrap.setValue(value);
		return true;
	}
	return false;
}
}

GarageModel::GarageModel(std::shared_ptr<GameWrapper> gw, std::shared_ptr<InventoryModel> inv, std::shared_ptr<PersistentStorage> ps, std::shared_ptr<CVarManagerWrapper> cv):
	m_gw(std::move(gw)), m_inv(std::move(inv)), m_ps(std::move(ps)), m_cv(cv)
{
	m_ps->RegisterPersistentCvar(kCvarCycleFavEnabled, "0", "Enable cycling through favorite presets", true, true, 0, true, 1, true);
	m_ps->RegisterPersistentCvar(kCvarCycleFavShuffle, "0", "Shuffle the favorite presets", true, true, 0, true, 1, true);
	m_ps->RegisterPersistentCvar(kCvarCycleFavList, "", "List of favorites", true, false, 0, false, 1, true).addOnValueChanged([this](...) {
		this->LoadFavorites();
	});

	LoadFavorites();

	for (const char* funName : { "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",/* "Function TAGame.GameEvent_Soccar_TA.Destroyed" */}) {
		m_gw->HookEvent(funName, std::bind(&GarageModel::EquipNextFavoritePreset, this, funName));
	}

	// Each kickoff enables changing preset
	m_gw->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound", [this](...) {
		preventSettingNextFavoritePreset = false;
	});
}

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
	if (itemData.slot == 0)
	{
		int otherTeamIndex = teamIndex == 0 ? 1 : 0;
		auto [blue, orange] = preset.GetLoadoutData();
		int otherBody;
		if (teamIndex == 0)
		{
			otherBody = orange.GetLoadout().Get(0);
		}
		else
		{
			otherBody = blue.GetLoadout().Get(0);
		}
		if (otherBody != itemData.prodId)
		{
			preset.EquipProduct(itemData.instanceId, itemData.slot, otherTeamIndex);
		}
	}
	if (equippedPresetIndex == presetIndex)
	{
		save.SetPreviewTeam(teamIndex);
	}
}

void GarageModel::UpdateFavorite(const std::string& name, bool isFavorite)
{
	if (isFavorite) {
		favorites.insert(name);
	}
	else {
		auto it = favorites.find(name);
		if (it != favorites.end()) 
			favorites.erase(it);
	}
	SaveFavorites();
}

bool GarageModel::IsFavorite(const std::string& name) const
{
	return favorites.find(name) != favorites.end();
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
		auto data = m_inv->GetProdData(item);
		DEBUGLOG("InstaceId: {}:{}. slot: {}", item.lower_bits, item.upper_bits, data.slot);
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

void GarageModel::LoadFavorites()
{
	auto favListCvar = _globalCvarManager->getCvar(kCvarCycleFavList);
	auto favStr = favListCvar ? favListCvar.getStringValue() : "";

	std::stringstream ss(favStr);
	std::string item;
	favorites.clear();
	while (std::getline(ss, item, kCvarCycleFavListDelimiter)) {
		favorites.insert(item);
	}
}

bool GarageModel::GetFavoritesEnabled() const
{
	return GetCvarBoolOrFalse(m_cv.get(), kCvarCycleFavEnabled);
}

bool GarageModel::GetFavoritesShuffle() const
{
	return GetCvarBoolOrFalse(m_cv.get(), kCvarCycleFavShuffle);
}

bool GarageModel::SetFavoritesEnabled(bool enabled)
{
	return SetCvarVariable(m_cv.get(), kCvarCycleFavEnabled, enabled);
}

bool GarageModel::SetFavoritesShuffle(bool shuffle)
{
	return SetCvarVariable(m_cv.get(), kCvarCycleFavShuffle, shuffle);
}

void GarageModel::SaveFavorites() const
{
	std::string serialized;
	for (const auto& fav : favorites) {
		if (!serialized.empty())
			serialized.push_back(kCvarCycleFavListDelimiter);
		serialized.append(fav);
	}
	auto favListCvar = _globalCvarManager->getCvar(kCvarCycleFavList);
	if (favListCvar) {
		favListCvar.setValue(serialized);
	}
	else 
		LOG("OMG: Unable to save favorites because CVAR is not registered");

	m_ps->WritePersistentStorage();
}

void GarageModel::EquipPreset(size_t presetIndex) {
	EquipPreset(presets[presetIndex].name);
	
	// safe to do because we are not changing presets.
	equippedPresetIndex = presetIndex;
}

void GarageModel::EquipNextFavoritePreset(const char* evName) {
	if (!GetCvarBoolOrFalse(m_cv.get(), kCvarCycleFavEnabled))
		return;

	// If multiple events trigger swapping presets, 
	// do not do it again unless a game/kickoff started.
	// Useful for hopping between trainings, and if both MatchEnded and Destroyed hooks
	// are enabled (currently Destroyed should not be enabled because of SDK bug).
	if (std::exchange(preventSettingNextFavoritePreset, true))
		return;

	DEBUGLOG_PERSIST("Equipping new preset, event name: {}", evName);

	std::vector<size_t> favIndices;
	favIndices.reserve(presets.size());
	for (size_t i = 0; i < presets.size(); ++i) {
		// i != equippedPresetIndex ensures we don't have same preset twice in a row
		if (i != equippedPresetIndex && favorites.find(presets[i].name) != favorites.end()) 
			favIndices.push_back(i);
	}

	if (favIndices.empty())
		return;

	size_t nextPresetIndex = 0;
	if (favIndices.size() == 1) {
		nextPresetIndex = favIndices.front();
	} else if (GetCvarBoolOrFalse(m_cv.get(), kCvarCycleFavShuffle)) {
		std::random_device rd;
		std::mt19937 mt(rd());
		std::uniform_int_distribution<> dist(0, favIndices.size() - 1); // range is inclusive
		nextPresetIndex = favIndices[dist(mt)];
	}
	else {
		auto currIdx = std::upper_bound(favIndices.cbegin(), favIndices.cend(), equippedPresetIndex);
		nextPresetIndex = (currIdx == favIndices.cend()) ? favIndices.front() : *currIdx;
	}

	DEBUGLOG_PERSIST("Equipping preset {}. Total favorites: {}. Previous preset: {}", nextPresetIndex, presets.size(), equippedPresetIndex);

	EquipPreset(nextPresetIndex);
}