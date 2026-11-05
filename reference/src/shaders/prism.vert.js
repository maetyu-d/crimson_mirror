export default /* glsl */ `#version 300 es
precision highp float;

// One instanced box per simulated prism, stretched along its velocity.
// Local box space: x = along the flow, y = up, z = across.

in vec3 position;
in vec3 normal;
in vec4 i_position; // xyz, age
in vec4 i_velocity; // xyz, lifespan
in vec4 i_meta;     // palette slot, generation
in vec4 i_seed;

uniform mat4 u_viewMatrix;
uniform mat4 u_projectionMatrix;
uniform sampler2D u_palette; // 4 x (2 * routes): rgb = a, b, c, d; alpha = length, offset, strength, ambience
uniform float u_routes;
uniform float u_size;
uniform float u_stretch;
uniform float u_uniformity;  // 0 = varied sizes and shapes, 1 = all alike
uniform float u_spread;      // palette distance across a ribbon
uniform float u_shimmer;     // palette drift with age
uniform float u_paletteScale;  // multiplies every route's palette length
uniform float u_paletteOffset; // added to every route's palette offset
uniform float u_unify;         // 0 = each route's own palette, 1 = all share one

out vec3 v_pos;
out vec3 v_normal;
out vec3 v_col;
flat out float v_dither;

const float TAU = 6.28318530718;

vec3 hash33(vec3 p3) {
  p3 = fract(p3 * vec3(.1031, .1030, .0973));
  p3 += dot(p3, p3.yxz + 33.33);
  return fract((p3.xxy + p3.yxx) * p3.zyx);
}

vec3 pal(float t, vec3 a, vec3 b, vec3 c, vec3 d) {
  return a + b * cos(TAU * (c * t + d));
}

void main() {
  float age = i_position.w;
  float life = i_velocity.w;
  vec3 h = hash33(i_seed.xyz * 57.3 + i_seed.w);
  v_dither = hash33(i_seed.zxy * 13.1 + 4.7).x;

  // grow in at birth, crumble away at the end
  // (the fade matches FADE in simulate.vert.js)
  float grow = age < 0. ? 0. : smoothstep(0., .9, age) * (1. - smoothstep(life - 1.4, life, age));

  // sizes skew small with the odd chunky block; uniformity pulls every
  // prism's size, proportions and roll toward the same values
  float s = u_size * mix(mix(.45, 1.7, pow(i_seed.w, 3.)), 1., u_uniformity) * grow;
  float speed = length(i_velocity.xyz);
  vec2 section = mix(vec2(mix(.35, .7, h.x), mix(.55, 1., h.y)), vec2(.52, .78), u_uniformity);
  vec3 dims = s * vec3(1.6 + u_stretch * speed * .25, section);

  // frame from the velocity, rolled a little per prism
  vec3 f = speed > 1e-4 ? i_velocity.xyz / speed : vec3(1., 0., 0.);
  vec3 up0 = abs(f.y) > .98 ? vec3(1., 0., 0.) : vec3(0., 1., 0.);
  vec3 r = normalize(cross(f, up0));
  vec3 u = cross(r, f);
  float roll = (h.z - .5) * .7 * (1. - u_uniformity);
  vec3 u2 = u * cos(roll) + r * sin(roll);
  vec3 r2 = r * cos(roll) - u * sin(roll);
  mat3 B = mat3(f, u2, r2);

  vec3 wp = i_position.xyz + B * (position * dims);
  vec3 wn = B * normal;

  // palette lookup: the route's row in the prism's palette slot
  float route = floor(i_seed.x * u_routes);
  int row = int(route + i_meta.x * u_routes);
  // unify blends every route toward the slot's first route, so all ribbons
  // can share one palette
  int row0 = int(i_meta.x * u_routes);
  vec4 A = mix(texelFetch(u_palette, ivec2(0, row), 0), texelFetch(u_palette, ivec2(0, row0), 0), u_unify);
  vec4 Bc = mix(texelFetch(u_palette, ivec2(1, row), 0), texelFetch(u_palette, ivec2(1, row0), 0), u_unify);
  vec4 C = mix(texelFetch(u_palette, ivec2(2, row), 0), texelFetch(u_palette, ivec2(2, row0), 0), u_unify);
  vec4 D = mix(texelFetch(u_palette, ivec2(3, row), 0), texelFetch(u_palette, ivec2(3, row0), 0), u_unify);
  // the palette input: the route's own phase, place across the ribbon and
  // age; scale multiplies all of it, so 0 gives each palette a single colour
  float t = hash33(vec3(route, 1.7, 3.1)).x * 8. + i_seed.y * u_spread + age * u_shimmer;
  vec3 col = D.a + C.a * pal(t * u_paletteScale * A.a + Bc.a + u_paletteOffset, A.rgb, Bc.rgb, C.rgb, D.rgb);
  v_col = clamp(col, 0., 1.) * (.93 + .14 * h.x);

  vec4 vp = u_viewMatrix * vec4(wp, 1.);
  v_pos = vp.xyz;
  v_normal = mat3(u_viewMatrix) * wn;
  gl_Position = u_projectionMatrix * vp;
}
`;
