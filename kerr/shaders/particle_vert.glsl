#version 460 core

layout(location = 0) in vec2  aPos;   // screen UV [0,1]
layout(location = 1) in float aLife;   // life [0,1]

uniform vec3  uParticleColor;
uniform float uPointSize = 4.0;
uniform vec2  uBHCenter;            // black hole center UV position
uniform float uColorGradient = 0.0f; // color gradient intensity (0=off, 1=strong)

out float vLife;
out vec3  vColor;
out float vDistToBH;  // distance to BH center

void main() {
    // Convert screen UV [0,1] to clip space [-1,1]
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
    gl_PointSize = uPointSize;

    vLife = aLife;
    vColor = uParticleColor;
    
    // Calculate distance to BH center for color gradient
    float dx = aPos.x - uBHCenter.x;
    float dy = aPos.y - uBHCenter.y;
    vDistToBH = sqrt(dx * dx + dy * dy);
}
