#version 330 core

// Transform feedback step: one vertex per prism. Each prism belongs to a
// route; it is born in a small cloud around the route's (slowly wandering)
// origin and carried by a curl-noise current, so prisms born together travel
// together as a ribbon. The whole flow is held inside a container shape,
// described by a signed distance function.

in vec4 a_position; // xyz, age (negative = not born yet)
in vec4 a_velocity; // xyz, lifespan
in vec4 a_meta;     // palette slot, generation
in vec4 a_seed;     // x: route, y/z: place in the ribbon, w: size

uniform float u_time;
uniform vec4 u_audio;
uniform float u_reactivity;
uniform float u_dt;
uniform float u_routes;
uniform float u_slot;       // palette slot newborns adopt
uniform float u_flowScale;  // noise frequency
uniform float u_flowSpeed;
uniform float u_evolve;     // how quickly the current itself changes
uniform float u_drift;      // steady push along +x
uniform float u_steer;      // how quickly prisms turn into the current
uniform float u_spawnRadius;
uniform vec2 u_life;        // min, max lifespan (s)
uniform vec3 u_bounds;      // half extents of the slab: width and height from the viewport, depth from u_shapeSize
uniform float u_shape;      // 0 slab, 1 sphere, 2 torus, 3 capsule, 4 wave sheet
uniform float u_nextShape;
uniform float u_shapeBlend;
uniform float u_shapeSize;
uniform float u_cling;      // 0 = fill the shape, 1 = flow over its surface

out vec4 v_position;
out vec4 v_velocity;
out vec4 v_meta;

const float FADE = 1.4; // seconds a prism takes to shrink away

vec3 hash33(vec3 p3) {
  p3 = fract(p3 * vec3(.1031, .1030, .0973));
  p3 += dot(p3, p3.yxz + 33.33);
  return fract((p3.xxy + p3.yxx) * p3.zyx);
}

