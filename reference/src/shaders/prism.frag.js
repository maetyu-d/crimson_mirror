export default /* glsl */ `#version 300 es
precision highp float;

// Writes lit albedo to rgb and linear view depth to alpha, plus the
// view-space normal to a second target, for the post pass (as in Manifold).

in vec3 v_pos;
in vec3 v_normal;
in vec3 v_col;
flat in float v_dither; // per prism, so overlapping fades don't share holes

uniform float u_gloss;
uniform float u_nearFade; // distance prisms are fully solid from; 0 = off

layout(location = 0) out vec4 colour;
layout(location = 1) out vec4 normalOut;

void main() {
  // near the camera, drop a growing share of pixels (screen-door
  // transparency), so the G-buffer stays opaque for the post pass
  float fade = u_nearFade > 0. ? smoothstep(u_nearFade * .35, u_nearFade, -v_pos.z) : 1.;
  float threshold = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(.06711056, .00583715))) + v_dither);
  if (fade < 1. && fade <= threshold) discard;

  vec3 n = normalize(v_normal);
  vec3 V = normalize(-v_pos);
  vec3 L = normalize(vec3(-.45, .65, .6));
  float dif = max(dot(n, L), 0.);
  float back = max(dot(n, -L), 0.);
  float hemi = n.y * .5 + .5;

  // a soft sheen on the faces that catch the light, like lacquered card
  float spec = pow(max(dot(n, normalize(L + V)), 0.), 48.);
  float rim = pow(1. - max(dot(n, V), 0.), 4.);

  vec3 lit = v_col * (.52 + .5 * dif + .14 * hemi - .1 * back);
  lit += (spec * .45 + rim * .08) * u_gloss;
  colour = vec4(lit, -v_pos.z);
  normalOut = vec4(n, fade); // post.frag.js fades outlines and AO with it
}
`;
