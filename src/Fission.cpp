#include "Fission.h"
#include <cmath>
#include <xtensor/xview.hpp>

#include "Tile.h"

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
    double c = std::max(1.0, coolingPerTick);
    double heatMultiplier = std::log10(heatPerTick / c) / (1 + std::exp(heatPerTick / c * heatMult)) + 1;
    return round(heatMultiplier * 100) / 100;
  }

  Evaluator::Evaluator(const Settings &settings) :
      settings(settings), rules(xt::empty<int>({settings.sizeX, settings.sizeY, settings.sizeZ})),
      active(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})),
      moderatorInLine(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})),
      visited(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})) {}

  int Evaluator::getTileSafe(const int x, const int y, int const z) const {
    if (!state->in_bounds(x, y, z)) {
      return -1;
    }
    return (*state)(x, y, z);
  }

  bool Evaluator::hasCellInLine(int x, int y, int z, int dx, int dy, int dz) {
    for (int n{}; n <= 4; ++n) {
      x += dx;
      y += dy;
      z += dz;
      const int tile = getTileSafe(x, y, z);
      if (tile == Cell) {
        for (int i{}; i < n; ++i) {
          x -= dx;
          y -= dy;
          z -= dz;
          moderatorInLine(x, y, z) = true;
        }
        if (getTileSafe(x, y, z) == Moderator) {
          active(x, y, z) = true;
        }
        return true;
      }
      if (tile != Moderator) {
        return false;
      }
    }
    return false;
  }

  int Evaluator::countAdjFuelCells(const int x, const int y, int const z) {
    return hasCellInLine(x, y, z, -1, 0, 0)
         + hasCellInLine(x, y, z, +1, 0, 0)
         + hasCellInLine(x, y, z, 0, -1, 0)
         + hasCellInLine(x, y, z, 0, +1, 0)
         + hasCellInLine(x, y, z, 0, 0, -1)
         + hasCellInLine(x, y, z, 0, 0, +1);
  }

  int Evaluator::isActiveSafe(std::string tile, const int x, const int y, int const z) const {
    if (!state->in_bounds(x, y, z)) {
      return false;
    }
    return (*state)(x, y, z) == Tile::getTileID(tile) && active(x, y, z) && (tile < Active || tile == Moderator || settings.activeHeatsinkPrime);
  }

  int Evaluator::countActiveNeighbors(const std::string &tile, const int x, const int y, int const z) const {
    return
      + isActiveSafe(tile, x - 1, y, z)
      + isActiveSafe(tile, x + 1, y, z)
      + isActiveSafe(tile, x, y - 1, z)
      + isActiveSafe(tile, x, y + 1, z)
      + isActiveSafe(tile, x, y, z - 1)
      + isActiveSafe(tile, x, y, z + 1);
  }

  bool Evaluator::isTileSafe(int tile, const int x, const int y, int const z) const {
    if (!state->in_bounds(x, y, z)) {
      return false;
    }
    return (*state)(x, y, z) == tile;
  }

  int Evaluator::countNeighbors(int tile, const int x, const int y, int const z) const {
    return
      + isTileSafe(tile, x - 1, y, z)
      + isTileSafe(tile, x + 1, y, z)
      + isTileSafe(tile, x, y - 1, z)
      + isTileSafe(tile, x, y + 1, z)
      + isTileSafe(tile, x, y, z - 1)
      + isTileSafe(tile, x, y, z + 1);
  }

  int Evaluator::countCasingNeighbors(const int x, const int y, int const z) const {
    return
      + !state->in_bounds(x - 1, y, z)
      + !state->in_bounds(x + 1, y, z)
      + !state->in_bounds(x, y - 1, z)
      + !state->in_bounds(x, y + 1, z)
      + !state->in_bounds(x, y, z - 1)
      + !state->in_bounds(x, y, z + 1);
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
    moderatorInLine.fill(false);
    this->state = &currentState;
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          int tile((*this->state)(x, y, z));
          if (tile == Cell) {
            int adjFuelCells(countAdjFuelCells(x, y, z));
            rules(x, y, z) = nullptr;
            ++result.breed;
            result.cellsHeatMult += (adjFuelCells + 1) * (adjFuelCells + 2) / 2;
            result.cellsEnergyMult += adjFuelCells + 1;
            result.moderatorCellMultiplier += countActiveNeighbors("Moderator", x, y, z) * (adjFuelCells + 1);
          } else {
            if (tile < Active) {
              rules(x, y, z) = tile;
            } else if (tile < Cell) {
              rules(x, y, z) = tile - Active;
            } else {
              rules(x, y, z) = nullptr;
            }
          }
        }
      }
    }

    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          if ((*this->state)(x, y, z) == Moderator) {
            if (!moderatorInLine(x, y, z)) {
              result.invalidTiles.emplace_back(x, y, z);
            }
          } else {
            if (rules(x, y, z) == "Redstone") {
              active(x, y, z) = countNeighbors(Cell, x, y, z);
            } else if (rules(x, y, z) == "Lapis") {
              active(x, y, z) = countNeighbors(Cell, x, y, z) && countCasingNeighbors(x, y, z);
            } else if (rules(x, y, z) == "Enderium") {
              active(x, y, z) =
                  countCasingNeighbors(x, y, z) == 3 && (!x || x == settings.sizeX - 1) && (!y || y == settings.sizeY - 1) && (!z || z == settings.sizeZ - 1);
            } else if (rules(x, y, z) == "Cryotheum") {
              active(x, y, z) = countNeighbors(Cell, x, y, z) >= 2;
            } else if (rules(x, y, z) == "Manganese") {
              active(x, y, z) = countNeighbors(Cell, x, y, z) >= 2;
            }
          }
        }
      }

      for (int x{}; x < settings.sizeX; ++x) {
        for (int y{}; y < settings.sizeY; ++y) {
          for (int z{}; z < settings.sizeZ; ++z) {
            if (rules(x, y, z) == "Water") {
              active(x, y, z) = countNeighbors(Cell, x, y, z) || countActiveNeighbors("Moderator", x, y, z);
            } else if (rules(x, y, z) == "Quartz") {
              active(x, y, z) = countActiveNeighbors("Moderator", x, y, z);
            } else if (rules(x, y, z) == "Glowstone") {
              active(x, y, z) = countActiveNeighbors("Moderator", x, y, z) >= 2;
            } else if (rules(x, y, z) == "Helium") {
              active(x, y, z) = countActiveNeighbors("Redstone", x, y, z) == 1 && countCasingNeighbors(x, y, z);
            } else if (rules(x, y, z) == "Emerald") {
              active(x, y, z) = countActiveNeighbors("Moderator", x, y, z) && countNeighbors(Cell, x, y, z);
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
    }

    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          if (rules(x, y, z) == "Gold") {
            active(x, y, z) = countActiveNeighbors("Water", x, y, z) && countActiveNeighbors("Redstone", x, y, z);
          } else if (rules(x, y, z) == "Diamond") {
            active(x, y, z) = countActiveNeighbors("Water", x, y, z) && countActiveNeighbors("Quartz", x, y, z);
          } else if (rules(x, y, z) == "Copper") {
            active(x, y, z) = countActiveNeighbors("Glowstone", x, y, z);
          } else if (rules(x, y, z) == "Prismarine") {
            active(x, y, z) = countActiveNeighbors("Water", x, y, z);
          } else if (rules(x, y, z) == "Obsidian") {
            active(x, y, z) = isActiveSafe("Glowstone", x - 1, y, z) && isActiveSafe("Glowstone", x + 1, y, z) ||
                              isActiveSafe("Glowstone", x, y - 1, z) && isActiveSafe("Glowstone", x, y + 1, z) ||
                              isActiveSafe("Glowstone", x, y, z - 1) && isActiveSafe("Glowstone", x, y, z + 1);
          } else if (rules(x, y, z) == "Aluminium") {
            active(x, y, z) = countActiveNeighbors("Quartz", x, y, z) && countActiveNeighbors("Lapis", x, y, z);
          } else if (rules(x, y, z) == "Boron") {
            active(x, y, z) = countActiveNeighbors("Quartz", x, y, z) && (countCasingNeighbors(x, y, z) || countActiveNeighbors("Moderator", x, y, z));
          } else if (rules(x, y, z) == "Silver") {
            active(x, y, z) = countActiveNeighbors("Glowstone", x, y, z) >= 2 && countActiveNeighbors("Tin", x, y, z);
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
          int tile((*this->state)(x, y, z));
          if (tile < Cell) {
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
              result.cooling += settings.coolingRates[tile];
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
