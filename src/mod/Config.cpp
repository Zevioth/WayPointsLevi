#include "mod/Config.h"

#include <pl/Config.hpp>

namespace pl::config {

template <> struct Schema<waypointmanager::ModConfig> {
  static constexpr std::string_view title = "Waypoint Manager";
  static constexpr std::string_view description =
      "Persistent waypoint storage used by the native Levi Mod Menu V2 manager.";

  static constexpr FieldSchema field(std::string_view name) {
    if (name == "version") {
      return {.title = "Version", .readOnly = true};
    }
    if (name == "enabled") {
      return {.title = "Mod Enabled"};
    }
    if (name == "maxVisible") {
      return {.title = "Max Visible Waypoints", .minimum = 1, .maximum = 64};
    }
    if (name == "stageCustomName") {
      return {.title = "Editor Name"};
    }
    if (name == "stageX" || name == "stageZ") {
      return {.title = "Editor Horizontal Coordinate"};
    }
    if (name == "stageY") {
      return {.title = "Editor Y Coordinate", .minimum = -64, .maximum = 320};
    }
    return {};
  }
};

} // namespace pl::config
