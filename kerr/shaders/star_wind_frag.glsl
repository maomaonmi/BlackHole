#version 460 core

in float vLife;
in float vSpeed;
in vec3  vColor;

out vec4 fragColor;

void main() {
    float alpha = clamp(vLife, 0.0, 1.0);

    // Circular soft point sprite
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    if (dist > 0.5) discard;
    float softEdge = 1.0 - smoothstep(0.2, 0.5, dist);

    vec3 col = vColor;
    // Brighter near star surface (high life), dimmer far away
    float glow = 2.0 + alpha * 3.0 * vSpeed;
    col *= glow;

    alpha *= softEdge;
    if (alpha < 0.002) discard;

    fragColor = vec4(col, alpha);
}
