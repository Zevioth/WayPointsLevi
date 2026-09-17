#pragma once

#include <string>
#include <vector>

namespace waypointmanager {

struct Waypoint {
  int id = 0;
  int nameIndex = 0;
  std::string customName;
  int x = 0;
  int y = 64;
  int z = 0;
  int dimensionIndex = 0;
  bool visible = true;
};

struct ModConfig {
  int version = 2;
  bool enabled = true;
  int maxVisible = 16;

  std::vector<Waypoint> waypoints;
  int nextWaypointId = 1;
  int activeWaypointId = 0;

  // Kept for migration from the previous three-module implementation.
  int stageSlotId = 0;
  int stageNameIndex = 0;
  std::string stageCustomName;
  int stageX = 0;
  int stageY = 64;
  int stageZ = 0;
  int stageDimensionIndex = 0;
  int stageVisible = 1;
  int stageDeleteId = 0;
};

} // namespace waypointmanager
