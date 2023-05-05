#include <unordered_set>
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

template<typename T>
T SelectNext(const std::vector<T> items, size_t previous, bool shuffle) {
	if (items.size() == 1) {
		return items.front();
	}
	if (shuffle) {
		std::random_device rd;
		std::mt19937 mt(rd());
		std::uniform_int_distribution<> dist(0, items.size() - 1); // range is inclusive
		return items[dist(mt)];
	}
	auto currIdx = std::upper_bound(items.cbegin(), items.cend(), previous);
	return (currIdx == items.cend()) ? items.front() : *currIdx;
}

}

GarageModel::GarageModel(std::shared_ptr<GameWrapper> gw, std::shared_ptr<InventoryModel> inv, std::shared_ptr<PersistentStorage> ps, std::shared_ptr<CVarManagerWrapper> cv):
	m_gw(std::move(gw)), m_inv(std::move(inv)), m_ps(std::move(ps)), m_cv(cv)
{
	m_ps->RegisterPersistentCvar(kCvarCycleFavEnabled, "0", "Enable cycling through favorite presets", true, true, 0, true, 1, true);
	m_ps->RegisterPersistentCvar(kCvarCycleFavShuffle, "0", "Shuffle the favorite presets", true, true, 0, true, 1, true);
	m_ps->RegisterPersistentCvar(kCvarCycleFavNotify, "0", "Notify on automatic preset change", true, true, 0, true, 1, true);
	m_ps->RegisterPersistentCvar(kCvarRandomGoalExplosion, "0", "Random Goal Explosion", true, true, 0, true, 1, true);
	m_ps->RegisterPersistentCvar(kCvarCycleFavList, "", "List of favorites", true, false, 0, false, 1, true).addOnValueChanged([this](...) {
		this->LoadFavorites();
	});

	LoadFavorites();

	for (const char* funName : { "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",/* "Function TAGame.GameEvent_Soccar_TA.Destroyed" */}) {
		m_gw->HookEvent(funName, [this](auto funName) {
			EquipNextFavoritePreset(funName.c_str());
			RandomGoalExplosion(funName.c_str());
		});
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

bool GarageModel::SetFavoritesEnabled(bool enabled)
{
	return SetCvarVariable(m_cv.get(), kCvarCycleFavEnabled, enabled);
}

bool GarageModel::GetFavoritesShuffle() const
{
	return GetCvarBoolOrFalse(m_cv.get(), kCvarCycleFavShuffle);
}

bool GarageModel::SetFavoritesShuffle(bool shuffle)
{
	return SetCvarVariable(m_cv.get(), kCvarCycleFavShuffle, shuffle);
}

bool GarageModel::GetFavoritesNotify() const
{
	return GetCvarBoolOrFalse(m_cv.get(), kCvarCycleFavNotify);
}

bool GarageModel::SetFavoritesNotify(bool notify)
{
	return SetCvarVariable(m_cv.get(), kCvarCycleFavNotify, notify);
}

bool GarageModel::GetRandomGoalExplosionEnabled() const
{
	return GetCvarBoolOrFalse(m_cv.get(), kCvarRandomGoalExplosion);
}

bool GarageModel::SetRandomGoalExplosionEnabled(bool enabled)
{
	return SetCvarVariable(m_cv.get(), kCvarRandomGoalExplosion, enabled);
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
	DEBUGLOG("EquipNextFavoritePreset {}", evName);
	if (!GetCvarBoolOrFalse(m_cv.get(), kCvarCycleFavEnabled))
		return;

	// If multiple events trigger swapping presets, 
	// do not do it again unless a game/kickoff started.
	// Useful for hopping between trainings, and if both MatchEnded and Destroyed hooks
	// are enabled (currently Destroyed should not be enabled because of SDK bug).
	if (std::exchange(preventSettingNextFavoritePreset, true))
		return;

	DEBUGLOG("Equipping new preset, event name: {}", evName);

	std::vector<size_t> favIndices;
	favIndices.reserve(presets.size());
	for (size_t i = 0; i < presets.size(); ++i) {
		// i != equippedPresetIndex ensures we don't have same preset twice in a row
		if (i != equippedPresetIndex && favorites.find(presets[i].name) != favorites.end()) 
			favIndices.push_back(i);
	}

	if (favIndices.empty())
		return;

	size_t nextPresetIndex = SelectNext(favIndices, equippedPresetIndex, GetCvarBoolOrFalse(m_cv.get(), kCvarCycleFavShuffle));

	if (GetFavoritesNotify()) {
		// Nasty hack because the notification settings are global: enabling them just for sending this notification
		const auto oldNotifications = GetCvarBoolOrFalse(m_cv.get(), kCvarGlobalNotificationsEnabled);
		if (SetCvarVariable(m_cv.get(), kCvarGlobalNotificationsEnabled, true)) {
			m_gw->Toast("OMG: Next Preset", presets[nextPresetIndex].name, "default", 10.0, ToastType_Info);
		}
		if (!oldNotifications)
			SetCvarVariable(m_cv.get(), kCvarGlobalNotificationsEnabled, false);
	}

	DEBUGLOG("Equipping preset {}. Total favorites: {}. Previous preset: {}", nextPresetIndex, presets.size(), equippedPresetIndex);

	EquipPreset(nextPresetIndex);
}

void GarageModel::RandomGoalExplosion(const char* evName)
{
	DEBUGLOG("RandomGoalExplosion {}", evName);
	if (!GetCvarBoolOrFalse(m_cv.get(), kCvarRandomGoalExplosion))
		return;
	const int slotId = static_cast<int>(ItemSlots::GoalExplosion);

	ProductInstanceID currentGoalExplosionInstanceId{ 0, 0 };
	for (const auto& item : presets[equippedPresetIndex].loadout) {
		if (item.slot == slotId) {
			currentGoalExplosionInstanceId = item.instanceId;
			break;
		}
	}

	const auto& goalExplosions = m_inv->GetSlotProducts(slotId);
	/* TODO: Favorited could possibly be calculated only once
	 * because it seems OMG does not refresh inventory data during lifetime.
	 * Leaving it as is in hope that changes (or I do it eventually in another diff).
	 */
	std::vector<size_t> favorited;
	size_t currentGoalExplosionFavIdx = 0; // useful if we'll want to cycle through them
	for (size_t i = 0; i < goalExplosions.size(); ++i) {
		const auto& goalExplosion = goalExplosions[i];
		if (!goalExplosion.favorite)
			continue;
		if (goalExplosion.instanceId == currentGoalExplosionInstanceId)
			currentGoalExplosionFavIdx = i;
		else 
			favorited.push_back(i);
	}

	const auto& nextGoalExplosion = goalExplosions[SelectNext(favorited, currentGoalExplosionFavIdx, true)];

	LOG("Equipping goal explosion {}!", nextGoalExplosion.name);

	for (int itemIndex : {0, 1})
		EquipItem(equippedPresetIndex, nextGoalExplosion, itemIndex);

}
