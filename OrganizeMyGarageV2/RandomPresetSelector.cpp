#include "pch.h"
#include "RandomPresetSelector.h"

#include <algorithm>
#include <random>
#include <unordered_map>

#include "CVarManagerSingleton.h"
#include "GarageModel.h"
#include "InventoryModel.h"
#include "PersistentStorage.h"

namespace {
bool GetCvarBoolOrFalse(const char* cvarName) {
  auto cv = CVarManagerSingleton::getInstance().getCvar();
  if (!cv) return false;
  auto cvarWrap = cv->getCvar(cvarName);
  if (cvarWrap) return cvarWrap.getBoolValue();
  return false;
}

int GetCvarBoolOrZero(const char* cvarName) {
  auto cv = CVarManagerSingleton::getInstance().getCvar();
  if (!cv) return false;
  auto cvarWrap = cv->getCvar(cvarName);
  if (cvarWrap) return cvarWrap.getIntValue();
  return 0;
}

template<typename T>
bool SetCvarVariable(const char* cvarName, T value) {
  auto cv = CVarManagerSingleton::getInstance().getCvar();
  if (!cv) return false;
  CVarWrapper cvarWrap = cv->getCvar(cvarName);
  if (cvarWrap) {
    cvarWrap.setValue(value);
    return true;
  }
  return false;
}

/// <summary>
/// Class that handles shuffling array of indices. 
/// We keep the shuffle order consistent thorugh the runtime.
/// </summary>
class Shuffler {
 public:
  static Shuffler& getInstance() {
    static Shuffler instance;
    return instance;
  }
  size_t getNext(size_t idx, size_t n) {
    auto it = precomputedShuffles.find(n);
    if (it == precomputedShuffles.end())
      it = precomputedShuffles.insert({n, create(n)}).first;
    return it->second[idx];
  }

 private:
  std::vector<size_t> create(size_t size) {
    // just shuffling a vector could create a cycle shorter than size
    std::vector<size_t> shuffled(size), ret(size);
    for (size_t i = 0; i < size; ++i) shuffled[i] = i;
    std::random_device rd;
    std::mt19937 mt(rd());
    std::shuffle(shuffled.begin(), shuffled.end(), mt);
    for (size_t i = 0; i < size; ++i) {
      ret[shuffled[i]] = shuffled[(i + 1) % size];
    }
    return ret;
  }

  std::unordered_map<size_t, std::vector<size_t>> precomputedShuffles;
};

template <typename T>
T SelectNext(const std::vector<T>& items, size_t previous, SelectMethod method) {
  if (items.empty()) {
    DEBUGLOG("PANIC! Nothing to select!");
    return {};
  }

  if (items.size() == 1) {
    return items.front();
  }

  if (method == SelectMethod::Randomize) {
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<> dist(
        0, int(items.size() - 1));  // range is inclusive
    return items[dist(mt)];
  }

  if (method == SelectMethod::Shuffle) {
    auto currIdx = std::lower_bound(items.cbegin(), items.cend(), previous);
    const auto nextIdx = Shuffler::getInstance().getNext(currIdx - items.cbegin(), items.size());
    return items[nextIdx];
  } else {
    auto nextIdx = std::upper_bound(items.cbegin(), items.cend(), previous);
    return (nextIdx == items.cend()) ? items.front() : *nextIdx;
  }
}
}  // end of anonymous namespace

RandomPresetSelector::RandomPresetSelector(
    std::shared_ptr<GameWrapper> gw, std::shared_ptr<PersistentStorage> ps,
    std::shared_ptr<GarageModel> gm, std::shared_ptr<InventoryModel> im)
    : gw_(std::move(gw)),
      ps_(std::move(ps)),
      gm_(std::move(gm)),
      im_(std::move(im)) {
  ps_->RegisterPersistentCvar(kCvarCycleFavMethod, "0",
                              "Method of cycling through favorite presets", true,
                              true, 0, true, 3, true);
  ps_->RegisterPersistentCvar(kCvarCycleFavNotify, "0",
                              "Notify on automatic preset change", true, true,
                              0, true, 1, true);
  ps_->RegisterPersistentCvar(kCvarRandomGoalExplosion, "0",
                              "Random Goal Explosion", true, true, 0, true, 1,
                              true);
  ps_->RegisterPersistentCvar(kCvarCycleFavList, "", "List of favorites", true,
                              false, 0, false, 1, true)
      .addOnValueChanged([this](...) { this->LoadFavorites(); });

  LoadFavorites();

  for (const char* funName :
       {"Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
        "Function TAGame.GameEvent_Soccar_TA.Destroyed"}) {
    gw_->HookEvent(funName, [this](auto funName) {
      EquipNextFavoritePreset(funName.c_str());
      RandomGoalExplosion(funName.c_str());
    });
  }

  // Each kickoff enables changing preset
  gw_->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound",
                 [this](...) { preventSettingNextFavoritePreset = false; });
}

void RandomPresetSelector::UpdateFavorite(const std::string& name,
                                          bool isFavorite) {
  if (isFavorite) {
    favorites_.insert(name);
  } else {
    auto it = favorites_.find(name);
    if (it != favorites_.end()) favorites_.erase(it);
  }
  SaveFavorites();
}

void RandomPresetSelector::LoadFavorites() {
  auto cv = CVarManagerSingleton::getInstance().getCvar();
  if (!cv) return;
  auto favListCvar = cv->getCvar(kCvarCycleFavList);
  auto favStr = favListCvar ? favListCvar.getStringValue() : "";

  std::stringstream ss(favStr);
  std::string item;
  favorites_.clear();
  while (std::getline(ss, item, kCvarCycleFavListDelimiter)) {
    favorites_.insert(item);
  }
}

