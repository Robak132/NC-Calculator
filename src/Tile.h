#ifndef TILE_H
#define TILE_H
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace Fission {
  class Tile {
    static std::vector<json, std::less<>> tiles;
    static std::map<std::string, int, std::less<>> lookupTiles;
  public:
    static void loadTiles();
    static int getTileID(const std::string &name);
  };
} // namespace Fission

#endif // TILE_H
