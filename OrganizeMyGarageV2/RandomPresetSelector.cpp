#include "pch.h"
#include "RandomPresetSelector.h"

#include <random>

#include "GarageModel.h"
#include "InventoryModel.h"
#include "PersistentStorage.h"

namespace {
bool GetCvarBoolOrFalse(CVarManagerWrapper* cv, const char* cvarName) {
  if (!cv) return false;
  auto cvarWrap = cv->getCvar(cvarName);
  if (cvarWrap) return cvarWrap.getBoolValue();
  return false;
}

bool SetCvarVariable(CVarManagerWrapper* cv, const char* cvarName, bool value) {
  if (!cv) return false;
  CVarWrapper cvarWrap = cv->getCvar(cvarName);
  if (cvarWrap) {
    cvarWrap.setValue(value);
    return true;
  }
  return false;
}

template <typename T>
T SelectNext(const std::vector<T>& items, size_t previous, bool shuffle) {
  if (items.size() == 1) {
    return items.front();
  }
   if (shuffle) {
     std::random_device rd;
     std::mt19937 mt(rd());
     std::uniform_int_distribution<> dist(
         0, int(items.size() - 1));  // range is inclusive
     return items[dist(mt)];
   }
  auto currIdx = std::upper_bound(items.cbegin(), items.cend(), previous);
  return (currIdx == items.cend()) ? items.front() : *currIdx;
}
}  // end of anonymous namespace

RandomPresetSelector::RandomPresetSelector(
    std::shared_ptr<GameWrapper> gw, std::shared_ptr<PersistentStorage> ps,
    std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GarageModel> gm,
    std::shared_ptr<InventoryModel> im)
    : m_gw(std::move(gw)),
      m_ps(std::move(ps)),
      m_cv(std::move(cv)),
      m_gm(std::move(gm)),
      m_im(std::move(im)) {
  m_ps->RegisterPersistentCvar(kCvarCycleFavEnabled, "0",
                               "Enable cycling through favorite presets", true,
                               true, 0, true, 1, true);
  m_ps->RegisterPersistentCvar(kCvarCycleFavShuffle, "0",
                               "Shuffle the favorite presets", true, true, 0,
                               true, 1, true);
  m_ps->RegisterPersistentCvar(kCvarCycleFavNotify, "0",
                               "Notify on automatic preset change", true, true,
                               0, true, 1, true);
  m_ps->RegisterPersistentCvar(kCvarRandomGoalExplosion, "0",
                               "Random Goal Explosion", true, true, 0, true, 1,
                               true);
  m_ps->RegisterPersistentCvar(kCvarCycleFavList, "", "List of favorites", true,
                               false, 0, false, 1, true)
      .addOnValueChanged([this](...) { this->LoadFavorites(); });

  LoadFavorites();

  for (const char* funName :
       {"Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
        "Function TAGame.GameEvent_Soccar_TA.Destroyed"}) {
    m_gw->HookEvent(funName, [this](auto funName) {
      EquipNextFavoritePreset(funName.c_str());
      RandomGoalExplosion(funName.c_str());
    });
  }

  // Each kickoff enables changing preset
  m_gw->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound",
                  [this](...) { preventSettingNextFavoritePreset = false; });
}

void RandomPresetSelector::UpdateFavorite(const std::string& name,
                                          bool isFavorite) {
  if (isFavorite) {
    favorites.insert(name);
  } else {
    auto it = favorites.find(name);
    if (it != favorites.end()) favorites.erase(it);
  }
  SaveFavorites();
}

void RandomPresetSelector::LoadFavorites() {
  auto favListCvar = m_cv->getCvar(kCvarCycleFavList);
  auto favStr = favListCvar ? favListCvar.getStringValue() : "";

  std::stringstream ss(favStr);
  std::string item;
  favorites.clear();
  while (std::getline(ss, item, kCvarCycleFavListDelimiter)) {
    favorites.insert(item);
  }
}

bool RandomPresetSelector::IsFavorite(const std::string& name) const {
  return favorites.find(name) != favorites.end();
}

bool RandomPresetSelector::GetFavoritesEnabled() const {
  return GetCvarBoolOrFalse(m_cv.get(), kCvarCycleFavEnabled);
}

bool RandomPresetSelector::SetFavoritesEnabled(bool enabled) {
  return SetCvarVariable(m_cv.get(), kCvarCycleFavEnabled, enabled);
}

bool RandomPresetSelector::GetFavoritesShuffle() const {
  return GetCvarBoolOrFalse(m_cv.get(), kCvarCycleFavShuffle);
}

bool RandomPresetSelector::SetFavoritesShuffle(bool shuffle) {
  return SetCvarVariable(m_cv.get(), kCvarCycleFavShuffle, shuffle);
}

bool RandomPresetSelector::GetFavoritesNotify() const {
  return GetCvarBoolOrFalse(m_cv.get(), kCvarCycleFavNotify);
}

bool RandomPresetSelector::SetFavoritesNotify(bool notify) {
  return SetCvarVariable(m_cv.get(), kCvarCycleFavNotify, notify);
}

