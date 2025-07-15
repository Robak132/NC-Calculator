#ifndef _FISSION_H_
#define _FISSION_H_
#include <xtensor/xtensor.hpp>
#include "Component.h"

namespace Fission {
  using Coords = std::vector<std::tuple<int, int, int>>;

  enum { GoalPower, GoalBreeder, GoalEfficiency };

  struct Settings {
    int sizeX, sizeY, sizeZ;
    double fuelBasePower, fuelBaseHeat;
    bool ensureHeatNeutral;
    int goal;
    bool symX, symY, symZ;
    bool activeHeatsinkPrime;
    double genMult, heatMult, modFEMult, modHeatMult, FEGenMult;
    std::map<int, Component> components = {{0, Component(0, "Air", "air", -1, 0)}};
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
    xt::xtensor<std::string, 3> rules;
    xt::xtensor<bool, 3> active, isModeratorInLine, visited;
    const xt::xtensor<int, 3> *state = new xt::xtensor<int, 3>;

    const Component *getTileSafe(int x, int y, int z) const;
    bool hasCellInLine(int x, int y, int z, int dx, int dy, int dz);
    int countAdjFuelCells(int x, int y, int z);
    int isActiveSafe(const std::string &tileName, int x, int y, int z) const;
    int countActiveNeighbors(const std::string &tileName, int x, int y, int z) const;
    bool isTileSafe(const std::string &tileName, int x, int y, int z) const;
    int countNeighbors(const std::string &tileName, int x, int y, int z) const;
    int countCasingNeighbors(int x, int y, int z) const;

    int countPassiveHeatsinks() const;

  public:
    Evaluator(const Settings &settings);
    Component getTileFromState(int i, int y, int z) const;
    int componentIdFromName(const std::string &name) {
      for (const auto &[id, component] : settings.components) {
        if (component.getName() == name) {
          return id;
        }
      }
      return -1;
    }
    void run(const xt::xtensor<int, 3> &currentState, Evaluation &result);
  };
} // namespace Fission

#endif