// Simplex noise, Ashima Arts / Stefan Gustavson (MIT)
vec4 permute(vec4 x) { return mod(((x * 34.) + 1.) * x, 289.); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - .85373472095314 * r; }
float snoise(vec3 v) {
  const vec2 C = vec2(1. / 6., 1. / 3.);
  const vec4 D = vec4(0., .5, 1., 2.);
  vec3 i = floor(v + dot(v, C.yyy));
  vec3 x0 = v - i + dot(i, C.xxx);
  vec3 g = step(x0.yzx, x0.xyz);
  vec3 l = 1. - g;
  vec3 i1 = min(g.xyz, l.zxy);
  vec3 i2 = max(g.xyz, l.zxy);
  vec3 x1 = x0 - i1 + C.xxx;
  vec3 x2 = x0 - i2 + 2. * C.xxx;
  vec3 x3 = x0 - 1. + 3. * C.xxx;
  i = mod(i, 289.);
  vec4 p = permute(permute(permute(i.z + vec4(0., i1.z, i2.z, 1.)) + i.y + vec4(0., i1.y, i2.y, 1.)) + i.x + vec4(0., i1.x, i2.x, 1.));
  float n_ = 1. / 7.;
  vec3 ns = n_ * D.wyz - D.xzx;
  vec4 j = p - 49. * floor(p * ns.z * ns.z);
  vec4 x_ = floor(j * ns.z);
  vec4 y_ = floor(j - 7. * x_);
  vec4 x = x_ * ns.x + ns.yyyy;
  vec4 y = y_ * ns.x + ns.yyyy;
  vec4 h = 1. - abs(x) - abs(y);
  vec4 b0 = vec4(x.xy, y.xy);
  vec4 b1 = vec4(x.zw, y.zw);
  vec4 s0 = floor(b0) * 2. + 1.;
  vec4 s1 = floor(b1) * 2. + 1.;
  vec4 sh = -step(h, vec4(0.));
  vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
  vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;
  vec3 p0 = vec3(a0.xy, h.x);
  vec3 p1 = vec3(a0.zw, h.y);
  vec3 p2 = vec3(a1.xy, h.z);
  vec3 p3 = vec3(a1.zw, h.w);
  vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
  p0 *= norm.x; p1 *= norm.y; p2 *= norm.z; p3 *= norm.w;
  vec4 m = max(.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.);
  m = m * m;
  return 42. * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

// Exact spatial derivative of the simplex polynomial.
vec3 noiseGradient(vec3 v) {
  const vec2 C = vec2(1. / 6., 1. / 3.);
  const vec4 D = vec4(0., .5, 1., 2.);
  vec3 i = floor(v + dot(v, C.yyy));
  vec3 x0 = v - i + dot(i, C.xxx);
  vec3 g = step(x0.yzx, x0.xyz);
  vec3 l = 1. - g;
  vec3 i1 = min(g.xyz, l.zxy);
  vec3 i2 = max(g.xyz, l.zxy);
  vec3 x1 = x0 - i1 + C.xxx;
  vec3 x2 = x0 - i2 + 2. * C.xxx;
  vec3 x3 = x0 - 1. + 3. * C.xxx;
  i = mod(i, 289.);
  vec4 p = permute(permute(permute(i.z + vec4(0., i1.z, i2.z, 1.)) + i.y + vec4(0., i1.y, i2.y, 1.)) + i.x + vec4(0., i1.x, i2.x, 1.));
  float n_ = 1. / 7.;
  vec3 ns = n_ * D.wyz - D.xzx;
  vec4 j = p - 49. * floor(p * ns.z * ns.z);
  vec4 x_ = floor(j * ns.z);
  vec4 y_ = floor(j - 7. * x_);
  vec4 x = x_ * ns.x + ns.yyyy;
  vec4 y = y_ * ns.x + ns.yyyy;
  vec4 h = 1. - abs(x) - abs(y);
  vec4 b0 = vec4(x.xy, y.xy);
  vec4 b1 = vec4(x.zw, y.zw);
  vec4 s0 = floor(b0) * 2. + 1.;
  vec4 s1 = floor(b1) * 2. + 1.;
  vec4 sh = -step(h, vec4(0.));
  vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
  vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;
  vec3 p0 = vec3(a0.xy, h.x);
  vec3 p1 = vec3(a0.zw, h.y);
  vec3 p2 = vec3(a1.xy, h.z);
  vec3 p3 = vec3(a1.zw, h.w);
  vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
  p0 *= norm.x; p1 *= norm.y; p2 *= norm.z; p3 *= norm.w;
  vec4 m = max(.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.);
  vec4 d = vec4(dot(p0,x0),dot(p1,x1),dot(p2,x2),dot(p3,x3));
  vec4 m2 = m*m, m3 = m2*m, m4 = m2*m2;
  return 42. * (m4.x*p0 + m4.y*p1 + m4.z*p2 + m4.w*p3
    - 8. * (m3.x*d.x*x0 + m3.y*d.y*x1 + m3.z*d.z*x2 + m3.w*d.w*x3));
}

// three decorrelated noises drifting through time: the vector potential
vec3 potential(vec3 p) {
  float t = u_time * u_evolve;
  return vec3(
    snoise(p + vec3(0., 0., t)),
    snoise(p + vec3(31.4, -12.7, -t)),
    snoise(p + vec3(-9.2, 47.1, t * .7))
  );
}

// divergence-free, so the current neither bunches nor thins out prisms
vec3 curl(vec3 p) {
  float t = u_time * u_evolve;
  vec3 a = noiseGradient(p + vec3(0.,0.,t));
  vec3 b = noiseGradient(p + vec3(31.4,-12.7,-t));
  vec3 c = noiseGradient(p + vec3(-9.2,47.1,t*.7));
  return vec3(c.y-b.z, a.z-c.x, b.x-a.y);
}

// --- container shapes (distance functions after Inigo Quilez)
float sdBox(vec3 p, vec3 b) {
  vec3 q = abs(p) - b;
  return length(max(q, 0.)) + min(max(q.x, max(q.y, q.z)), 0.);
}

float shapeDistance(vec3 p, int shape) {
  float S = u_shapeSize;
  if (shape == 1) return length(p) - S; // sphere
  if (shape == 2) { // torus, facing the viewer
    vec2 q = vec2(length(p.xy) - S * .95, p.z);
    return length(q) - S * .4;
  }
  if (shape == 3) { // capsule lying along x
    vec3 q = p;
    q.x -= clamp(q.x, -S * 1.3, S * 1.3);
    return length(q) - S * .6;
  }
  if (shape == 4) { // a rippling sheet across the view
    float wave = S * .4 * sin(p.x * 3. / S + u_time * .1) * cos(p.y * 2. / S - u_time * .07);
    // vertical distance overstates the true one on the slopes; scale it down
    float sheet = (abs(p.z - wave) - S * .35) * .7;
    vec2 q = abs(p.xy) - u_bounds.xy; // trimmed to the view's width and height
    float sides = length(max(q, 0.)) + min(max(q.x, q.y), 0.);
    return max(sheet, sides);
  }
  return sdBox(p, u_bounds); // slab
}

float container(vec3 p) {
  float d = shapeDistance(p, int(u_shape + .5));
  // Skip the second SDF when evolution is disabled or at a keyframe.
  if (u_shapeBlend <= 0.) return d;
  return mix(d, shapeDistance(p, int(u_nextShape + .5)), u_shapeBlend);
}

vec3 containerNormal(vec3 p) {
  const vec2 e = vec2(.05, 0.);
  vec3 g = vec3(
    container(p + e.xyy) - container(p - e.xyy),
    container(p + e.yxy) - container(p - e.yxy),
    container(p + e.yyx) - container(p - e.yyx)
  );
  // flat spots (e.g. deep inside a slab) have no gradient; avoid NaNs there
  return g / max(length(g), 1e-6);
}

// how far inside a wall the current starts to turn back
float shapeMargin(int shape) {
  return shape == 0 ? min(u_bounds.z, u_bounds.y) * .3 : u_shapeSize * (shape == 4 ? .12 : .2);
}

float wallMargin() {
  return mix(shapeMargin(int(u_shape + .5)), shapeMargin(int(u_nextShape + .5)), u_shapeBlend);
}

vec3 current(vec3 p) {
  vec3 v = curl(p * u_flowScale) * u_flowSpeed * .5;
  v.x += u_drift;
  v *= 1. + u_reactivity * u_audio.w * .6;

  float d = container(p);
  vec3 n = containerNormal(p);
  float m = wallMargin();
  // cling: move along the surface rather than through it, and draw in
  // toward it from either side
  v -= n * dot(v, n) * u_cling * .85;
  v -= n * clamp(d / m, -1., 1.) * u_flowSpeed * 1.5 * u_cling;
  // fold back inward near the walls, so ribbons curl over
  v -= n * smoothstep(-m, m * .7, d) * u_flowSpeed * 1.5;
  return v;
}

// each route's origin wanders slowly around its own home inside the shape:
// a point in the shape's bounding box, pulled inside if it lands outside,
// and pulled toward the surface by cling
vec3 shapeBounds(int shape) {
  float S = u_shapeSize;
  return shape == 0 || shape == 4 ? u_bounds
           : shape == 2 ? vec3(S * 1.35, S * 1.35, S * .4)
           : shape == 3 ? vec3(S * 1.9, S * .6, S * .6)
           : vec3(S);
}

vec3 routeOrigin(float r) {
  vec3 h = hash33(vec3(r * 17.13, r * 3.71 + 1.3, 7.7));
  vec3 box = mix(shapeBounds(int(u_shape + .5)), shapeBounds(int(u_nextShape + .5)), u_shapeBlend);
  vec3 home = (h * 2. - 1.) * box * .9;
  home += sin(u_time * (.03 + h.zxy * .06) + h.yzx * 6.2831) * box * .15;
  for (int i = 0; i < 3; i++) {
    float d = container(home);
    vec3 n = containerNormal(home);
    if (d > 0.) home -= n * (d + h.z * wallMargin());
  }
  home -= containerNormal(home) * container(home) * u_cling;
  return home;
}

void main() {
  vec3 p = a_position.xyz;
  float age = a_position.w;
  vec3 vel = a_velocity.xyz;
  float life = a_velocity.w;
  vec4 meta = a_meta;

  bool waiting = age < 0.;
  age += u_dt;

  if (waiting && age < 0.) {
    // not born yet: hold still
  } else if (waiting || age > life) {
    // (re)birth around the route origin; the ribbon offset is fixed per prism
    float gen = meta.y + 1.;
    vec3 rnd = hash33(a_seed.xyz * 91.7 + gen * 1.618);
    // a flattish cloud, so ribbons start out as sheets facing the viewer
    vec3 cloud = (vec3(a_seed.y, a_seed.z, rnd.x) * 2. - 1.) * u_spawnRadius * vec3(1., 1., .35);
    p = routeOrigin(floor(a_seed.x * u_routes)) + cloud;
    vel = current(p);
    age = 0.;
    life = mix(u_life.x, u_life.y, rnd.z);
    meta = vec4(u_slot, gen, 0., 0.);
  } else {
    vel = mix(vel, current(p), 1. - exp(-u_dt * u_steer * (1. + u_reactivity * u_audio.w * 2.)));
    p += vel * u_dt;
    // escaped the shape: cut its life short so it shrinks away (prism.vert.js
    // takes FADE to do it) and is reborn, rather than vanishing on the spot
    if (container(p) > wallMargin() * 1.5) life = min(life, age + FADE);
  }

  v_position = vec4(p, age);
  v_velocity = vec4(vel, life);
  v_meta = meta;
}
