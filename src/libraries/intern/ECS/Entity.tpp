#pragma once

#include "ECS.hpp"
#include <concepts>

template <typename C, typename... Args>
    requires std::constructible_from<C, Args...>
void ECS::Entity::addComponent(Args&&... args)
{
    ecs.addComponent<C>(this, std::forward<Args>(args)...);
};

template <typename C>
void ECS::Entity::removeComponent()
{
    ecs.removeComponent<C>(this);
};

template <typename C>
C* ECS::Entity::getComponent()
{
    return ecs.getComponent<C>(this);
};