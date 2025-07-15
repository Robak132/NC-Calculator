#ifndef COMPONENT_H
#define COMPONENT_H

#include <string>

class Component {
  int id = 0;
  std::string name;
  std::string type;
  int limit = -1;
  double coolingRate = 0.0;

public:
  Component(int id, const std::string &name, const std::string &type, int limit, double coolingRate);
  Component(const Component& other);
  int getId() const { return id; }
  std::string getType() const { return type; }
  std::string getName() const { return name; }
  int getLimit() const { return limit; }
  void setLimit(const int i) { limit = i; }
  double getCoolingRate() const { return coolingRate; }
  bool isCell() const { return type == "cell"; }
  bool isModerator() const { return type == "moderator"; }
  bool isActive() const { return type == "active"; }
  bool isPassive() const { return type == "passive"; }
  bool isHeatsink() const { return type == "active" || type == "passive"; }
  bool operator==(const Component& other) const;
};

#endif // COMPONENT_H
