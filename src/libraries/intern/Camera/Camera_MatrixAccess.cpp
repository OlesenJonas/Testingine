#include "Camera.hpp"

[[nodiscard]] glm::mat4 Camera::getView() const
{
    return matrices[0];
}
[[nodiscard]] glm::mat4 Camera::getInvView() const
{
    return matrices[1];
}
[[nodiscard]] glm::mat4 Camera::getProj() const
{
    return matrices[2];
}
[[nodiscard]] glm::mat4 Camera::getInvProj() const
{
    return matrices[3];
}
[[nodiscard]] glm::mat4 Camera::getProjView() const
{
    return matrices[4];
}
[[nodiscard]] glm::mat4 Camera::getInvProjView() const
{
    return matrices[5];
}

void Camera::setView(const glm::mat4& mat)
{
    matrices[0] = mat;
}
void Camera::setInvView(const glm::mat4& mat)
{
    matrices[1] = mat;
}
void Camera::setProj(const glm::mat4& mat)
{
    matrices[2] = mat;
}
void Camera::setInvProj(const glm::mat4& mat)
{
    matrices[3] = mat;
}
void Camera::setProjView(const glm::mat4& mat)
{
    matrices[4] = mat;
}
void Camera::setInvProjView(const glm::mat4& mat)
{
    matrices[5] = mat;
}