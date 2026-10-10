#version 460 core

in float vLife;
in vec3  vColor;
in float vDistToBH;  // distance to BH center

uniform float uColorGradient = 0.0f; // color gradient intensity (0=off, 1=strong)
uniform vec3  uParticleColor;        // base color

out vec4 fragColor;

void main() {
    // vLife is endpoint alpha (0 = tail, 1 = head)
    float alpha = clamp(vLife, 0.0, 1.0);

    // Start with base color
    vec3 col = vColor;

    // Color gradient based on distance to BH center
    // Inner = blue-white (hot), Outer = orange-red (cool)
    if (uColorGradient > 0.01) {
        // Normalize distance: 0.05 (inner) to 0.5 (outer)
        float normalizedDist = clamp((vDistToBH - 0.05) / 0.45, 0.0, 1.0);
        // Inner color: blue-white (hot plasma)
        vec3 innerCol = vec3(0.6, 0.75, 1.0);  // blue-white
        // Outer color: orange-red (cooler)
        vec3 outerCol = vec3(1.0, 0.5, 0.2);   // orange-red
        // Interpolate based on distance
        vec3 gradientCol = mix(innerCol, outerCol, normalizedDist);
        // Blend base color with gradient (not replace)
        col = mix(col * 0.3, gradientCol, uColorGradient * 0.7);
    }

    // Enhance saturation and vibrancy
    float luminance = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(luminance), col, 1.3); // boost saturation

    // Glow effect
    float glow = 3.0 + alpha * 4.0;
    col *= glow;

    // Add highlight near head
    float highlight = pow(alpha, 3.0) * 1.5;
    col += vec3(highlight);

    // Gamma correction for contrast
    col = pow(col, vec3(0.9));

    if (alpha < 0.002) discard;

    fragColor = vec4(col, alpha);
}
