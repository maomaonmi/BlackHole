#version 460 core

layout(location = 0) in vec2 aPos; // fullscreen quad [-1,1]

out vec2 vUV;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vUV = aPos * 0.5 + 0.5; // [0,1]
}
