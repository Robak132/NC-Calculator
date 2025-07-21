#include "../src/Fission.h"
#include "../src/Tile.h"

int main(int argc, char *argv[]) {
  Fission::Settings settings;
  settings.sizeX = 7;
  settings.sizeY = 7;
  settings.sizeZ = 7;
  settings.fuelBasePower = 1000.0;
  settings.fuelBaseHeat = 1000.0;
  settings.limit[Fission::Air] = 1000;
  settings.coolingRates[Fission::Cell] = 1.0;
  settings.ensureHeatNeutral = true;
  settings.goal = Fission::GoalPower;
  settings.symX = true;
  settings.symY = true;
  settings.symZ = true;
  Fission::Tile::loadTiles();
  Fission::Evaluator evaluator(settings);
}