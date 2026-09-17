#include "mod/MyMod.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace waypointmanager {

namespace {

constexpr std::array<const char *, 10> kPresetNames = {
    "Home", "Base", "Mine", "Farm", "Shop",
    "Portal", "NetherBase", "EndPortal", "WaypointA", "WaypointB"};

constexpr std::array<const char *, 3> kDimensionNames = {"Overworld", "Nether", "End"};

constexpr const char *ButtonId = "waypointmanager.wp_button";
constexpr const char *MainModuleId = "waypointmanager.main";

int clampInt(int value, int minValue, int maxValue) {
  return std::clamp(value, minValue, maxValue);
}

int parseInt(std::string_view value, int fallback) {
  try {
    std::string copy(value);
    std::size_t consumed = 0;
    int parsed = std::stoi(copy, &consumed);
    if (consumed != copy.size()) return fallback;
    return parsed;
  } catch (...) {
    return fallback;
  }
}

const char *presetName(int index) {
  if (index < 0) index = 0;
  return kPresetNames[static_cast<std::size_t>(index) % kPresetNames.size()];
}

const char *dimensionName(int index) {
  if (index < 0) index = 0;
  return kDimensionNames[static_cast<std::size_t>(index) % kDimensionNames.size()];
}

std::string displayName(const Waypoint &waypoint) {
  if (!waypoint.customName.empty()) return waypoint.customName;
  return presetName(waypoint.nameIndex);
}

pl::modmenu::ConfigNodeV2 makeNode(const char *key, const char *title,
                                   const char *category,
                                   pl::modmenu::ConfigControlTypeV2 type) {
  pl::modmenu::ConfigNodeV2 node;
  node.id = key;
  node.key = key;
  node.title = title;
  node.category = category;
  node.type = type;
  return node;
}

pl::modmenu::ConfigNodeV2 makeInfo(const char *id, const char *title,
                                   const std::string &description,
                                   const char *category) {
  auto node = makeNode(id, title, category, pl::modmenu::ConfigControlTypeV2::Info);
  node.description = description;
  return node;
}

pl::modmenu::ConfigNodeV2 makeSection(const char *id, const char *title,
                                      const char *category) {
  return makeNode(id, title, category, pl::modmenu::ConfigControlTypeV2::Section);
}

} // namespace

WaypointManagerMod &WaypointManagerMod::instance() {
  static WaypointManagerMod instance;
  return instance;
}

WaypointManagerMod::WaypointManagerMod() : mSelf(*ll::mod::NativeMod::current()) {}

bool WaypointManagerMod::load() {
  auto &self = getSelf();
  self.getLogger().debug("Loading...");

  std::error_code ec;
  std::filesystem::create_directories(self.getDataDir(), ec);
  if (ec) {
    self.getLogger().error("Failed to create data directory {}: {}", self.getDataDir().string(),
                           ec.message());
    return false;
  }
  std::filesystem::create_directories(self.getConfigDir(), ec);
  if (ec) {
    self.getLogger().error("Failed to create config directory {}: {}",
                           self.getConfigDir().string(), ec.message());
    return false;
  }

  mConfigFile.emplace();
  if (!mConfigFile->load()) {
    self.getLogger().warn("Failed to load typed config");
    return false;
  }

  mConfig = mConfigFile->value();

  // Migrate/repair old configs without deleting saved waypoints.
  mConfig.version = 2;
  int highestId = 0;
  for (const auto &waypoint : mConfig.waypoints) highestId = std::max(highestId, waypoint.id);
  if (mConfig.nextWaypointId <= highestId) mConfig.nextWaypointId = highestId + 1;
  mConfig.maxVisible = clampInt(mConfig.maxVisible, 1, 64);

  if (mConfig.activeWaypointId != 0 && !findWaypointById(mConfig.activeWaypointId)) {
    mConfig.activeWaypointId = 0;
  }

  mConfigFile->value() = mConfig;
  mConfigFile->save();

  self.getLogger().info("Loaded {} from {}", self.getName(), self.getModDir().string());
  return true;
}

bool WaypointManagerMod::enable() {
  auto &self = getSelf();
  self.getLogger().debug("Enabling...");

  if (!mConfig.enabled) {
    self.getLogger().info("Waypoint Manager is disabled by config");
    return true;
  }

  bool ok = registerMainModule();
  ok = registerWaypointButton() && ok;
  return ok;
}

bool WaypointManagerMod::disable() {
  auto &self = getSelf();
  self.getLogger().debug("Disabling...");

  if (mButtonRegistered) {
    pl::modmenu::unregisterButton(ButtonId);
    mButtonRegistered = false;
  }
  if (mMainModuleRegistered) {
    pl::modmenu::unregisterModule(MainModuleId);
    mMainModuleRegistered = false;
  }
  return true;
}

