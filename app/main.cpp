#include "../src/Fission.h"
#include "../src/OptFission.h"

int main(int argc, char *argv[]) {
  Fission::Settings settings;
  settings.sizeX = 7;
  settings.sizeY = 7;
  settings.sizeZ = 7;
  settings.fuelBasePower = 100.0;
  settings.fuelBaseHeat = 100.0;
  for (int i = 0; i < Fission::Cell; ++i) {
    settings.limit[i] = 10; // Example limits
    settings.coolingRates[i] = 1.0; // Example cooling rates
  }
  settings.ensureHeatNeutral = true;
  settings.goal = Fission::GoalPower;
  settings.symX = true;
  settings.symY = true;
  settings.symZ = true;

  // Fission::Opt opt(settings, true);

  // while (true) {
  //   opt.step();
  //   if (opt.needsRedrawBest()) {
  //     const auto &bestSample = opt.getBest();
  //     // Here you would typically render the best sample
  //     // For example, print its power and heat:
  //     std::cout << "Best Power: " << bestSample.value.power << ", Heat: " << bestSample.value.heat << std::endl;
  //   }
  // }
}