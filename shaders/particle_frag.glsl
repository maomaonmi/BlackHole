#version 460 core

in float vLife;
in vec3  vColor;

out vec4 fragColor;

void main() {
    // vLife is endpoint alpha (0 = tail, 1 = head)
    float alpha = clamp(vLife, 0.0, 1.0);

    // Enhance saturation and vibrancy
    vec3 col = vColor;
    float luminance = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(luminance), col, 1.5); // boost saturation

    // Strong glow with highlight near head
    float glow = 4.0 + alpha * 6.0;
    col *= glow;

    // Add specular-like highlight for bright spots
    float highlight = pow(alpha, 3.0) * 2.5;
    col += vec3(highlight);

    // Increase contrast: make brights brighter, darks darker
    col = pow(col, vec3(0.85)); // gamma correction for contrast

    if (alpha < 0.002) discard;

    fragColor = vec4(col, alpha);
}