bool WaypointManagerMod::unload() {
  getSelf().getLogger().debug("Unloading...");
  mConfigFile.reset();
  return true;
}

bool WaypointManagerMod::registerWaypointButton() {
  bool ok = pl::modmenu::ButtonBuilder(ButtonId, "Waypoints")
                .modId(getSelf().getId())
                .moduleId(MainModuleId)
                .label("WP")
                .androidKeyCode(0)
                .behavior(pl::modmenu::ButtonBehavior::Click)
                .stylePreset(pl::modmenu::ButtonStylePreset::Accent)
                .sizeScale(1.10f, 1.0f)
                .onEvent([this](std::string_view buttonId, pl::modmenu::ButtonEvent event,
                                float value) { onWaypointButtonEvent(buttonId, event, value); })
                .registerButton();

  if (ok) {
    getSelf().getLogger().info("Registered WAYPOINTS HUD button ({})", ButtonId);
  } else {
    getSelf().getLogger().error("Failed to register WAYPOINTS HUD button ({})", ButtonId);
  }
  mButtonRegistered = ok;
  return ok;
}

void WaypointManagerMod::onWaypointButtonEvent(std::string_view buttonId,
                                                pl::modmenu::ButtonEvent event, float value) {
  (void)buttonId;
  (void)value;
  if (event == pl::modmenu::ButtonEvent::Click) {
    pl::modmenu::requestOpenMenu();
  }
}

bool WaypointManagerMod::registerMainModule() {
  const std::string description =
      "Native Levi waypoint manager. One clean module for selecting, creating, "
      "editing, hiding and deleting persistent waypoints.";

  bool ok = pl::modmenu::ModuleBuilder(MainModuleId, "Waypoint Manager")
                .modId(getSelf().getId())
                .description(description)
                .defaultEnabled(mConfig.enabled)
                .onToggle([this](std::string_view moduleId, bool enabled) {
                  onMainModuleToggle(moduleId, enabled);
                })
                .onConfigChanged([this](std::string_view moduleId, std::string_view key,
                                        std::string_view value) {
                  onMainModuleConfigChanged(moduleId, key, value);
                })
                .registerModule();

  if (!ok) {
    getSelf().getLogger().error("Failed to register module {}", MainModuleId);
    mMainModuleRegistered = false;
    return false;
  }

  mMainModuleRegistered = true;
  rebuildSchema();
  getSelf().getLogger().info("Registered single-module native waypoint manager");
  return true;
}

void WaypointManagerMod::onMainModuleToggle(std::string_view moduleId, bool enabled) {
  (void)moduleId;
  if (!mConfigFile) return;

  mConfig.enabled = enabled;
  mConfigFile->value() = mConfig;
  mConfigFile->save();
}

void WaypointManagerMod::onMainModuleConfigChanged(std::string_view moduleId,
                                                   std::string_view key,
                                                   std::string_view value) {
  (void)moduleId;
  if (!mConfigFile) return;

  if (key == "selectedWaypoint") {
    if (value == "new") {
      resetEditorForNewWaypoint();
    } else {
      const int id = parseInt(value, 0);
      if (id > 0 && findWaypointById(id)) {
        loadWaypointIntoEditor(id);
      } else {
        resetEditorForNewWaypoint();
      }
    }
    persistAndRefresh();
    return;
  }

  if (key == "stageCustomName") {
    mConfig.stageCustomName = std::string(value.substr(0, 48));
  } else if (key == "stageX") {
    mConfig.stageX = clampInt(parseInt(value, mConfig.stageX), -2000000, 2000000);
  } else if (key == "stageY") {
    mConfig.stageY = clampInt(parseInt(value, mConfig.stageY), -64, 320);
  } else if (key == "stageZ") {
    mConfig.stageZ = clampInt(parseInt(value, mConfig.stageZ), -2000000, 2000000);
  } else if (key == "stageDimensionIndex") {
    mConfig.stageDimensionIndex = clampInt(parseInt(value, mConfig.stageDimensionIndex), 0, 2);
  } else if (key == "stageVisible") {
    mConfig.stageVisible = (value == "true" || value == "1") ? 1 : 0;
  } else if (key == "maxVisible") {
    mConfig.maxVisible = clampInt(parseInt(value, mConfig.maxVisible), 1, 64);
  } else if (key == "newWaypoint") {
    resetEditorForNewWaypoint();
  } else if (key == "saveWaypoint") {
    saveEditedWaypoint();
  } else if (key == "deleteWaypoint") {
    deleteEditedWaypoint();
  } else {
    return;
  }

  persistAndRefresh();
}

void WaypointManagerMod::rebuildSchema() {
  if (mMainModuleRegistered) {
    pl::modmenu::setConfigSchemaJson(MainModuleId, buildSchemaJson());
  }
}

