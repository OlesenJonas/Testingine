#pragma once

#include "ECS.hpp"
#include <concepts>

template <typename C, typename... Args>
    requires std::constructible_from<C, Args...>
void ECS::Entity::addComponent(Args&&... args)
{
    ECS::get()->addComponent<C>(this, std::forward<Args>(args)...);
};

template <typename C>
void ECS::Entity::removeComponent()
{
    ECS::get()->removeComponent<C>(this);
};

template <typename C>
C& ECS::Entity::getComponent()
{
    return ECS::get()->getComponent<C>(this);
};