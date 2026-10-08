#pragma once
#include <array>
#include <cstdint>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
namespace scene {
inline constexpr std::array<std::array<float,3>,8> vertices{{
    {{-.5f,-.5f,-.5f}}, {{.5f,-.5f,-.5f}}, {{.5f,.5f,-.5f}}, {{-.5f,.5f,-.5f}},
    {{-.5f,-.5f,.5f}}, {{.5f,-.5f,.5f}}, {{.5f,.5f,.5f}}, {{-.5f,.5f,.5f}}
}};
inline constexpr std::array<uint16_t,36> indices{{
    0,2,1, 0,3,2, 4,5,6, 4,6,7, 0,4,7, 0,7,3,
    1,2,6, 1,6,5, 0,1,5, 0,5,4, 3,7,6, 3,6,2
}};
inline glm::mat4 model(glm::vec3 p, glm::vec3 degrees, glm::vec3 s) {
    auto m = glm::translate(glm::mat4(1),p);
    m = glm::rotate(m,glm::radians(degrees.z),{0,0,1});
    m = glm::rotate(m,glm::radians(degrees.y),{0,1,0});
    m = glm::rotate(m,glm::radians(degrees.x),{1,0,0});
    return glm::scale(m,s);
}
inline glm::mat4 projection(bool ortho, float aspect, float fov, float halfHeight) {
    auto p = ortho ? glm::orthoRH_ZO(-halfHeight*aspect,halfHeight*aspect,-halfHeight,halfHeight,.1f,100.f)
                   : glm::perspectiveRH_ZO(glm::radians(fov),aspect,.1f,100.f);
    p[1][1] *= -1;
    return p;
}
inline glm::vec3 path(float t,float radius,float height,float phase) {
    const float a=t+phase;
    return {radius*std::cos(a),height*std::sin(2*a),radius*std::sin(a)};
}
}
