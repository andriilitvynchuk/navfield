#include <cstdlib>
#include <fstream>
#include <string>

#include "argparse/argparse.hpp"
#include "nlohmann/json.hpp"
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

namespace navfield {

struct Config {
  std::string name{"navfield"};
  int timeout_ms{1000};
};

Config load_config(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return Config{};
  }
  const nlohmann::json j = nlohmann::json::parse(file);
  Config cfg;
  if (j.contains("name")) cfg.name = j["name"].get<std::string>();
  if (j.contains("timeout_ms")) cfg.timeout_ms = j["timeout_ms"].get<int>();
  return cfg;
}

quill::Logger* setup_logger() {
  quill::Backend::start();
  auto sink =
      quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
  return quill::Frontend::create_or_get_logger("main", std::move(sink));
}

struct Args {
  std::string config_path{"config.json"};
};

Args parse_args(int argc, char* argv[]) {
  argparse::ArgumentParser program("navfield", "1.0.0");
  program.add_argument("--config")
      .default_value(std::string{"config.json"})
      .help("Path to JSON config file");
  program.parse_args(argc, argv);
  return Args{.config_path = program.get<std::string>("--config")};
}

}  // namespace navfield

int main(int argc, char* argv[]) {
  const navfield::Args args = navfield::parse_args(argc, argv);
  quill::Logger* logger = navfield::setup_logger();

  const std::string config_path = args.config_path;
  LOG_INFO(logger, "Loading config from: {}", config_path);

  const navfield::Config cfg = navfield::load_config(config_path);
  LOG_INFO(logger, "Config: name={}, timeout_ms={}", cfg.name, cfg.timeout_ms);

  quill::Backend::stop();
  return EXIT_SUCCESS;
}
