#version 460 core

layout(location = 0) in vec2  aPos;   // screen UV [0,1]
layout(location = 1) in float aLife;   // life [0,1]
layout(location = 2) in float aSpeed;  // speed factor

uniform vec3  uWindColor;
uniform float uWindSize = 2.0;

out float vLife;
out float vSpeed;
out vec3  vColor;

void main() {
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
    gl_PointSize = uWindSize * (0.5 + aSpeed * 0.5);
    vLife  = aLife;
    vSpeed = aSpeed;
    vColor = uWindColor;
}
