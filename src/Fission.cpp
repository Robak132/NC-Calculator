#include "Fission.h"
#include <cmath>
#include <xtensor/xview.hpp>

#include "Tile.h"

namespace Fission {
  std::vector<std::unique_ptr<Tile>> Evaluator::tiles = {
    std::make_unique<Tile>(0, "Air", "Air", "air", 0),
    std::make_unique<Tile>(1, "Fuel Cell", "Fuel Cell", "cell", 1),
    std::make_unique<Tile>(2, "Moderator", "Moderator", "moderator", 0),
  };
  std::map<std::string, Tile*, std::less<>> Evaluator::lookupTiles = {
    {"Air", tiles[0].get()},
    {"Fuel Cell", tiles[1].get()},
    {"Moderator", tiles[2].get()}
  };

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
      settings(settings), rules(xt::empty<Tile*>({settings.sizeX, settings.sizeY, settings.sizeZ})),
      active(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})),
      moderatorInLine(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})),
      visited(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})), state(nullptr) {
    loadTiles(settings);
  }

  void Evaluator::loadTiles(const Settings &settings) {
    auto data = settings.data;
    tiles.reserve(data.size());

    for (auto it = data.begin(); it != data.end(); ++it) {
      auto tile = std::make_unique<Tile>(it - data.begin(), (*it)["name"].get<std::string>(), (*it)["title"].get<std::string>(), (*it)["type"].get<std::string>(), it->contains("cooling_rate") ? (*it)["cooling_rate"].get<int>() : 0);
      lookupTiles[(*it)["title"]] = tile.get();
      tiles.push_back(std::move(tile));
    }
  }

  Tile *Evaluator::getTile(const std::string &name) { return lookupTiles[name]; }

  Tile *Evaluator::Cell() { return getTile("Fuel Cell"); }

  Tile *Evaluator::Moderator() { return getTile("Moderator"); }

  Tile *Evaluator::Air() { return getTile("Air"); }

  int Evaluator::getTileSafe(const int x, const int y, int const z) const {
    if (!state->in_bounds(x, y, z)) {
      return -1;
    }
    return (*state)(x, y, z);
  }

  bool Evaluator::hasCellInLine(int x, int y, int z, const int dx, const int dy, const int dz) {
    for (int n{}; n <= 4; ++n) {
      x += dx;
      y += dy;
      z += dz;
      const int tile = getTileSafe(x, y, z);
      if (tile == Cell()->getId()) {
        for (int i{}; i < n; ++i) {
          x -= dx;
          y -= dy;
          z -= dz;
          moderatorInLine(x, y, z) = true;
        }
        if (getTileSafe(x, y, z) == Moderator()->getId()) {
          active(x, y, z) = true;
        }
        return true;
      }
      if (tile != Moderator()->getId()) {
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

  int Evaluator::isActiveSafe(const std::string &tile, const int x, const int y, int const z) const {
    if (!state->in_bounds(x, y, z)) {
      return false;
    }
    const Tile *tilePtr = getTile(tile);
    return (*state)(x, y, z) == tilePtr->getId() && active(x, y, z) && (tilePtr->getType() == Passive || tilePtr->getType() == TileType::Moderator || settings.activeHeatsinkPrime);
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

  bool Evaluator::isTileSafe(const std::string &tile, const int x, const int y, int const z) const {
    if (!state->in_bounds(x, y, z)) {
      return false;
    }
    return (*state)(x, y, z) == getTile(tile)->getId();
  }

  int Evaluator::countNeighbors(const std::string &tile, const int x, const int y, int const z) const {
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
    std::cout << "Running evaluation..." << std::endl;
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
          std::cout << "debug: (" << x << ", " << y << ", " << z << ") = " << (*this->state)(x, y, z) << std::endl;
          if (Tile *tile = tiles.at((*this->state)(x, y, z)).get(); tile == Cell()) {
            const int adjFuelCells = countAdjFuelCells(x, y, z);
            rules(x, y, z) = nullptr;
            ++result.breed;
            result.cellsHeatMult += (adjFuelCells + 1) * (adjFuelCells + 2) / 2;
            result.cellsEnergyMult += adjFuelCells + 1;
            result.moderatorCellMultiplier += countActiveNeighbors("Moderator", x, y, z) * (adjFuelCells + 1);
          } else {
            if (tile->getType() == Passive || tile->getType() == Active) {
              rules(x, y, z) = tile;
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
          if ((*this->state)(x, y, z) == Moderator()->getId()) {
            if (!moderatorInLine(x, y, z)) {
              result.invalidTiles.emplace_back(x, y, z);
            }
          } else {
            if (std::string type = rules(x, y, z)->getName(); type == "Redstone") {
              active(x, y, z) = countNeighbors("Fuel Cell", x, y, z);
            } else if (type == "Lapis") {
              active(x, y, z) = countNeighbors("Fuel Cell", x, y, z) && countCasingNeighbors(x, y, z);
            } else if (type == "Enderium") {
              active(x, y, z) =
                  countCasingNeighbors(x, y, z) == 3 && (!x || x == settings.sizeX - 1) && (!y || y == settings.sizeY - 1) && (!z || z == settings.sizeZ - 1);
            } else if (type == "Cryotheum") {
              active(x, y, z) = countNeighbors("Fuel Cell", x, y, z) >= 2;
            } else if (type == "Manganese") {
              active(x, y, z) = countNeighbors("Fuel Cell", x, y, z) >= 2;
            }
          }
        }
      }
    }

    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          if (std::string type = rules(x, y, z)->getName(); type == "Water") {
            active(x, y, z) = countNeighbors("Fuel Cell", x, y, z) || countActiveNeighbors("Moderator", x, y, z);
          } else if (type == "Quartz") {
            active(x, y, z) = countActiveNeighbors("Moderator", x, y, z);
          } else if (type == "Glowstone") {
            active(x, y, z) = countActiveNeighbors("Moderator", x, y, z) >= 2;
          } else if (type == "Helium") {
            active(x, y, z) = countActiveNeighbors("Redstone", x, y, z) == 1 && countCasingNeighbors(x, y, z);
          } else if (type == "Emerald") {
            active(x, y, z) = countActiveNeighbors("Moderator", x, y, z) && countNeighbors("Fuel Cell", x, y, z);
          } else if (type == "Tin") {
            active(x, y, z) = isActiveSafe("Lapis", x - 1, y, z) && isActiveSafe("Lapis", x + 1, y, z) ||
                              isActiveSafe("Lapis", x, y - 1, z) && isActiveSafe("Lapis", x, y + 1, z) ||
                              isActiveSafe("Lapis", x, y, z - 1) && isActiveSafe("Lapis", x, y, z + 1);
          } else if (type == "Magnesium") {
            active(x, y, z) = countActiveNeighbors("Moderator", x, y, z) && countCasingNeighbors(x, y, z);
          } else if (type == "End Stone") {
            active(x, y, z) = countActiveNeighbors("Enderium", x, y, z);
          } else if (type == "Arsenic") {
            active(x, y, z) = countActiveNeighbors("Moderator", x, y, z) >= 3;
          }
        }
      }
    }

    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          std::string type = rules(x, y, z)->getName();
          if (type == "Gold") {
            active(x, y, z) = countActiveNeighbors("Water", x, y, z) && countActiveNeighbors("Redstone", x, y, z);
          } else if (type == "Diamond") {
            active(x, y, z) = countActiveNeighbors("Water", x, y, z) && countActiveNeighbors("Quartz", x, y, z);
          } else if (type == "Copper") {
            active(x, y, z) = countActiveNeighbors("Glowstone", x, y, z);
          } else if (type == "Prismarine") {
            active(x, y, z) = countActiveNeighbors("Water", x, y, z);
          } else if (type == "Obsidian") {
            active(x, y, z) = isActiveSafe("Glowstone", x - 1, y, z) && isActiveSafe("Glowstone", x + 1, y, z) ||
                              isActiveSafe("Glowstone", x, y - 1, z) && isActiveSafe("Glowstone", x, y + 1, z) ||
                              isActiveSafe("Glowstone", x, y, z - 1) && isActiveSafe("Glowstone", x, y, z + 1);
          } else if (type == "Aluminium") {
            active(x, y, z) = countActiveNeighbors("Quartz", x, y, z) && countActiveNeighbors("Lapis", x, y, z);
          } else if (type == "Boron") {
            active(x, y, z) = countActiveNeighbors("Quartz", x, y, z) && (countCasingNeighbors(x, y, z) || countActiveNeighbors("Moderator", x, y, z));
          } else if (type == "Silver") {
            active(x, y, z) = countActiveNeighbors("Glowstone", x, y, z) >= 2 && countActiveNeighbors("Tin", x, y, z);
          }
        }
      }
    }

    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          std::string type = rules(x, y, z)->getName();
          if (type == "Iron") {
            active(x, y, z) = countActiveNeighbors("Gold", x, y, z);
          } else if (type == "Fluorite") {
            active(x, y, z) = countActiveNeighbors("Prismarine", x, y, z) && countActiveNeighbors("Gold", x, y, z);
          } else if (type == "Nether Brick") {
            active(x, y, z) = countActiveNeighbors("Obsidian", x, y, z);
          }
        }
      }
    }

    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          std::string type = rules(x, y, z)->getName();
          if (type == "Lead") {
            active(x, y, z) = countActiveNeighbors("Iron", x, y, z);
          } else if (type == "Purpur") {
            active(x, y, z) = countActiveNeighbors("Iron", x, y, z) && countCasingNeighbors(x, y, z);
          }
        }
      }
    }

    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          int tile((*this->state)(x, y, z));
          if (tile < Cell()->getId()) {
            if (std::string type = rules(x, y, z)->getName(); type == "Slime") {
              active(x, y, z) = countActiveNeighbors("Water", x, y, z) && countActiveNeighbors("Lead", x, y, z);
            } else if (type == "Lithium") {
              active(x, y, z) = isActiveSafe("Lead", x - 1, y, z) && isActiveSafe("Lead", x + 1, y, z) ||
                                  isActiveSafe("Lead", x, y - 1, z) && isActiveSafe("Lead", x, y + 1, z) ||
                                  isActiveSafe("Lead", x, y, z - 1) && isActiveSafe("Lead", x, y, z + 1);
            } else if (type == "Nitrogen") {
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
