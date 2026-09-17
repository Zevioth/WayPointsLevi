#pragma once

#include "mod/Config.h"

#include <pl/Config.hpp>
#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>
#include <pl/ModMenuConfig.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace waypointmanager {

class WaypointManagerMod {
public:
  static WaypointManagerMod &instance();

  WaypointManagerMod(const WaypointManagerMod &) = delete;
  WaypointManagerMod &operator=(const WaypointManagerMod &) = delete;

  bool load();
  bool enable();
  bool disable();
  bool unload();

  [[nodiscard]] ll::mod::NativeMod &getSelf() const { return mSelf; }

private:
  WaypointManagerMod();

  bool registerWaypointButton();
  bool registerMainModule();

  void onWaypointButtonEvent(std::string_view buttonId, pl::modmenu::ButtonEvent event, float value);
  void onMainModuleToggle(std::string_view moduleId, bool enabled);
  void onMainModuleConfigChanged(std::string_view moduleId, std::string_view key,
                                 std::string_view value);

  void rebuildSchema();
  void resetEditorForNewWaypoint();
  void loadWaypointIntoEditor(int id);
  void saveEditedWaypoint();
  void deleteEditedWaypoint();
  void persistAndRefresh();

  Waypoint *findWaypointById(int id);
  [[nodiscard]] std::string buildWaypointSummary() const;
  [[nodiscard]] std::string buildSchemaJson() const;

  ll::mod::NativeMod &mSelf;
  std::optional<pl::config::ConfigFile<ModConfig>> mConfigFile;
  ModConfig mConfig;

  bool mButtonRegistered = false;
  bool mMainModuleRegistered = false;
};

} // namespace waypointmanager
