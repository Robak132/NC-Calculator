#ifndef _FISSION_H_
#define _FISSION_H_
#include <xtensor/xtensor.hpp>

namespace Fission {
  using Coords = std::vector<std::tuple<int, int, int>>;

  enum {
    // Cooler
    Water, Redstone, Helium, Enderium, Cryotheum, Nitrogen, Quartz, Gold,
    Glowstone, Lapis, Diamond, Iron, Emerald, Copper, Tin, Magnesium,
    Manganese, EndStone, Arsenic, Prismarine, Obsidian, Aluminium,
    Boron, Silver, Fluorite, NetherBrick, Lead, Purpur, Slime, Lithium, Active,
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
    xt::xtensor<int, 3> rules;
    xt::xtensor<bool, 3> isActive, isModeratorInLine, visited;
    const xt::xtensor<int, 3> *state;
    
    int getTileSafe(int x, int y, int z) const;
    bool hasCellInLine(int x, int y, int z, int dx, int dy, int dz);
    int countAdjFuelCells(int x, int y, int z);
    int isActiveSafe(int tile, int x, int y, int z) const;
    int countActiveNeighbors(int tile, int x, int y, int z) const;
    bool isTileSafe(int tile, int x, int y, int z) const;
    int countNeighbors(int tile, int x, int y, int z) const;
    int countCasingNeighbors(int x, int y, int z) const;
  public:
    Evaluator(const Settings &settings);
    void run(const xt::xtensor<int, 3> &currentState, Evaluation &result);
  };
}

#endif
