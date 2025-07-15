#include "Fission.h"
#include <cmath>
#include <nlohmann/json.hpp>
#include <xtensor/xview.hpp>

namespace Fission {
  void Evaluation::compute(const Settings &settings) {
    const double moderatorsFE = moderatorCellMultiplier * settings.modFEMult / 100.0;
    const double moderatorsHeat = moderatorCellMultiplier * settings.modHeatMult / 100.0;
    heat = settings.fuelBaseHeat * (cellsHeatMult + moderatorsHeat);
    power = settings.fuelBasePower * (cellsEnergyMult + moderatorsFE);
    power = trunc(power * heatMultiplier(heat, cooling, settings.heatMult) * settings.FEGenMult / 10.0 * settings.genMult);
    netHeat = heat - cooling;
    dutyCycle = std::min(1.0, cooling / heat);
    avgPower = power * dutyCycle;
    avgBreed = breed * dutyCycle;
    double mult = fuelCellMultiplier > breed ? 1.0 * fuelCellMultiplier / breed : breed; // Silly double cast
    efficiency = breed ? power / (settings.fuelBasePower * mult) : 1.0;
  }

  double Evaluation::heatMultiplier(const double heatPerTick, const double coolingPerTick, const double heatMult) {
    if (heatPerTick == 0.0) {
      return 0.0;
    }
    const double c = std::max(1.0, coolingPerTick);
    double heatMultiplier = std::log10(heatPerTick / c) / (1 + std::exp(heatPerTick / c * heatMult)) + 1;
    return round(heatMultiplier * 100) / 100;
  }

