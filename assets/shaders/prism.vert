#version 330 core

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
uniform float u_bands[28];
uniform vec4 u_audio;
uniform float u_energy;
uniform float u_reactivity;
uniform float u_stretch;
uniform float u_uniformity;  // 0 = varied sizes and shapes, 1 = all alike
uniform float u_spread;      // palette distance across a ribbon
uniform float u_shimmer;     // palette drift with age
uniform float u_paletteScale;  // multiplies every route's palette length
uniform float u_paletteOffset; // added to every route's palette offset
uniform float u_unify;         // 0 = each route's own palette, 1 = all share one
uniform float u_living;        // the patch's branching tissue uses the same skin
uniform float u_treble;
uniform float u_time;
uniform bool u_webcamEnabled;

out vec3 v_pos;
out vec3 v_normal;
out vec3 v_col;
flat out float v_dither;
flat out float v_glint;
out vec2 v_facetUV;
flat out float v_mirrorFace;

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
  bool living = u_living > .5;
  vec4 seed = living ? vec4((i_meta.y+.5)/u_routes, i_seed.y, i_seed.z, .5) : i_seed;
  float age = living ? 0. : i_position.w;
  float life = i_velocity.w;
  vec3 h = hash33(seed.xyz * 57.3 + seed.w);
  v_dither = hash33(seed.zxy * 13.1 + 4.7).x;

  // grow in at birth, crumble away at the end
  // (the fade matches FADE in simulate.vert.js)
  float grow = age < 0. ? 0. : smoothstep(0., .9, age) * (1. - smoothstep(life - 1.4, life, age));

  // sizes skew small with the odd chunky block; uniformity pulls every
  // prism's size, proportions and roll toward the same values
  float s = u_size * mix(mix(.45, 1.7, pow(seed.w, 3.)), 1., u_uniformity) * grow;
  float band = u_bands[int(clamp(floor(seed.x * u_routes), 0., 27.))];
  s *= 1. + u_reactivity * (band * .2 + u_energy * .15 + u_audio.w * .5);
  float speed = length(i_velocity.xyz);
  vec2 section = mix(vec2(mix(.35, .7, h.x), mix(.55, 1., h.y)), vec2(.52, .78), u_uniformity);
  vec3 dims = s * vec3((1.6 + u_stretch * speed * .25) * (1. + u_reactivity * (band * .32 + u_audio.w * .65)), section);
  if (living) dims = vec3(i_velocity.w,i_position.w*mix(.65,.25,i_meta.w),i_position.w*mix(1.,1.8,i_meta.w));

  if (u_webcamEnabled && !living) {
    // Thin glass shards with a broad face and a dark edge.
    dims.x = min(dims.x*.53, s*mix(2.2, 3.5, h.y));
    dims.y = s*mix(1.05, 1.9, h.x);
    dims.z = s*mix(.18, .36, h.z);
  }
  v_facetUV = abs(normal.x) > .5 ? position.yz
              : abs(normal.y) > .5 ? position.xz : position.xy;
  v_mirrorFace = living ? 1. : abs(normal.z);

  // frame from the velocity, rolled a little per prism
  vec3 f = speed > 1e-4 ? i_velocity.xyz / speed : vec3(1., 0., 0.);
  vec3 up0 = abs(f.y) > .98 ? vec3(1., 0., 0.) : vec3(0., 1., 0.);
  vec3 r = normalize(cross(f, up0));
  vec3 u = cross(r, f);
  float roll = living ? i_meta.z : (h.z - .5) * .7 * (1. - u_uniformity);
  if (u_webcamEnabled && !living) roll += (h.x - .5)*1.35;
  vec3 u2 = u * cos(roll) + r * sin(roll);
  vec3 r2 = r * cos(roll) - u * sin(roll);
  mat3 B = mat3(f, u2, r2);

  // Small fibres catch travelling glints. Their response stays local, so
  // highs read as fine surface activity across the larger folds.
  float fine = living ? 1. - smoothstep(.08, .25, i_position.w)
                      : 1. - smoothstep(.25, .65, seed.w);
  float phase = dot(i_position.xyz, vec3(.48, .76, -.32)) + u_time*18. + h.y*TAU;
  float flicker = smoothstep(.45, 1., sin(phase));
  v_glint = u_treble*fine*flicker*(living ? 1. : grow);

  vec3 offset = B * (position * dims);
  vec3 wp = i_position.xyz + offset;
  vec3 wn = B * normal;
  if (!living) {
    BodyFrame frame = bodyFrame(i_position.xyz);
    vec3 fibreSway = r*(.16*u_treble*fine*sin(phase + .8))*grow;
    wp = frame.centre + bodyTangent(frame, offset + fibreSway);
    wn = bodyNormal(frame, wn);
  }

  // palette lookup: the route's row in the prism's palette slot
  float route = floor(seed.x * u_routes);
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
  float t = hash33(vec3(route, 1.7, 3.1)).x * 8. + seed.y * u_spread + age * u_shimmer;
  vec3 col = D.a + C.a * pal(t * u_paletteScale * A.a + Bc.a + u_paletteOffset, A.rgb, Bc.rgb, C.rgb, D.rgb);
  col *= max(.18, 1. + u_reactivity * (-.65 + u_energy * .95 + u_audio.w * .9));
  if (living) {
    // A richer feeding trunk grades back into the emitter's exact palette at
    // the branch tips. Lighting and musical modulation remain shared.
    col *= mix(1.35,1.,i_meta.w);
  }
  v_col = clamp(col, 0., 1.) * (.93 + .14 * h.x);

  vec4 vp = u_viewMatrix * vec4(wp, 1.);
  v_pos = vp.xyz;
  v_normal = mat3(u_viewMatrix) * wn;
  gl_Position = u_projectionMatrix * vp;
}
