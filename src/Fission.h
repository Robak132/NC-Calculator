#ifndef _FISSION_H_
#define _FISSION_H_
#include <xtensor/xtensor.hpp>

namespace Fission {
  using Coords = std::vector<std::tuple<int, int, int>>;

  enum {
    // Cooler
    Active = 30,
    // Other
    Cell = Active * 2, Moderator, Air
  };

  enum {
    GoalPower,
    GoalBreeder,
    GoalEfficiency
  };

  struct Settings {
    int sizeX, sizeY, sizeZ;
    double fuelBasePower, fuelBaseHeat;
    int limit[Air];
    double coolingRates[Cell];
    bool ensureHeatNeutral;
    int goal;
    bool symX, symY, symZ;
    bool activeHeatsinkPrime;
    double genMult, heatMult, modFEMult, modHeatMult, FEGenMult;
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
    xt::xtensor<int, std::string> rules;
    xt::xtensor<bool, 3> active, moderatorInLine, visited;
    const xt::xtensor<int, 3> *state = new xt::xtensor<int, 3>;
    
    int getTileSafe(int x, int y, int z) const;
    bool hasCellInLine(int x, int y, int z, int dx, int dy, int dz);
    int countAdjFuelCells(int x, int y, int z);
    int isActiveSafe(std::string tile, int x, int y, int z) const;
    int countActiveNeighbors(const std::string &tile, int x, int y, int z) const;
    bool isTileSafe(int tile, int x, int y, int z) const;
    int countNeighbors(int tile, int x, int y, int z) const;
    int countCasingNeighbors(int x, int y, int z) const;
  public:
    Evaluator(const Settings &settings);
    void run(const xt::xtensor<int, 3> &currentState, Evaluation &result);
  };
}

#endif
