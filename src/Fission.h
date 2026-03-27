#ifndef _FISSION_H_
#define _FISSION_H_
#include <array>
#include <xtensor/xtensor.hpp>

namespace Fission {
  using Coords = std::vector<std::tuple<int, int, int>>;

  enum class Tile : int {
    // Cooler
    Water, Copper, Cryotheum, Enderium, Redstone, Helium, Boron, Lapis,
    Emerald, Quartz, Tin, Aluminium, Magnesium, Manganese, Glowstone,
    // Other
    Cell, Moderator, Air
  };

  enum class Goal : int {
    Power,
    Breeder,
    Efficiency
  };

  constexpr int TileCount = static_cast<int>(Tile::Air);
  constexpr int CoolerCount = static_cast<int>(Tile::Cell);

  struct Settings {
    int sizeX, sizeY, sizeZ;
    double fuelBasePower, fuelBaseHeat;
    std::array<int, TileCount> limit;
    std::array<double, CoolerCount> coolingRates;
    bool ensureHeatNeutral;
    Goal goal;
    bool symX, symY, symZ;
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
    
    void reset(Evaluation &result);
    void initializeRulesAndCellMetrics(Evaluation &result);
    void applyPrimaryActivationRules(Evaluation &result);
    void applySecondaryActivationRules();
    void applyTertiaryActivationRules();
    void accumulateCoolingAndInvalidTiles(Evaluation &result) const;

    int getTileSafe(int x, int y, int z) const;
    bool hasCellInLine(int x, int y, int z, int dx, int dy, int dz);
    int countAdjFuelCells(int x, int y, int z);
    int isActiveSafe(int tile, int x, int y, int z) const;
    int countActiveNeighbors(int tile, int x, int y, int z) const;
    bool isTileSafe(int tile, int x, int y, int z) const;
    int countNeighbors(int tile, int x, int y, int z) const;
    int countCasingNeighbors(int x, int y, int z) const;
  public:
    explicit Evaluator(const Settings &settings);
    void run(const xt::xtensor<int, 3> &currentState, Evaluation &result);
  };
}

#endif
