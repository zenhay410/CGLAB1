#version 450
layout(location=0) in vec3 position;
layout(location=0) out vec3 vertexColor;
layout(set=0, binding=0, std140) uniform ObjectData {
    mat4 mvp;
    vec4 tint;
    ivec4 options;
} object;
void main() {
    gl_Position = object.mvp * vec4(position, 1.0);
    // Map local coordinates [-0.5,0.5] to RGB [0,1].
    vec3 base = object.options.x != 0 ? position + vec3(0.5) : vec3(1.0);
    vertexColor = base * object.tint.rgb;
}
