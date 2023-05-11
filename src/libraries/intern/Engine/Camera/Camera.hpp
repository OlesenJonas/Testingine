#pragma once

#define _USE_MATH_DEFINES
#include <array>
#include <cmath>
#include <string>

#include <glm/ext.hpp>
#include <glm/glm.hpp>

class Context;

/*
    TODO:
    look sensitivity, scroll sensitivity, pan sensitivity
*/

class Camera
{
  public:
    enum struct Mode
    {
        ORBIT,
        FLY
    };

    Camera() = default;
    explicit Camera(float aspect, float near = 0.1f, float far = 100.0f);

    void update();
    void updateViewMatrices();
    void updateMatrices();

    void move(glm::vec3 offset);
    void rotate(float dx, float dy);
    void setPosition(glm::vec3 newPosition);
    void setRotation(float phi, float theta);
    void setRadius(float radius);
    void changeRadius(bool increase);
    void setMode(Mode mode);
    void setFlySpeed(float speed);

    glm::vec3 getPosition() const;
    glm::vec2 getRotation() const;
    float getRadius() const;
    const std::array<glm::mat4, 6>& getMatrices();

    float getAspect() const;
    float getNear() const;
    float getFar() const;
    Mode getMode() const;
    float getFlySpeed() const;

    [[nodiscard]] glm::mat4 getView() const;
    [[nodiscard]] glm::mat4 getInvView() const;
    [[nodiscard]] glm::mat4 getProj() const;
    [[nodiscard]] glm::mat4 getInvProj() const;
    [[nodiscard]] glm::mat4 getProjView() const;
    [[nodiscard]] glm::mat4 getInvProjView() const;
    [[nodiscard]] glm::mat4 getSkyProj() const;
    void setView(const glm::mat4& mat);
    void setInvView(const glm::mat4& mat);
    void setProj(const glm::mat4& mat);
    void setInvProj(const glm::mat4& mat);
    void setProjView(const glm::mat4& mat);
    void setInvProjView(const glm::mat4& mat);

  private:
    Mode mode = Mode::ORBIT;
    glm::vec3 position{0.0f, 0.0f, 1.0f};
    float phi = 0.0f;
    float theta = glm::pi<float>() * 0.5;
    float radius = 1.0f;

    float aspect = 1.77f;
    float fov = glm::radians(60.0f);
    float cam_near = 0.1f;
    float cam_far = 100.0f;
    float flySpeed = 2.0f;

    //[0] is view, [1] is inverse view, [2] projection, [3] inverse Projection, [4] ProjView, [5] inverse ProjView
    std::array<glm::mat4, 6> matrices;
};