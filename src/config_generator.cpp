#include "mod/Config.h"

#include <pl/Config.hpp>

#include <cstdio>
#include <filesystem>

int main(int argc, char **argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
    return 1;
  }

  std::filesystem::path outDir(argv[1]);
  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);
  if (ec) {
    std::fprintf(stderr, "failed to create %s: %s\n", outDir.string().c_str(), ec.message().c_str());
    return 1;
  }

  pl::config::ConfigFile<waypointmanager::ModConfig> config(
      waypointmanager::ModConfig{}, outDir / "config.json", outDir / "config.schema.json");
  if (!config.load()) {
    std::fprintf(stderr, "failed to generate default config\n");
    return 1;
  }
  config.save();
  return 0;
}
