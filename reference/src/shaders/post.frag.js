export default /* glsl */ `#version 300 es
precision highp float;
// samplers default to lowp, and some drivers (Mesa) really do read them back
// at half precision: depth then comes back in steps the outline test reads as
// edges, hatching faces seen at a glancing angle
precision highp sampler2D;

// Manifold's composite: geometry outlines, screen-space AO and fog into the
// background, here an IQ palette gradient as in Euphonic.

uniform sampler2D b_scene;
uniform sampler2D b_normal;
uniform float u_tanFov; // tan(fov / 2), for rebuilding view positions
uniform vec2 u_resolution;
uniform float u_time;
uniform float u_dpr;
uniform float u_lines;    // outline strength
uniform float u_ao;       // occlusion strength
uniform float u_aoRadius; // world units
uniform float u_fog;
uniform float u_fogStart;
uniform float u_dark; // 0 = light background, 1 = dark (eased)
uniform vec3 u_bgA;
uniform vec3 u_bgB;
uniform vec3 u_bgC;
uniform vec3 u_bgD;
uniform vec4 u_bgP; // length, offset, strength, ambience

out vec4 colour;

const float FAR = 1e4;
const float TAU = 6.28318530718;
const vec3 LUMA = vec3(.299, .587, .114);
const float BG_GRADIENT = .4; // how much of the background palette's gradient shows

float D(vec4 s) { return s.a > 0. ? s.a : FAR; }

vec3 viewPos(vec2 px, float d) {
  vec2 ndc = px / u_resolution * 2. - 1.;
  return vec3(ndc * vec2(u_tanFov * u_resolution.x / u_resolution.y, u_tanFov) * d, -d);
}

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 pal(float t, vec3 a, vec3 b, vec3 c, vec3 d) {
  return a + b * cos(TAU * (c * t + d));
}

vec3 sky(vec2 px) {
  vec2 uv = (px - .5 * u_resolution) / min(u_resolution.x, u_resolution.y);
  vec3 c = u_bgP.w + u_bgP.z * pal(uv.y * u_bgP.x + u_bgP.y, u_bgA, u_bgB, u_bgC, u_bgD);
  // keep the gradient subtle: 40% of its swing around the centre colour
  vec3 mid = u_bgP.w + u_bgP.z * pal(u_bgP.y, u_bgA, u_bgB, u_bgC, u_bgD);
  c = clamp(mix(mid, c, BG_GRADIENT), 0., 1.);
  // light: lift it toward a pale wash so the prisms read against it
  vec3 day = mix(c, vec3(dot(c, LUMA)) * .2 + .8, .55);
  // dark: keep the palette's hue as a deep tint, with a faint coloured glow
  vec3 night = c * .12 + .012;
  float glow = exp(-length(uv) * 2.5) * BG_GRADIENT;
  return mix(day + glow * .08, night + c * glow * .12, u_dark);
}

vec3 shade(vec2 px, vec3 bg) {
  ivec2 ip = ivec2(px);
  vec4 s = texelFetch(b_scene, ip, 0);
  float d = D(s);
  bool geom = d < FAR;

  // --- outlines from geometry, not colour (silhouette / crease / step)
  int lw = int(max(1., floor(u_dpr * .85 + .5)));
  vec3 P = viewPos(px, d);
  vec4 nc = texelFetch(b_normal, ip, 0);
  vec3 n = nc.xyz;
  float fa = geom ? nc.a : 1.; // near fade: 1 = solid (see prism.frag.js)
  float pixelSize = 2. * u_tanFov * d / u_resolution.y * float(lw);
  float edge = 0.;
  float dmin = d;
  vec3 ncol = s.rgb;
  for (int k = 0; k < 8; k++) {
    ivec2 o = ivec2(k == 0 || k == 1 || k == 7 ? 1 : k == 3 || k == 4 || k == 5 ? -1 : 0,
                    k == 1 || k == 2 || k == 3 ? 1 : k == 5 || k == 6 || k == 7 ? -1 : 0) * lw;
    ivec2 iq = clamp(ip + o, ivec2(0), ivec2(u_resolution) - 1);
    vec4 sn = texelFetch(b_scene, iq, 0);
    float dn = D(sn);
    bool ngeom = dn < FAR;
    vec4 nq = texelFetch(b_normal, iq, 0);
    // edges touching a faded prism fade with it, so its dither holes
    // don't light up as outlines
    float w = min(fa, ngeom ? nq.a : 1.);
    if (geom != ngeom) {
      edge = max(edge, w);
    } else if (geom) {
      edge = max(edge, smoothstep(.04, .15, 1. - dot(n, nq.xyz)) * w);
      vec3 Pn = viewPos(vec2(iq) + .5, dn);
      edge = max(edge, smoothstep(.75, 2.5, abs(dot(Pn - P, n)) / pixelSize) * w);
    }
    if (dn < dmin) {
      dmin = dn;
      ncol = sn.rgb;
    }
  }

  // --- screen-space ambient occlusion: the swirls' hollows go dark
  float ao = 1.;
  if (geom) {
    float occ = 0.;
    float rad = u_resolution.y * u_aoRadius / (2. * u_tanFov * d);
    float rot = hash(px) * TAU;
    for (int k = 0; k < 16; k++) {
      float f = (float(k) + .5) / 16.;
      float a = rot + float(k) * 2.39996;
      vec2 o = vec2(cos(a), sin(a)) * rad * sqrt(f);
      float diff = d - D(texture(b_scene, (px + o) / u_resolution));
      occ += smoothstep(.03, u_aoRadius * .5, diff) * (1. - smoothstep(u_aoRadius * 2., u_aoRadius * 5., diff));
    }
    ao = 1. - occ / 16. * .85 * u_ao * fa;
  }

  float fd = geom ? mix(d, dmin, edge) : dmin;
  float fx = max(fd - u_fogStart, 0.) / u_fog;
  float fog = fd >= FAR ? 1. : 1. - exp(-pow(fx, 1.3));

  vec3 col = geom ? s.rgb * ao : bg;

  vec3 tint = bg * (dot(col, LUMA) / max(dot(bg, LUMA), 1e-3));
  col = mix(col, tint, smoothstep(0., .7, fog) * .5);

  float lineFade = 1. - smoothstep(.25, .8, fog);
  col = mix(col, ncol * vec3(.4, .34, .38), edge * .7 * lineFade * u_lines);

  return mix(col, bg, fog);
}

void main() {
  vec2 px = gl_FragCoord.xy;
  vec3 bg = sky(px);
  vec3 col = shade(px, bg);

  vec2 q = (px - .5 * u_resolution) / u_resolution.y;
  col *= 1. - .18 * smoothstep(.5, 1.2, length(q));
  col = mix(vec3(dot(col, LUMA)), col, 1.08);

  col += (hash(px + fract(u_time)) - .5) * .02;

  colour = vec4(col, 1.);
}
`;
