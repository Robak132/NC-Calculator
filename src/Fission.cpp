#include <cmath>
#include <limits>
#include <xtensor/xview.hpp>
#include "Fission.h"

namespace Fission {
  namespace {
    constexpr int Water = static_cast<int>(Tile::Water);
    constexpr int Copper = static_cast<int>(Tile::Copper);
    constexpr int Cryotheum = static_cast<int>(Tile::Cryotheum);
    constexpr int Enderium = static_cast<int>(Tile::Enderium);
    constexpr int Redstone = static_cast<int>(Tile::Redstone);
    constexpr int Helium = static_cast<int>(Tile::Helium);
    constexpr int Boron = static_cast<int>(Tile::Boron);
    constexpr int Lapis = static_cast<int>(Tile::Lapis);
    constexpr int Emerald = static_cast<int>(Tile::Emerald);
    constexpr int Quartz = static_cast<int>(Tile::Quartz);
    constexpr int Tin = static_cast<int>(Tile::Tin);
    constexpr int Aluminium = static_cast<int>(Tile::Aluminium);
    constexpr int Magnesium = static_cast<int>(Tile::Magnesium);
    constexpr int Manganese = static_cast<int>(Tile::Manganese);
    constexpr int Glowstone = static_cast<int>(Tile::Glowstone);
    constexpr int Cell = static_cast<int>(Tile::Cell);
    constexpr int Moderator = static_cast<int>(Tile::Moderator);
  }

  void Evaluation::compute(const Settings &settings) {
    const double moderatorsFE = moderatorCellMultiplier * settings.modFEMult / 100.0;
    const double moderatorsHeat = moderatorCellMultiplier * settings.modHeatMult / 100.0;
    heat = settings.fuelBaseHeat * (cellsHeatMult + moderatorsHeat);
    const double coolingPerTick = cooling + 1.0;
    power = settings.fuelBasePower * (cellsEnergyMult + moderatorsFE);
    power = trunc(power * heatMultiplier(heat, coolingPerTick, settings.heatMult) * settings.FEGenMult / 10.0 * settings.genMult);
    const double fullVolume = (settings.sizeX + 2.0) * (settings.sizeY + 2.0) * (settings.sizeZ + 2.0);
    const double roundedLogVolume = std::round(std::log(fullVolume) * 10.0) / 10.0;
    const double multiplier = std::max(1.0, roundedLogVolume - 1.0);
    heatLimit = multiplier * 1'000'000.0;
    netHeat = heat - cooling;
    if (netHeat <= 0.0) {
      dutyCycle = 1.0;
    } else {
      // Heat-positive mode: run from 0 -> 90% heat, then cool from 90% -> 0.
      // On-phase uses net heating (heat - cooling), off-phase uses full cooling.
      const double safeHeat = std::max(heat, std::numeric_limits<double>::min());
      const double strictUpperBound = std::nextafter(1.0, 0.0);
      dutyCycle = std::min(strictUpperBound, std::max(0.0, cooling / safeHeat));
    }
    avgPower = power * dutyCycle;
    avgBreed = breed * dutyCycle;
    efficiency = breed ? power / (settings.fuelBasePower * breed) : 0.0;
  }

  double Evaluation::heatMultiplier(const double heatPerTick, const double coolingPerTick, const double heatMult) {
    if (heatPerTick == 0.0) {
      return 0.0;
    }
    double c = std::max(1.0, coolingPerTick);
    double heatMultiplier = std::log10(heatPerTick / c) / (1 + std::exp(heatPerTick / c * heatMult)) + 1;
    return round(heatMultiplier * 100) / 100;
  }

