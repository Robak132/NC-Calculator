#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "OptFission.h"

class FissionApp {
  struct CliOptions {
    int sizeX = 5;
    int sizeY = 5;
    int sizeZ = 5;
    int steps = 50000;
    int progressEvery = 5000;
    bool useNet = false;
    bool ensureHeatNeutral = false;
    std::string goal = "power";
    std::string fuelName;
    std::filesystem::path fuelConfigDir;
  };

  struct FuelPreset {
    std::string name;
    double forgeEnergy = 0.0;
    double heat = 0.0;
  };

  CliOptions options_;

  static std::string normalizeFuelKey(const std::string &name) {
    std::string out;
    out.reserve(name.size());
    for (unsigned char c : name) {
      if (c == '-' || c == '_' || std::isspace(c))
        continue;
      out.push_back(static_cast<char>(std::toupper(c)));
    }
    return out;
  }

  static std::optional<std::string> readFileText(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in)
      return std::nullopt;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return content;
  }

  static std::optional<std::string> extractStringField(const std::string &obj, const std::string &field) {
    const std::regex rx("\"" + field + R"(\"\s*:\s*\"([^\"]+)\")");
    std::smatch match;
    if (std::regex_search(obj, match, rx) && match.size() >= 2)
      return match[1].str();
    return std::nullopt;
  }

  static std::optional<double> extractNumberField(const std::string &obj, const std::string &field) {
    const std::regex rx("\"" + field + R"(\"\s*:\s*(-?[0-9]+(?:\.[0-9]+)?))");
    std::smatch match;
    if (std::regex_search(obj, match, rx) && match.size() >= 2)
      return std::stod(match[1].str());
    return std::nullopt;
  }

  static std::vector<FuelPreset> parseFuelPresets(const std::string &json) {
    std::vector<FuelPreset> fuels;
    const std::regex objRx(R"(\{[^{}]*\})");
    for (std::sregex_iterator it(json.begin(), json.end(), objRx), end; it != end; ++it) {
      const std::string obj = it->str();
      const auto name = extractStringField(obj, "name");
      const auto forgeEnergy = extractNumberField(obj, "forge_energy");
      const auto heat = extractNumberField(obj, "heat");
      if (!name || !forgeEnergy || !heat)
        continue;
      fuels.push_back(FuelPreset{*name, *forgeEnergy, *heat});
    }
    return fuels;
  }

  static bool parseIntArg(const char *raw, int &out) {
    try {
      std::string s(raw);
      size_t idx = 0;
      const int value = std::stoi(s, &idx);
      if (idx != s.size())
        return false;
      out = value;
      return true;
    } catch (...) {
      return false;
    }
  }

  static int parseGoal(const std::string &goal) {
    if (goal == "power")
      return Fission::GoalPower;
    if (goal == "breeder")
      return Fission::GoalBreeder;
    if (goal == "efficiency")
      return Fission::GoalEfficiency;
    throw std::runtime_error("Unsupported goal: " + goal + " (expected power, breeder or efficiency)");
  }

  static void initCoolingRates(Fission::Settings &settings) {
    for (int i = 0; i < Fission::Air; ++i) {
      settings.limit[i] = -1;
      settings.coolingRates[i] = 0.0;
    }
    settings.coolingRates[Fission::Water] = 60;
    settings.coolingRates[Fission::Copper] = 80;
    settings.coolingRates[Fission::Cryotheum] = 200;
    settings.coolingRates[Fission::Enderium] = 120;
    settings.coolingRates[Fission::Redstone] = 90;
    settings.coolingRates[Fission::Helium] = 140;
    settings.coolingRates[Fission::Boron] = 160;
    settings.coolingRates[Fission::Lapis] = 120;
    settings.coolingRates[Fission::Emerald] = 160;
    settings.coolingRates[Fission::Quartz] = 90;
    settings.coolingRates[Fission::Tin] = 120;
    settings.coolingRates[Fission::Aluminium] = 175;
    settings.coolingRates[Fission::Magnesium] = 110;
    settings.coolingRates[Fission::Manganese] = 150;
    settings.coolingRates[Fission::Glowstone] = 130;
  }

  static void printSummary(const Fission::Sample &best) {
    std::cout << "\nBest result:\n";
    std::cout << "  Power: " << best.value.power << " FE/t\n";
    std::cout << "  Avg Power: " << best.value.avgPower << " FE/t\n";
    std::cout << "  Heat: " << best.value.heat << " H/t\n";
    std::cout << "  Cooling: " << best.value.cooling << " H/t\n";
    std::cout << "  Net Heat: " << best.value.netHeat << " H/t\n";
    std::cout << "  Duty Cycle: " << best.value.dutyCycle * 100.0 << " %\n";
    std::cout << "  Fuel Use Rate: " << best.value.avgBreed << "x\n";
    std::cout << "  Efficiency: " << best.value.efficiency * 100.0 << " %\n";
  }

  static void printUsage() {
    std::cout << "Usage: fission-cmd [options]\n"
                 "Options:\n"
                 "  --size <x> <y> <z>                Core size (default: 5 5 5)\n"
                 "  --steps <n>                       Optimizer steps (default: 50000)\n"
                 "  --progress-every <n>              Print progress interval (default: 5000)\n"
                 "  --goal <power|breeder|efficiency> Optimization goal (default: power)\n"
                 "  --fuel <name>                     Fuel name from config/fission_fuel\n"
                 "  --fuel-config-dir <path>          Override fuel config directory\n"
                 "  --heat-neutral                    Enforce net heat <= 0\n"
                 "  --use-net                         Enable neural net mode\n"
                 "  --help                            Show this message\n";
  }

  bool parseArgs(const int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--help") {
        printUsage();
        return false;
      }
      if (arg == "--size") {
        if (i + 3 >= argc || !parseIntArg(argv[i + 1], options_.sizeX) || !parseIntArg(argv[i + 2], options_.sizeY) || !parseIntArg(argv[i + 3], options_.sizeZ))
          throw std::runtime_error("Invalid --size arguments");
        i += 3;
        continue;
      }
      if (arg == "--steps") {
        if (i + 1 >= argc || !parseIntArg(argv[i + 1], options_.steps))
          throw std::runtime_error("Invalid --steps value");
        ++i;
        continue;
      }
      if (arg == "--progress-every") {
        if (i + 1 >= argc || !parseIntArg(argv[i + 1], options_.progressEvery))
          throw std::runtime_error("Invalid --progress-every value");
        ++i;
        continue;
      }
      if (arg == "--goal") {
        if (i + 1 >= argc)
          throw std::runtime_error("Missing --goal value");
        options_.goal = argv[++i];
        continue;
      }
      if (arg == "--fuel") {
        if (i + 1 >= argc)
          throw std::runtime_error("Missing --fuel value");
        options_.fuelName = argv[++i];
        continue;
      }
      if (arg == "--fuel-config-dir") {
        if (i + 1 >= argc)
          throw std::runtime_error("Missing --fuel-config-dir value");
        options_.fuelConfigDir = argv[++i];
        continue;
      }
      if (arg == "--heat-neutral") {
        options_.ensureHeatNeutral = true;
        continue;
      }
      if (arg == "--use-net") {
        options_.useNet = true;
        continue;
      }
      throw std::runtime_error("Unknown option: " + arg);
    }

    if (options_.sizeX <= 0 || options_.sizeY <= 0 || options_.sizeZ <= 0)
      throw std::runtime_error("Core size must be positive");
    if (options_.steps <= 0)
      throw std::runtime_error("--steps must be positive");
    if (options_.progressEvery <= 0)
      throw std::runtime_error("--progress-every must be positive");

    return true;
  }

  static std::filesystem::path resolveDefaultFuelConfigDir(const char *argv0) {
    std::vector<std::filesystem::path> candidates = {
      "config/fission_fuel",
      "../config/fission_fuel",
      "../../config/fission_fuel"
    };
    if (argv0 != nullptr && std::strlen(argv0) > 0) {
      const std::filesystem::path exeDir = std::filesystem::absolute(argv0).parent_path();
      candidates.push_back(exeDir / "../config/fission_fuel");
      candidates.push_back(exeDir / "config/fission_fuel");
    }
    for (const auto &candidate : candidates) {
      if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate))
        return std::filesystem::weakly_canonical(candidate);
    }
    return candidates.front();
  }

  static std::unordered_map<std::string, FuelPreset> loadFuelMap(const std::filesystem::path &fuelDir) {
    std::unordered_map<std::string, FuelPreset> result;
    if (!std::filesystem::exists(fuelDir) || !std::filesystem::is_directory(fuelDir))
      return result;
    for (const auto &entry : std::filesystem::directory_iterator(fuelDir)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".json")
        continue;
      const auto text = readFileText(entry.path());
      if (!text)
        continue;
      for (const auto &fuel : parseFuelPresets(*text))
        result[normalizeFuelKey(fuel.name)] = fuel;
    }
    return result;
  }

  Fission::Settings buildSettings(const FuelPreset &fuel) const {
    Fission::Settings settings{};
    settings.sizeX = options_.sizeX;
    settings.sizeY = options_.sizeY;
    settings.sizeZ = options_.sizeZ;
    settings.fuelBasePower = fuel.forgeEnergy;
    settings.fuelBaseHeat = fuel.heat;
    settings.ensureHeatNeutral = options_.ensureHeatNeutral;
    settings.goal = parseGoal(options_.goal);
    settings.symX = false;
    settings.symY = false;
    settings.symZ = false;
    settings.genMult = 1.0;
    settings.heatMult = 1.0;
    settings.modFEMult = 100.0;
    settings.modHeatMult = 100.0;
    settings.FEGenMult = 1.0;
    initCoolingRates(settings);
    return settings;
  }

