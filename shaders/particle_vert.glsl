#version 460 core

layout(location = 0) in vec2  aPos;   // screen UV [0,1]
layout(location = 1) in float aLife; // life [0,1]

uniform vec3  uParticleColor;
uniform float uPointSize = 4.0;

out float vLife;
out vec3  vColor;

void main() {
    // Convert screen UV [0,1] to clip space [-1,1]
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
    gl_PointSize = uPointSize;

    vLife = aLife;
    vColor = uParticleColor;
}
