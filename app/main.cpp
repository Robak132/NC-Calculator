#include "../src/Fission.h"

int main(int argc, char *argv[]) {
      Fission::Settings settings;
      settings.sizeX = 7;
      settings.sizeY = 7;
      settings.sizeZ = 7;
      settings.fuelBasePower = 100.0;
      settings.fuelBaseHeat = 100.0;
      settings.ensureHeatNeutral = true;
      settings.goal = Fission::GoalPower;
      settings.symX = true;
      settings.symY = true;
      settings.symZ = true;

    Fission::Evaluator evaluation = Fission::Evaluator(settings);

}