bool RandomPresetSelector::IsFavorite(const std::string& name) const {
  return favorites_.find(name) != favorites_.end();
}

SelectMethod RandomPresetSelector::GetFavoritesSelectionMethod() const {
  return static_cast<SelectMethod>(GetCvarBoolOrZero(kCvarCycleFavMethod));
}

bool RandomPresetSelector::SetFavoritesSelectionMethod(SelectMethod value) {
  return SetCvarVariable(kCvarCycleFavMethod, value);
}

bool RandomPresetSelector::GetFavoritesNotify() const {
  return GetCvarBoolOrFalse(kCvarCycleFavNotify);
}

bool RandomPresetSelector::SetFavoritesNotify(bool notify) {
  return SetCvarVariable(kCvarCycleFavNotify, notify);
}

bool RandomPresetSelector::GetRandomGoalExplosionEnabled() const {
  return GetCvarBoolOrFalse(kCvarRandomGoalExplosion);
}

bool RandomPresetSelector::SetRandomGoalExplosionEnabled(bool enabled) {
  return SetCvarVariable(kCvarRandomGoalExplosion, enabled);
}

void RandomPresetSelector::SaveFavorites() const {
  auto cv = CVarManagerSingleton::getInstance().getCvar();
  if (!cv) return;
  std::string serialized;
  for (const auto& fav : favorites_) {
    if (!serialized.empty()) serialized.push_back(kCvarCycleFavListDelimiter);
    serialized.append(fav);
  }
  auto favListCvar = cv->getCvar(kCvarCycleFavList);
  if (favListCvar) {
    favListCvar.setValue(serialized);
  } else
    LOG("OMG: Unable to save favorites because CVAR is not registered");

  ps_->WritePersistentStorage();
}

void RandomPresetSelector::EquipNextFavoritePreset(const char* evName) {
  DEBUGLOG("EquipNextFavoritePreset {}", evName);
  SelectMethod method = GetFavoritesSelectionMethod();
  if (method == SelectMethod::Disabled) return;

  // If multiple events trigger swapping presets,
  // do not do it again unless a game/kickoff started.
  // Useful for hopping between trainings.
  if (std::exchange(preventSettingNextFavoritePreset, true)) return;

  DEBUGLOG("Equipping new preset, event name: {}", evName);

  std::vector<size_t> favIndices;
  const auto& presets = gm_->GetPresets();
  auto equippedPresetIndex = gm_->GetCurrentPresetIndex();
  favIndices.reserve(presets.size());
  for (size_t i = 0; i < presets.size(); ++i) {
    // i != equippedPresetIndex ensures we don't have same preset twice in a row
    const bool skip = method == SelectMethod::Randomize && i == equippedPresetIndex;
    if (!skip && favorites_.find(presets[i].name) != favorites_.end())
      favIndices.push_back(i);
  }

  if (favIndices.empty()) return;

  size_t nextPresetIndex =
      SelectNext(favIndices, equippedPresetIndex, method);

  if (GetFavoritesNotify()) {
    // Nasty hack because the notification settings are global: enabling them
    // just for sending this notification
    const auto oldNotifications =
        GetCvarBoolOrFalse(kCvarGlobalNotificationsEnabled);
    if (SetCvarVariable(kCvarGlobalNotificationsEnabled, true)) {
      gw_->Toast("OMG: Next Preset", presets[nextPresetIndex].name, "default",
                 10.0, ToastType_Info);
    }
    if (!oldNotifications)
      SetCvarVariable(kCvarGlobalNotificationsEnabled, false);
  }

  DEBUGLOG("Equipping preset {}. Total favorites: {}. Previous preset: {}",
           nextPresetIndex, presets.size(), equippedPresetIndex);

  gm_->EquipPreset(nextPresetIndex);
}

void RandomPresetSelector::RandomGoalExplosion(const char* evName) {
  DEBUGLOG("RandomGoalExplosion {}", evName);
  if (!GetCvarBoolOrFalse(kCvarRandomGoalExplosion)) return;
  const int slotId = static_cast<int>(ItemSlots::GoalExplosion);

  ProductInstanceID currentGoalExplosionInstanceId{0, 0};
  const auto& presets = gm_->GetPresets();
  auto equippedPresetIndex = gm_->GetCurrentPresetIndex();
  for (const auto& item : presets[equippedPresetIndex].loadout) {
    if (item.slot == slotId) {
      currentGoalExplosionInstanceId = item.instanceId;
      break;
    }
  }

  const auto& goalExplosions = im_->GetSlotProducts(slotId);
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
    DEBUGLOG("Found favorite goal explosion {}", goalExplosion.name);
    if (goalExplosion.instanceId == currentGoalExplosionInstanceId)
      currentGoalExplosionFavIdx = i;
    else
      favorited.push_back(i);
  }

  if (favorited.empty()) {
    DEBUGLOG("No favorite goal explosions - nothing to select from.");
    return;
  }

  const auto& nextGoalExplosion = goalExplosions[SelectNext(
      favorited, currentGoalExplosionFavIdx, SelectMethod::Randomize)];

  LOG("Equipping goal explosion {}!", nextGoalExplosion.name);

  for (int itemIndex : {0, 1})
    gm_->EquipItem(equippedPresetIndex, nextGoalExplosion, itemIndex);
}
