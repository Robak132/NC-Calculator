#ifndef TILE_H
#define TILE_H
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace Fission {
  class Tile {
    int id;
    std::string name;
    std::string fullName;
    std::string type;
    int coolingRate;

  public:
    Tile(const int id, const std::string &name, const std::string &fullName, const std::string &type, const int coolingRate) :
        id(id), name(name), fullName(fullName), type(type), coolingRate(coolingRate) {}
    Tile(const Tile &other) :
        id(other.id), name(other.name), fullName(other.fullName), type(other.type), coolingRate(other.coolingRate) {}

    int getId() const { return id; }
    std::string getName() const { return name; }
    std::string getFullname() const { return fullName; }
    std::string getType() const { return type; }
    bool isPassive() const { return type == "passive"; }
    bool isActive() const { return type == "active"; }
    bool isCell() const { return type == "cell"; }
    bool isModerator() const { return type == "moderator"; }
    int getCoolingRate() const { return coolingRate; }
  };
} // namespace Fission

#endif // TILE_H
