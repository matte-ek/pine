#include "Transform.hpp"

Pine::Transform::Transform(Pine::Entity* parent) :
      IComponent(parent, ComponentType::Transform)
{
}