  Evaluator::Evaluator(const Settings &settings) :
      settings(settings), rules(xt::empty<std::string>({settings.sizeX, settings.sizeY, settings.sizeZ})),
      active(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})),
      isModeratorInLine(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})),
      visited(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})) {}

  const Component *Evaluator::getTileSafe(int x, int y, int z) const {
    if (!state->in_bounds(x, y, z)) {
      return nullptr;
    }
    return &(this->settings.components.at((*state)(x, y, z)));
  }

  bool Evaluator::hasCellInLine(int x, int y, int z, int dx, int dy, int dz) {
    for (int n{}; n <= 4; ++n) {
      x += dx;
      y += dy;
      z += dz;
      const Component *tile = getTileSafe(x, y, z);
      if (tile->isCell()) {
        for (int i{}; i < n; ++i) {
          x -= dx;
          y -= dy;
          z -= dz;
          isModeratorInLine(x, y, z) = true;
        }
        if (getTileSafe(x, y, z)->isModerator()) {
          active(x, y, z) = true;
        }
        return true;
      }
      if (!tile->isModerator()) {
        return false;
      }
    }
    return false;
  }

  int Evaluator::countAdjFuelCells(int x, int y, int z) {
    return hasCellInLine(x, y, z, -1, 0, 0) + hasCellInLine(x, y, z, +1, 0, 0) + hasCellInLine(x, y, z, 0, -1, 0) + hasCellInLine(x, y, z, 0, +1, 0) +
           hasCellInLine(x, y, z, 0, 0, -1) + hasCellInLine(x, y, z, 0, 0, +1);
  }

  int Evaluator::isActiveSafe(const std::string &tileName, const int x, const int y, const int z) const {
    if (!state->in_bounds(x, y, z)) {
      return false;
    }
    return getTileFromState(x, y, z).getName() == tileName && active(x, y, z) && tileName == "Moderator";
  }

  int Evaluator::countActiveNeighbors(const std::string &tileName, const int x, const int y, const int z) const {
    return
      + isActiveSafe(tileName, x - 1, y, z)
      + isActiveSafe(tileName, x + 1, y, z)
      + isActiveSafe(tileName, x, y - 1, z)
      + isActiveSafe(tileName, x, y + 1, z)
      + isActiveSafe(tileName, x, y, z - 1)
      + isActiveSafe(tileName, x, y, z + 1);
  }

  bool Evaluator::isTileSafe(const std::string &tileName, int x, int y, int z) const {
    if (!state->in_bounds(x, y, z)) {
      return false;
    }
    return getTileFromState(x, y, z).getName() == tileName;
  }

  int Evaluator::countNeighbors(const std::string &tileName, int x, int y, int z) const {
    return
      + isTileSafe(tileName, x - 1, y, z)
      + isTileSafe(tileName, x + 1, y, z)
      + isTileSafe(tileName, x, y - 1, z)
      + isTileSafe(tileName, x, y + 1, z)
      + isTileSafe(tileName, x, y, z - 1)
      + isTileSafe(tileName, x, y, z + 1);
  }

  int Evaluator::countCasingNeighbors(int x, int y, int z) const {
    return +!state->in_bounds(x - 1, y, z) + !state->in_bounds(x + 1, y, z) + !state->in_bounds(x, y - 1, z) + !state->in_bounds(x, y + 1, z) +
           !state->in_bounds(x, y, z - 1) + !state->in_bounds(x, y, z + 1);
  }

  int Evaluator::countPassiveHeatsinks() const {
    return std::count_if(settings.components.begin(), settings.components.end(), [](const std::pair<int, Component> &pair) {
      return pair.second.isPassive();
    });
  }

  Component Evaluator::getTileFromState(int x, int y, int z) const {
    return settings.components.at((*this->state)(x, y, z));
  }

  void Evaluator::run(const xt::xtensor<int, 3> &currentState, Evaluation &result) {
    result.invalidTiles.clear();
    result.cellsHeatMult = 0;
    result.cellsEnergyMult = 0;
    result.fuelCellMultiplier = 0;
    result.moderatorCellMultiplier = 0;
    result.cooling = 0.0;
    result.breed = 0; // Number of Cells
    active.fill(false);
    isModeratorInLine.fill(false);
    this->state = &currentState;
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          if (Component tile = getTileFromState(x, y, z); tile.isCell()) {
            int adjFuelCells = countAdjFuelCells(x, y, z);
            rules(x, y, z) = -1;
            ++result.breed;
            result.cellsHeatMult += (adjFuelCells + 1) * (adjFuelCells + 2) / 2;
            result.cellsEnergyMult += adjFuelCells + 1;
            result.moderatorCellMultiplier += countActiveNeighbors("Moderator", x, y, z) * (adjFuelCells + 1);
          } else {
            if (tile.isHeatsink()) {
              rules(x, y, z) = tile.getName();
            } else {
              rules(x, y, z) = -1;
            }
          }
        }
      }
    }

    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          if (getTileFromState(x, y, z).isModerator()) {
            if (!isModeratorInLine(x, y, z)) {
              result.invalidTiles.emplace_back(x, y, z);
            }
          } else {
            if (rules(x, y, z) == "Redstone") {
              active(x, y, z) = countNeighbors("Cell", x, y, z);
            } else if (rules(x, y, z) == "Lapis") {
              active(x, y, z) = countNeighbors("Cell", x, y, z) && countCasingNeighbors(x, y, z);
            } else if (rules(x, y, z) == "Enderium") {
              active(x, y, z) = countCasingNeighbors(x, y, z) == 3 &&
                                (!x || x == settings.sizeX - 1) &&
                                (!y || y == settings.sizeY - 1) &&
                                (!z || z == settings.sizeZ - 1);
            } else if (rules(x, y, z) == "Cryotheum") {
              active(x, y, z) = countNeighbors("Cell", x, y, z) >= 2;
            } else if (rules(x, y, z) == "Manganese") {
              active(x, y, z) = countNeighbors("Cell", x, y, z) >= 2;
            }
          }
        }
      }
    }

    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          if (rules(x, y, z) == "Water") {
            active(x, y, z) = countNeighbors("Cell", x, y, z) || countActiveNeighbors("Moderator", x, y, z);
          } else if (rules(x, y, z) == "Quartz") {
            active(x, y, z) = countActiveNeighbors("Moderator", x, y, z);
          } else if (rules(x, y, z) == "Glowstone") {
            active(x, y, z) = countActiveNeighbors("Moderator", x, y, z) >= 2;
          } else if (rules(x, y, z) == "Helium") {
            active(x, y, z) = countActiveNeighbors("Redstone", x, y, z) == 1 && countCasingNeighbors(x, y, z);
          } else if (rules(x, y, z) == "Emerald") {
            active(x, y, z) = countActiveNeighbors("Moderator", x, y, z) && countNeighbors("Cell", x, y, z);
          } else if (rules(x, y, z) == "Tin") {
            active(x, y, z) = isActiveSafe("Lapis", x - 1, y, z) && isActiveSafe("Lapis", x + 1, y, z) ||
                              isActiveSafe("Lapis", x, y - 1, z) && isActiveSafe("Lapis", x, y + 1, z) ||
                              isActiveSafe("Lapis", x, y, z - 1) && isActiveSafe("Lapis", x, y, z + 1);
          } else if (rules(x, y, z) == "Magnesium") {
            active(x, y, z) = countActiveNeighbors("Moderator", x, y, z) && countCasingNeighbors(x, y, z);
          } else if (rules(x, y, z) == "End Stone") {
            active(x, y, z) = countActiveNeighbors("Enderium", x, y, z);
          } else if (rules(x, y, z) == "Arsenic") {
            active(x, y, z) = countActiveNeighbors("Moderator", x, y, z) >= 3;
          }
        }
      }
    }

    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          switch (rules(x, y, z)) {
            case "Gold":
              active(x, y, z) = countActiveNeighbors("Water", x, y, z) && countActiveNeighbors("Redstone", x, y, z);
              break;
            case "Diamond":
              active(x, y, z) = countActiveNeighbors("Water", x, y, z) && countActiveNeighbors("Quartz", x, y, z);
              break;
            case "Copper":
              active(x, y, z) = countActiveNeighbors("Glowstone", x, y, z);
            case "Prismarine":
              active(x, y, z) = countActiveNeighbors("Water", x, y, z);
              break;
            case "Obsidian":
              active(x, y, z) = isActiveSafe("Glowstone", x - 1, y, z) && isActiveSafe("Glowstone", x + 1, y, z) ||
                                isActiveSafe("Glowstone", x, y - 1, z) && isActiveSafe("Glowstone", x, y + 1, z) ||
                                isActiveSafe("Glowstone", x, y, z - 1) && isActiveSafe("Glowstone", x, y, z + 1);
              break;
            case "Aluminium":
              active(x, y, z) = countActiveNeighbors("Quartz", x, y, z) && countActiveNeighbors("Lapis", x, y, z);
              break;
            case "Boron":
              active(x, y, z) = countActiveNeighbors("Quartz", x, y, z) && (countCasingNeighbors(x, y, z) || countActiveNeighbors("Moderator", x, y, z));
              break;
            case "Silver":
              active(x, y, z) = countActiveNeighbors("Glowstone", x, y, z) >= 2 && countActiveNeighbors("Tin", x, y, z);
            default:
              break;
          }
        }
      }
    }

    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          if (rules(x, y, z) == "Iron") {
            active(x, y, z) = countActiveNeighbors("Gold", x, y, z);
          } else if (rules(x, y, z) == "Fluorite") {
            active(x, y, z) = countActiveNeighbors("Prismarine", x, y, z) && countActiveNeighbors("Gold", x, y, z);
          } else if (rules(x, y, z) == "Nether Brick") {
            active(x, y, z) = countActiveNeighbors("Obsidian", x, y, z);
          }
        }
      }
    }

    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          if (rules(x, y, z) == "Lead") {
            active(x, y, z) = countActiveNeighbors("Iron", x, y, z);
          } else if (rules(x, y, z) == "Purpur") {
            active(x, y, z) = countActiveNeighbors("Iron", x, y, z) && countCasingNeighbors(x, y, z);
          }
        }
      }
    }

    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          Component tile = getTileFromState(x, y, z);
          if (tile.isPassive() || tile.isActive()) {
            if (rules(x, y, z) == "Slime") {
              active(x, y, z) = countActiveNeighbors("Water", x, y, z) && countActiveNeighbors("Lead", x, y, z);
            } else if (rules(x, y, z) == "Lithium") {
              active(x, y, z) = isActiveSafe("Lead", x - 1, y, z) && isActiveSafe("Lead", x + 1, y, z) ||
                                isActiveSafe("Lead", x, y - 1, z) && isActiveSafe("Lead", x, y + 1, z) ||
                                isActiveSafe("Lead", x, y, z - 1) && isActiveSafe("Lead", x, y, z + 1);
            } else if (rules(x, y, z) == "Nitrogen") {
              active(x, y, z) = countActiveNeighbors("Purpur", x, y, z) && countActiveNeighbors("Copper", x, y, z);
            }

            if (active(x, y, z)) {
              result.cooling += tile.getCoolingRate();
            } else {
              result.invalidTiles.emplace_back(x, y, z);
            }
          }
        }
      }
    }

    result.compute(settings);
  }
} // namespace Fission
