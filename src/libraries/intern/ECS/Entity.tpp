#pragma once

#include "ECS.hpp"
#include <concepts>

template <typename C, typename... Args>
    requires std::constructible_from<C, Args...>
C* ECS::Entity::addComponent(Args&&... args)
{
    return ECS::impl()->addComponent<C>(this->id, std::forward<Args>(args)...);
};

template <typename C>
void ECS::Entity::removeComponent()
{
    ECS::impl()->removeComponent<C>(this->id);
};

template <typename C>
C* ECS::Entity::getComponent()
{
    return ECS::impl()->getComponent<C>(this->id);
};