public:
  int run(int argc, char **argv) {
    try {
      if (!parseArgs(argc, argv))
        return 0;

      if (options_.fuelConfigDir.empty())
        options_.fuelConfigDir = resolveDefaultFuelConfigDir(argc > 0 ? argv[0] : nullptr);

      const auto fuels = loadFuelMap(options_.fuelConfigDir);
      if (fuels.empty()) {
        std::cerr << "No fuel presets found in: " << options_.fuelConfigDir << '\n';
        return 1;
      }

      const auto fuelKey = options_.fuelName.empty() ? fuels.begin()->first : normalizeFuelKey(options_.fuelName);
      const auto it = fuels.find(fuelKey);
      if (it == fuels.end()) {
        std::cerr << "Fuel not found: " << options_.fuelName << "\nAvailable fuels:\n";
        for (const auto &[k, v] : fuels)
          std::cerr << "  - " << v.name << '\n';
        return 1;
      }

      const Fission::Settings settings = buildSettings(it->second);

      std::cout << "Running optimization\n";
      std::cout << "  Fuel: " << it->second.name << " (power=" << settings.fuelBasePower << ", heat=" << settings.fuelBaseHeat << ")\n";
      std::cout << "  Size: " << settings.sizeX << "x" << settings.sizeY << "x" << settings.sizeZ << '\n';
      std::cout << "  Goal: " << options_.goal << '\n';
      std::cout << "  Fuel config dir: " << options_.fuelConfigDir << "\n\n";

      Fission::Opt optimizer(settings, options_.useNet);
      for (int i = 1; i <= options_.steps; ++i) {
        optimizer.step();
        if (i % options_.progressEvery == 0) {
          std::cout << "step=" << i
                    << " episode=" << optimizer.getNEpisode()
                    << " stage=" << optimizer.getNStage()
                    << " iter=" << optimizer.getNIteration() << '\n';
        }
      }
      printSummary(optimizer.getBest());
      return 0;
    } catch (const std::exception &e) {
      std::cerr << "Error: " << e.what() << '\n';
      printUsage();
      return 1;
    }
  }
};

int main(int argc, char **argv) {
  FissionApp app;
  return app.run(argc, argv);
}
