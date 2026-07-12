#version 330

// Common fragment header — minimal uniforms for basic rendering

uniform vec3  iResolution;
uniform float iTime;
uniform vec4  iDate;
uniform vec4  iMouse;
uniform vec4  iCurrentCursorColor;
uniform vec4  iPreviousCursorColor;
uniform float iTimeCursorChange;

// Output
#define fragColor gl_FragColor
