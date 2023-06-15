#include "Camera.hpp"
#include "../Engine/Engine.hpp"
#include "../InputManager/InputManager.hpp"
#include <Engine/Misc/Math.hpp>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

Camera::Camera(float aspect, float nearDistance, float farDistance)
    : cam_near(nearDistance), cam_far(farDistance), aspect(aspect)
{
    // default mode is ORBIT
    const glm::vec3 viewVec = posFromPolar(theta, phi);
    setView(glm::lookAt(position + radius * viewVec, position, glm::vec3(0.f, 1.f, 0.f)));
    setInvView(glm::inverse(getView()));

    glm::mat4 proj = glm::perspective(fov, aspect, nearDistance, farDistance);
    proj[1][1] *= -1;
    setProj(proj);
    setInvProj(glm::inverse(getProj()));

    setProjView(getProj() * getView());
    setInvProjView(glm::inverse(getProjView()));
}

void Camera::update()
{
    auto* window = Engine::get()->getMainWindow()->glfwWindow;
    auto* inputManager = Engine::get()->getInputManager();
    const glm::vec2 mouseDelta = inputManager->getMouseDelta();

    // todo: how much should be part of this, and how much should be inside InputManager?
    if(mode == Mode::ORBIT)
    {
        if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) != GLFW_RELEASE)
        {
            if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            {
                move(glm::vec3(-mouseDelta.x * 0.005f, mouseDelta.y * 0.005f, 0.f));
            }
            else
            {
                rotate(mouseDelta.x, mouseDelta.y);
            }
        }
    }
    else if(mode == Mode::FLY)
    {
        rotate(mouseDelta.x * 0.5f, mouseDelta.y * 0.5f);

        const glm::vec3 cam_move = glm::vec3(
            static_cast<float>(
                int(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) -
                int(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)),
            static_cast<float>(
                int(glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) -
                int(glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)),
            static_cast<float>(
                int(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) -
                int(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)));
        if(cam_move != glm::vec3(0.0f))
        {
            const float dt = inputManager->getRealDeltaTime();
            move(glm::normalize(cam_move) * dt);
        }
    }
    updateViewMatrices();
}

void Camera::updateViewMatrices()
{
    if(mode == Mode::ORBIT)
    {
        const glm::vec3 viewVec = posFromPolar(theta, phi);
        setView(glm::lookAt(position + radius * viewVec, position, glm::vec3(0.f, 1.f, 0.f)));
    }
    else if(mode == Mode::FLY)
    {
        const glm::vec3 viewVec = posFromPolar(theta, phi);
        setView(glm::lookAt(position, position - viewVec, glm::vec3(0.f, 1.f, 0.f)));
    }
    setInvView(glm::inverse(getView()));
    setProjView(getProj() * getView());
    setInvProjView(glm::inverse(getProjView()));
}

void Camera::setPosition(glm::vec3 newPosition)
{
    position = newPosition;
    updateViewMatrices();
}

void Camera::setRotation(float phi, float theta)
{
    this->phi = phi;
    this->theta = theta;
    updateViewMatrices();
}

void Camera::setRadius(float radius)
{
    this->radius = radius;
    updateViewMatrices();
}

// move the camera along its local axis
void Camera::move(glm::vec3 offset)
{
    const glm::vec3 camx = glm::row(getView(), 0);
    const glm::vec3 camy = glm::row(getView(), 1);
    const glm::vec3 camz = glm::row(getView(), 2);
    const glm::vec3 offs = offset.x * camx + offset.y * camy + offset.z * camz;
    position += (mode == Mode::FLY ? flySpeed : 1.0f) * offs;
}

void Camera::rotate(float dx, float dy)
{
    theta = std::min<float>(std::max<float>(theta - dy * 0.01f, 0.1f), M_PI - 0.1f);
    phi = phi - dx * 0.01f;
}

void Camera::setMode(Mode mode)
{
    this->mode = mode;

    const glm::vec3 viewVec = posFromPolar(theta, phi);
    if(mode == Mode::ORBIT)
    {
        // switching to orbit
        // old target becomes oribiting center (position)
        const glm::vec3 oldTarget = position - radius * viewVec;
        position = oldTarget;
    }
    else if(mode == Mode::FLY)
    {
        // switching to fly
        position = position + radius * viewVec;
    }
    updateViewMatrices();
}

void Camera::changeRadius(bool increase)
{
    if(increase)
        radius /= 0.95f;
    else
        radius *= 0.95f;
    updateViewMatrices();
}

void Camera::setFlySpeed(float speed)
{
    flySpeed = speed;
}

const std::array<glm::mat4, 6>& Camera::getMatrices()
{
    return matrices;
}

glm::vec3 Camera::getPosition() const
{
    return position;
}
glm::vec2 Camera::getRotation() const
{
    return {phi, theta};
}
float Camera::getRadius() const
{
    return radius;
}

float Camera::getAspect() const
{
    return aspect;
}

float Camera::getNear() const
{
    return cam_near;
}

float Camera::getFar() const
{
    return cam_far;
}

Camera::Mode Camera::getMode() const
{
    return mode;
}

float Camera::getFlySpeed() const
{
    return flySpeed;
}