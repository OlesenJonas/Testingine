#include "DefaultComponents.hpp"

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>

void Transform::calculateLocalTransformMatrix()
{
    localTransform =                                    //
        glm::translate(position) *                      //
        glm::eulerAngleXYZ(angle.x, angle.y, angle.z) * //
        glm::scale(scale);
}