  Evaluator::Evaluator(const Settings &settings)
    :settings(settings),
    rules(xt::empty<int>({settings.sizeX, settings.sizeY, settings.sizeZ})),
    isActive(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})),
    isModeratorInLine(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})),
    visited(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})),
    state(nullptr) {}

  int Evaluator::getTileSafe(int x, int y, int z) const {
    if (!state->in_bounds(x, y, z))
      return -1;
    return (*state)(x, y, z);
  }

  bool Evaluator::hasCellInLine(int x, int y, int z, int dx, int dy, int dz) {
    for (int n{}; n <= 4; ++n) {
      x += dx; y += dy; z += dz;
      int tile(getTileSafe(x, y, z));
      if (tile == Cell) {
        for (int i{}; i < n; ++i) {
          x -= dx; y -= dy; z -= dz;
          isModeratorInLine(x, y, z) = true;
        }
        if (getTileSafe(x, y, z) == Moderator) {
          isActive(x, y, z) = true;
        }
        return true;
      }
      if (tile != Moderator) return false;
    }
    return false;
  }

  int Evaluator::countAdjFuelCells(int x, int y, int z) {
    return hasCellInLine(x, y, z, -1, 0, 0)
         + hasCellInLine(x, y, z, +1, 0, 0)
         + hasCellInLine(x, y, z, 0, -1, 0)
         + hasCellInLine(x, y, z, 0, +1, 0)
         + hasCellInLine(x, y, z, 0, 0, -1)
         + hasCellInLine(x, y, z, 0, 0, +1);
  }

  int Evaluator::isActiveSafe(int tile, int x, int y, int z) const {
    if (!state->in_bounds(x, y, z))
      return false;
    return (*state)(x, y, z) == tile && isActive(x, y, z) && (tile < Cell || tile == Moderator);
  }

  int Evaluator::countActiveNeighbors(int tile, int x, int y, int z) const {
    return
      + isActiveSafe(tile, x - 1, y, z)
      + isActiveSafe(tile, x + 1, y, z)
      + isActiveSafe(tile, x, y - 1, z)
      + isActiveSafe(tile, x, y + 1, z)
      + isActiveSafe(tile, x, y, z - 1)
      + isActiveSafe(tile, x, y, z + 1);
  }

  bool Evaluator::isTileSafe(int tile, int x, int y, int z) const {
    if (!state->in_bounds(x, y, z))
      return false;
    return (*state)(x, y, z) == tile;
  }

  int Evaluator::countNeighbors(int tile, int x, int y, int z) const {
    return
      + isTileSafe(tile, x - 1, y, z)
      + isTileSafe(tile, x + 1, y, z)
      + isTileSafe(tile, x, y - 1, z)
      + isTileSafe(tile, x, y + 1, z)
      + isTileSafe(tile, x, y, z - 1)
      + isTileSafe(tile, x, y, z + 1);
  }

  int Evaluator::countCasingNeighbors(int x, int y, int z) const {
    return
      + !state->in_bounds(x - 1, y, z)
      + !state->in_bounds(x + 1, y, z)
      + !state->in_bounds(x, y - 1, z)
      + !state->in_bounds(x, y + 1, z)
      + !state->in_bounds(x, y, z - 1)
      + !state->in_bounds(x, y, z + 1);
  }

  void Evaluator::reset(Evaluation &result) {
    result.invalidTiles.clear();
    result.cellsHeatMult = 0;
    result.cellsEnergyMult = 0;
    result.fuelCellMultiplier = 0;
    result.moderatorCellMultiplier = 0;
    result.cooling = 0.0;
    result.breed = 0; // Number of Cells
    isActive.fill(false);
    isModeratorInLine.fill(false);
  }

  void Evaluator::initializeRulesAndCellMetrics(Evaluation &result) {
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          int tile((*this->state)(x, y, z));
          if (tile == Cell) {
            int adjFuelCells(countAdjFuelCells(x, y, z));
            rules(x, y, z) = -1;
            ++result.breed;
            result.cellsHeatMult += (adjFuelCells + 1) * (adjFuelCells + 2) / 2;
            result.cellsEnergyMult += adjFuelCells + 1;
            result.moderatorCellMultiplier += countNeighbors(Moderator, x, y, z) * (adjFuelCells + 1);
          } else {
            if (tile < Cell) {
              rules(x, y, z) = tile;
            } else {
              rules(x, y, z) = -1;
            }
          }
        }
      }
    }
  }

  void Evaluator::applyPrimaryActivationRules(Evaluation &result) {
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          if ((*this->state)(x, y, z) == Moderator) {
            if (!isModeratorInLine(x, y, z)) {
              result.invalidTiles.emplace_back(x, y, z);
            }
          } else switch (rules(x, y, z)) {
            case Redstone:
              isActive(x, y, z) = countNeighbors(Cell, x, y, z);
              break;
            case Lapis:
              isActive(x, y, z) = countNeighbors(Cell, x, y, z) && countCasingNeighbors(x, y, z);
              break;
            case Enderium:
              isActive(x, y, z) = countCasingNeighbors(x, y, z) == 3
                && (!x || x == settings.sizeX - 1)
                && (!y || y == settings.sizeY - 1)
                && (!z || z == settings.sizeZ - 1);
              break;
            case Cryotheum:
              isActive(x, y, z) = countNeighbors(Cell, x, y, z) >= 2 && countActiveNeighbors(Moderator, x, y, z);
              break;
            case Manganese:
              isActive(x, y, z) = countNeighbors(Cell, x, y, z) >= 2;
              break;
            default:
              break;
          }
        }
      }
    }
  }

  void Evaluator::applySecondaryActivationRules() {
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          switch (rules(x, y, z)) {
            case Water:
              isActive(x, y, z) = countNeighbors(Cell, x, y, z) || countActiveNeighbors(Moderator, x, y, z);
              break;
            case Quartz:
              isActive(x, y, z) = countActiveNeighbors(Moderator, x, y, z);
              break;
            case Glowstone:
              isActive(x, y, z) = countActiveNeighbors(Moderator, x, y, z) >= 2;
              break;
            case Helium:
              isActive(x, y, z) = countActiveNeighbors(Redstone, x, y, z) && countCasingNeighbors(x, y, z);
              break;
            case Emerald:
              isActive(x, y, z) = countActiveNeighbors(Moderator, x, y, z) && countNeighbors(Cell, x, y, z);
              break;
            case Tin:
              isActive(x, y, z) =
                isActiveSafe(Lapis, x - 1, y, z) &&
                isActiveSafe(Lapis, x + 1, y, z) ||
                isActiveSafe(Lapis, x, y - 1, z) &&
                isActiveSafe(Lapis, x, y + 1, z) ||
                isActiveSafe(Lapis, x, y, z - 1) &&
                isActiveSafe(Lapis, x, y, z + 1);
              break;
            case Magnesium:
              isActive(x, y, z) = countActiveNeighbors(Moderator, x, y, z) && countCasingNeighbors(x, y, z);
              break;
            default:
              break;
          }
        }
      }
    }
  }

  void Evaluator::applyTertiaryActivationRules() {
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          switch (rules(x, y, z)) {
            case Copper:
              isActive(x, y, z) = countActiveNeighbors(Glowstone, x, y, z);
              break;
            case Aluminium:
              isActive(x, y, z) = countActiveNeighbors(Quartz, x, y, z) && countActiveNeighbors(Lapis, x, y, z);
              break;
            case Boron:
              isActive(x, y, z) = countActiveNeighbors(Quartz, x, y, z) && (countCasingNeighbors(x, y, z) || countActiveNeighbors(Moderator, x, y, z));
              break;
            default:
              break;
          }
        }
      }
    }
  }

  void Evaluator::accumulateCoolingAndInvalidTiles(Evaluation &result) const {
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          int tile((*this->state)(x, y, z));
          if (tile < Cell) {
            if (isActive(x, y, z))
              result.cooling += settings.coolingRates[tile];
            else
              result.invalidTiles.emplace_back(x, y, z);
          }
        }
      }
    }
  }

  void Evaluator::run(const xt::xtensor<int, 3> &currentState, Evaluation &result) {
    this->state = &currentState;
    reset(result);
    initializeRulesAndCellMetrics(result);
    applyPrimaryActivationRules(result);
    applySecondaryActivationRules();
    applyTertiaryActivationRules();
    accumulateCoolingAndInvalidTiles(result);

    result.compute(settings);
  }
}
