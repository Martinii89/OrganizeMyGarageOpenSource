#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
constexpr char kCvarGlobalNotificationsEnabled[] =
    "cl_notifications_enabled_beta";  // at least it's out of alpha phase
constexpr char kCvarCycleFavEnabled[] = "omg_cycle_favorite_presets_enabled";
constexpr char kCvarCycleFavShuffle[] = "omg_cycle_favorite_presets_shuffle";
constexpr char kCvarCycleFavNotify[] = "omg_cycle_favorite_presets_notify";
constexpr char kCvarCycleFavList[] = "omg_cycle_favorites";
constexpr char kCvarRandomGoalExplosion[] = "omg_random_goal_explosion";
constexpr char kCvarCycleFavListDelimiter = '\f';  // hope noone uses this
}  // namespace

class GameWrapper;
class PersistentStorage;
class CVarManagerWrapper;
class GarageModel;
class InventoryModel;

class RandomPresetSelector {
  void EquipNextFavoritePreset(const char* evName);
  void RandomGoalExplosion(const char* evName);
  void SaveFavorites() const;

 public:
  RandomPresetSelector(std::shared_ptr<GameWrapper> gw,
                       std::shared_ptr<PersistentStorage> ps,
                       std::shared_ptr<CVarManagerWrapper> cv,
                       std::shared_ptr<GarageModel> gm,
                       std::shared_ptr<InventoryModel> im);

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
  std::shared_ptr<GameWrapper> m_gw;
  std::shared_ptr<PersistentStorage> m_ps;
  std::shared_ptr<CVarManagerWrapper> m_cv;
  std::shared_ptr<GarageModel> m_gm;
  std::shared_ptr<InventoryModel> m_im;
  std::unordered_set<std::string> favorites;
  bool preventSettingNextFavoritePreset{false};
};