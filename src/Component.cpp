#include "Component.h"
#include <iostream>

Component::Component(const int id, const std::string &name, const std::string &type, const int limit, const double coolingRate = 0) :
    id(id), name(name), type(type), limit(limit), coolingRate(coolingRate) {}

Component::Component(const Component &other) = default;

bool Component::operator==(const Component &other) const {
  return id == other.id && type == other.type && limit == other.limit && coolingRate == other.coolingRate;
}
