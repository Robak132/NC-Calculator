#include "Tile.h"
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

std::vector<json, std::less<>> Fission::Tile::tiles;
std::map<std::string, int, std::less<>> Fission::Tile::lookupTiles;

void Fission::Tile::loadTiles() {
  std::ifstream f("../data/components.json");
  json data = json::parse(f);
  for (int i{};i<data.size();i++) {
    auto data_ptr = data[i];
    tiles.push_back(data_ptr);
    lookupTiles[data_ptr["title"]] = data_ptr;
  }
}
int Fission::Tile::getTileID(const std::string &name) { return lookupTiles[name]; }
