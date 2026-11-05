#version 330 core


// Depth of field over the finished composite: a gather along a golden-angle
// spiral, after Dennis Gustafsson's single-pass bokeh. A sample counts only
// if its own blur disc reaches this pixel, so soft foreground prisms spill
// over sharp ones behind them, but sharp ones don't smear over the soft.

uniform sampler2D b_composite;
uniform sampler2D b_scene; // linear view depth in alpha; 0 = background
uniform vec2 u_resolution;
uniform float u_pixelScale;
uniform float u_dpr;
uniform float u_focus;    // view distance that's sharp
uniform float u_aperture; // blur, as a share of max, per unit of 1 - focus / depth
uniform float u_maxBlur;  // largest blur radius, css px

out vec4 colour;

const int SAMPLES = 64;
const float GOLDEN = 2.39996323;
const float FAR = 1e4;

float depthAt(vec2 uv) {
  float a = texture(b_scene, uv).a;
  return a > 0. ? a : FAR;
}

// blur radius in device px: grows with 1 / depth away from the focal plane,
// as a lens's does
float blur(float d) {
  return clamp(abs(1. - u_focus / d) * u_aperture, 0., 1.) * u_maxBlur * u_dpr;
}

void main() {
  vec2 px = 1. / u_resolution;
  vec2 uv = (gl_FragCoord.xy * u_pixelScale) * px;
  float d = depthAt(uv);
  float size = blur(d);
  vec3 col = texture(b_composite, uv).rgb;
  float tot = 1.;
  float R = u_maxBlur * u_dpr;
  float soft = max(.5, R / sqrt(float(SAMPLES)));
  for (int i = 0; i < SAMPLES; i++) {
    float r = R * sqrt((float(i) + .5) / float(SAMPLES));
    float a = float(i) * GOLDEN;
    vec2 suv = uv + vec2(cos(a), sin(a)) * r * px;
    float sd = depthAt(suv);
    float ss = blur(sd);
    // what's behind this pixel only bleeds over it as far as this pixel blurs
    if (sd > d) ss = min(ss, size * 2.);
    // samples whose disc doesn't reach here add the running average: no change
    float m = smoothstep(r - soft, r + soft, ss);
    col += mix(col / tot, texture(b_composite, suv).rgb, m);
    tot += 1.;
  }
  colour = vec4(col / tot, 1.);
}
