#pragma once

#include <fstream>
#include <stdexcept>
#include <string>

#include "nlohmann/json.hpp"

namespace navfield {

struct CameraConfig {
  int stereo_fps{60};
  int rgb_fps{30};
  std::string stereo_resolution{"THE_480_P"};
  std::string rgb_resolution{"THE_1080_P"};
  bool show_fps{true};
};

inline CameraConfig load_camera_config(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open config: " + path);
  }
  const nlohmann::json j = nlohmann::json::parse(file);
  CameraConfig cfg;
  if (j.contains("stereo_fps"))
    cfg.stereo_fps = j["stereo_fps"].get<int>();
  if (j.contains("rgb_fps"))
    cfg.rgb_fps = j["rgb_fps"].get<int>();
  if (j.contains("stereo_resolution"))
    cfg.stereo_resolution = j["stereo_resolution"].get<std::string>();
  if (j.contains("rgb_resolution"))
    cfg.rgb_resolution = j["rgb_resolution"].get<std::string>();
  if (j.contains("show_fps")) cfg.show_fps = j["show_fps"].get<bool>();
  return cfg;
}

}  // namespace navfield
