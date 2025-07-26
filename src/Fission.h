#ifndef _FISSION_H_
#define _FISSION_H_
#include <xtensor/xtensor.hpp>

#include "Tile.h"
using json = nlohmann::json;

namespace Fission {
  class Tile;
  using Coords = std::vector<std::tuple<int, int, int>>;

  enum {
    GoalPower,
    GoalBreeder,
    GoalEfficiency
  };

  struct Settings {
    int sizeX, sizeY, sizeZ;
    double fuelBasePower, fuelBaseHeat;
    int limit[60];
    double coolingRates[60];
    bool ensureHeatNeutral;
    int goal;
    bool symX, symY, symZ;
    bool activeHeatsinkPrime;
    double genMult, heatMult, modFEMult, modHeatMult, FEGenMult;
    json data;
  };

  struct Evaluation {
    // Raw
    Coords invalidTiles;
    double cooling;
    int breed, fuelCellMultiplier, moderatorCellMultiplier, cellsHeatMult, cellsEnergyMult;
    // Computed
    double heat, netHeat, dutyCycle, power, avgPower, avgBreed, efficiency;

    void compute(const Settings &settings);

    static double heatMultiplier(double heatPerTick, double coolingPerTick, double heatMult);
  };

  class Evaluator {
    const Settings &settings;
    xt::xtensor<Tile*, 3> rules;
    xt::xtensor<bool, 3> active, moderatorInLine, visited;
    const xt::xtensor<int, 3> *state;

    static void loadTiles(const Settings &settings);

    int getTileSafe(int x, int y, int z) const;
    bool hasCellInLine(int x, int y, int z, int dx, int dy, int dz);
    int countAdjFuelCells(int x, int y, int z);
    int isActiveSafe(const std::string &tile, int x, int y, int z) const;
    int countActiveNeighbors(const std::string &tile, int x, int y, int z) const;
    bool isTileSafe(const std::string &tile, int x, int y, int z) const;
    int countNeighbors(const std::string &tile, int x, int y, int z) const;
    int countCasingNeighbors(int x, int y, int z) const;
  public:
    static std::vector<std::unique_ptr<Tile>> tiles;
    static std::map<std::string, Tile*, std::less<>> lookupTiles;

    static Tile *getTile(const std::string &name);
    static Tile *Cell();
    static Tile *Moderator();
    static Tile *Air();

    Evaluator(const Settings &settings);
    void run(const xt::xtensor<int, 3> &currentState, Evaluation &result);
  };
}

#endif