void WaypointManagerMod::resetEditorForNewWaypoint() {
  mConfig.stageSlotId = 0;
  mConfig.stageNameIndex = 0;
  mConfig.stageCustomName = fmt::format("Waypoint {}", mConfig.nextWaypointId);
  mConfig.stageX = 0;
  mConfig.stageY = 64;
  mConfig.stageZ = 0;
  mConfig.stageDimensionIndex = 0;
  mConfig.stageVisible = 1;
}

void WaypointManagerMod::loadWaypointIntoEditor(int id) {
  auto *waypoint = findWaypointById(id);
  if (!waypoint) return;

  mConfig.stageSlotId = waypoint->id;
  mConfig.stageNameIndex = waypoint->nameIndex;
  mConfig.stageCustomName = displayName(*waypoint);
  mConfig.stageX = waypoint->x;
  mConfig.stageY = waypoint->y;
  mConfig.stageZ = waypoint->z;
  mConfig.stageDimensionIndex = waypoint->dimensionIndex;
  mConfig.stageVisible = waypoint->visible ? 1 : 0;
  mConfig.activeWaypointId = waypoint->id;
}

void WaypointManagerMod::saveEditedWaypoint() {
  Waypoint *target = nullptr;

  if (mConfig.stageSlotId > 0) {
    target = findWaypointById(mConfig.stageSlotId);
  }

  if (!target) {
    Waypoint fresh;
    fresh.id = mConfig.nextWaypointId++;
    target = &mConfig.waypoints.emplace_back(std::move(fresh));
  }

  target->nameIndex = mConfig.stageNameIndex;
  target->customName = mConfig.stageCustomName;
  if (target->customName.empty()) {
    target->customName = fmt::format("Waypoint {}", target->id);
  }
  target->x = clampInt(mConfig.stageX, -2000000, 2000000);
  target->y = clampInt(mConfig.stageY, -64, 320);
  target->z = clampInt(mConfig.stageZ, -2000000, 2000000);
  target->dimensionIndex = clampInt(mConfig.stageDimensionIndex, 0, 2);
  target->visible = mConfig.stageVisible != 0;

  mConfig.stageSlotId = target->id;
  mConfig.activeWaypointId = target->id;

  getSelf().getLogger().info("Saved waypoint #{} '{}'", target->id, target->customName);
}

void WaypointManagerMod::deleteEditedWaypoint() {
  if (mConfig.stageSlotId <= 0) return;

  const int id = mConfig.stageSlotId;
  auto it = std::remove_if(mConfig.waypoints.begin(), mConfig.waypoints.end(),
                           [id](const Waypoint &waypoint) { return waypoint.id == id; });
  if (it == mConfig.waypoints.end()) return;
  mConfig.waypoints.erase(it, mConfig.waypoints.end());

  if (mConfig.activeWaypointId == id) mConfig.activeWaypointId = 0;
  resetEditorForNewWaypoint();
  getSelf().getLogger().info("Deleted waypoint #{}", id);
}

void WaypointManagerMod::persistAndRefresh() {
  if (!mConfigFile) return;
  mConfig.version = 2;
  mConfigFile->value() = mConfig;
  mConfigFile->save();
  rebuildSchema();
}

Waypoint *WaypointManagerMod::findWaypointById(int id) {
  for (auto &waypoint : mConfig.waypoints) {
    if (waypoint.id == id) return &waypoint;
  }
  return nullptr;
}

std::string WaypointManagerMod::buildWaypointSummary() const {
  if (mConfig.waypoints.empty()) {
    return "No saved waypoints yet. Press New Waypoint to create your first location.";
  }

  int shown = 0;
  std::string summary;
  for (const auto &waypoint : mConfig.waypoints) {
    if (shown >= mConfig.maxVisible) break;

    const std::string line = fmt::format(
        "{}{}  ·  {}  ·  X {}  Y {}  Z {}  ·  {}{}\n",
        waypoint.id == mConfig.activeWaypointId ? "● " : "○ ", displayName(waypoint),
        dimensionName(waypoint.dimensionIndex), waypoint.x, waypoint.y, waypoint.z,
        waypoint.visible ? "Visible" : "Hidden", waypoint.visible ? "" : "");

    if (summary.size() + line.size() > 880) {
      summary += "… more waypoints hidden by Max Visible.";
      break;
    }
    summary += line;
    ++shown;
  }

  if (mConfig.waypoints.size() > static_cast<std::size_t>(shown)) {
    summary += fmt::format("\n{} more waypoint(s) not shown.",
                           mConfig.waypoints.size() - static_cast<std::size_t>(shown));
  }

  return summary;
}