bool RandomPresetSelector::GetRandomGoalExplosionEnabled() const {
  return GetCvarBoolOrFalse(m_cv.get(), kCvarRandomGoalExplosion);
}

bool RandomPresetSelector::SetRandomGoalExplosionEnabled(bool enabled) {
  return SetCvarVariable(m_cv.get(), kCvarRandomGoalExplosion, enabled);
}

void RandomPresetSelector::SaveFavorites() const {
  std::string serialized;
  for (const auto& fav : favorites) {
    if (!serialized.empty()) serialized.push_back(kCvarCycleFavListDelimiter);
    serialized.append(fav);
  }
  auto favListCvar = m_cv->getCvar(kCvarCycleFavList);
  if (favListCvar) {
    favListCvar.setValue(serialized);
  } else
    LOG("OMG: Unable to save favorites because CVAR is not registered");

  m_ps->WritePersistentStorage();
}

void RandomPresetSelector::EquipNextFavoritePreset(const char* evName) {
  DEBUGLOG("EquipNextFavoritePreset {}", evName);
  if (!GetCvarBoolOrFalse(m_cv.get(), kCvarCycleFavEnabled)) return;

  // If multiple events trigger swapping presets,
  // do not do it again unless a game/kickoff started.
  // Useful for hopping between trainings, and if both MatchEnded and Destroyed
  // hooks are enabled (currently Destroyed should not be enabled because of SDK
  // bug).
  if (std::exchange(preventSettingNextFavoritePreset, true)) return;

  DEBUGLOG("Equipping new preset, event name: {}", evName);

  std::vector<size_t> favIndices;
  const auto& presets = m_gm->GetPresets();
  auto equippedPresetIndex = m_gm->GetCurrentPresetIndex();
  favIndices.reserve(presets.size());
  for (size_t i = 0; i < presets.size(); ++i) {
    // i != equippedPresetIndex ensures we don't have same preset twice in a row
    if (i != equippedPresetIndex &&
        favorites.find(presets[i].name) != favorites.end())
      favIndices.push_back(i);
  }

  if (favIndices.empty()) return;

  size_t nextPresetIndex =
      SelectNext(favIndices, equippedPresetIndex,
                 GetCvarBoolOrFalse(m_cv.get(), kCvarCycleFavShuffle));

  if (GetFavoritesNotify()) {
    // Nasty hack because the notification settings are global: enabling them
    // just for sending this notification
    const auto oldNotifications =
        GetCvarBoolOrFalse(m_cv.get(), kCvarGlobalNotificationsEnabled);
    if (SetCvarVariable(m_cv.get(), kCvarGlobalNotificationsEnabled, true)) {
      m_gw->Toast("OMG: Next Preset", presets[nextPresetIndex].name, "default",
                  10.0, ToastType_Info);
    }
    if (!oldNotifications)
      SetCvarVariable(m_cv.get(), kCvarGlobalNotificationsEnabled, false);
  }

  DEBUGLOG("Equipping preset {}. Total favorites: {}. Previous preset: {}",
           nextPresetIndex, presets.size(), equippedPresetIndex);

  m_gm->EquipPreset(nextPresetIndex);
}

void RandomPresetSelector::RandomGoalExplosion(const char* evName) {
  DEBUGLOG("RandomGoalExplosion {}", evName);
  if (!GetCvarBoolOrFalse(m_cv.get(), kCvarRandomGoalExplosion)) return;
  const int slotId = static_cast<int>(ItemSlots::GoalExplosion);

  ProductInstanceID currentGoalExplosionInstanceId{0, 0};
  const auto& presets = m_gm->GetPresets();
  auto equippedPresetIndex = m_gm->GetCurrentPresetIndex();
  for (const auto& item : presets[equippedPresetIndex].loadout) {
    if (item.slot == slotId) {
      currentGoalExplosionInstanceId = item.instanceId;
      break;
    }
  }

  const auto& goalExplosions = m_im->GetSlotProducts(slotId);
  /*
   * TODO: Favorited could possibly be calculated only once
   * because it seems OMG does not refresh inventory data during lifetime.
   * Leaving it as is in hope that changes (or I do it eventually in another
   * diff).
   */
  std::vector<size_t> favorited;
  size_t currentGoalExplosionFavIdx =
      0;  // useful if we'll want to cycle through them
  for (size_t i = 0; i < goalExplosions.size(); ++i) {
    const auto& goalExplosion = goalExplosions[i];
    if (!goalExplosion.favorite) continue;
    if (goalExplosion.instanceId == currentGoalExplosionInstanceId)
      currentGoalExplosionFavIdx = i;
    else
      favorited.push_back(i);
  }

  const auto& nextGoalExplosion =
      goalExplosions[SelectNext(favorited, currentGoalExplosionFavIdx, true)];

  LOG("Equipping goal explosion {}!", nextGoalExplosion.name);

  for (int itemIndex : {0, 1})
    m_gm->EquipItem(equippedPresetIndex, nextGoalExplosion, itemIndex);
}
