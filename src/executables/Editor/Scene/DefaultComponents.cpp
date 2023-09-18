#include "DefaultComponents.hpp"

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

void Transform::calculateLocalTransformMatrix()
{
    localTransform =               //
        glm::translate(position) * //
        glm::toMat4(orientation) * //
        glm::scale(scale);
}