std::string WaypointManagerMod::buildSchemaJson() const {
  using namespace pl::modmenu;

  ConfigSchemaBuilder schema;
  schema.defaultCategory("waypoints")
      .category("waypoints", "Waypoints", "Browse and select saved locations.")
      .category("editor", "Editor", "Create or edit a waypoint.")
      .category("settings", "Settings", "Small module preferences.");

  schema.node(makeInfo("waypointSummary", "Saved Waypoints", buildWaypointSummary(), "waypoints"));

  auto selected = makeNode("selectedWaypoint", "Selected Waypoint", "waypoints",
                           ConfigControlTypeV2::Choice);
  selected.choiceStyle = ConfigChoiceStyleV2::Dropdown;
  selected.searchable = true;
  selected.currentValue = mConfig.stageSlotId > 0 ? std::to_string(mConfig.stageSlotId) : "new";
  selected.options.push_back({"new", "＋ New waypoint", "Start a fresh waypoint", "", false, {}, false});
  for (const auto &waypoint : mConfig.waypoints) {
    selected.options.push_back({
        std::to_string(waypoint.id),
        fmt::format("#{} · {}", waypoint.id, displayName(waypoint)),
        fmt::format("{}  •  X {}  Y {}  Z {}  •  {}", dimensionName(waypoint.dimensionIndex),
                    waypoint.x, waypoint.y, waypoint.z, waypoint.visible ? "Visible" : "Hidden"),
        "", false, {}, false});
  }
  schema.node(std::move(selected));

  auto newButton = makeNode("newWaypoint", "New Waypoint", "waypoints",
                            ConfigControlTypeV2::Button);
  newButton.actionValue = "new";
  newButton.section = "Actions";
  schema.node(std::move(newButton));

  auto maxVisible = makeNode("maxVisible", "Max Visible", "settings",
                             ConfigControlTypeV2::SliderInt);
  maxVisible.minValue = "1";
  maxVisible.maxValue = "64";
  maxVisible.step = "1";
  maxVisible.unit = " waypoints";
  maxVisible.currentValue = std::to_string(mConfig.maxVisible);
  schema.node(std::move(maxVisible));

  schema.node(makeInfo("editorHint", "Editor", "Edit the selected location below, then save it.", "editor"));

  auto name = makeNode("stageCustomName", "Name", "editor", ConfigControlTypeV2::Text);
  name.currentValue = mConfig.stageCustomName;
  name.defaultValue = "Waypoint";
  name.placeholder = "e.g. Main Base";
  name.maxLength = 48;
  schema.node(std::move(name));

  auto x = makeNode("stageX", "X Coordinate", "editor", ConfigControlTypeV2::Text);
  x.currentValue = std::to_string(mConfig.stageX);
  x.placeholder = "-2000000 to 2000000";
  x.maxLength = 10;
  schema.node(std::move(x));

  auto y = makeNode("stageY", "Y Coordinate", "editor", ConfigControlTypeV2::Text);
  y.currentValue = std::to_string(mConfig.stageY);
  y.placeholder = "-64 to 320";
  y.maxLength = 5;
  schema.node(std::move(y));

  auto z = makeNode("stageZ", "Z Coordinate", "editor", ConfigControlTypeV2::Text);
  z.currentValue = std::to_string(mConfig.stageZ);
  z.placeholder = "-2000000 to 2000000";
  z.maxLength = 10;
  schema.node(std::move(z));

  auto dimension = makeNode("stageDimensionIndex", "Dimension", "editor",
                            ConfigControlTypeV2::Choice);
  dimension.choiceStyle = ConfigChoiceStyleV2::Segmented;
  dimension.currentValue = std::to_string(mConfig.stageDimensionIndex);
  dimension.options = {{"0", "Overworld"}, {"1", "Nether"}, {"2", "End"}};
  schema.node(std::move(dimension));

  auto visibility = makeNode("stageVisible", "Show Waypoint", "editor",
                             ConfigControlTypeV2::Toggle);
  visibility.currentValue = mConfig.stageVisible ? "true" : "false";
  visibility.defaultValue = "true";
  schema.node(std::move(visibility));

  auto saveButton = makeNode("saveWaypoint", "Save Waypoint", "editor",
                             ConfigControlTypeV2::Button);
  saveButton.actionValue = "save";
  saveButton.section = "Actions";
  schema.node(std::move(saveButton));

  auto deleteButton = makeNode("deleteWaypoint", "Delete Selected Waypoint", "editor",
                               ConfigControlTypeV2::Button);
  deleteButton.actionValue = "delete";
  deleteButton.section = "Danger Zone";
  deleteButton.disabled = (mConfig.stageSlotId <= 0);
  schema.node(std::move(deleteButton));

  schema.node(makeInfo("nativeInfo", "Levi Native Module",
                       "Built as a native Levi module using Mod Menu V2. The HUD WP button opens this manager directly.",
                       "settings"));

  return schema.toJson();
}

} // namespace waypointmanager
