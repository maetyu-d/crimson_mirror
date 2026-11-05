#version 330 core

// Writes lit albedo to rgb and linear view depth to alpha, plus the
// view-space normal to a second target, for the post pass (as in Manifold).

in vec3 v_pos;
in vec3 v_normal;
in vec3 v_col;
flat in float v_dither; // per prism, so overlapping fades don't share holes
flat in float v_glint;
in vec2 v_facetUV;
flat in float v_mirrorFace;

uniform float u_gloss;
uniform float u_crimson;
uniform float u_nearFade; // distance prisms are fully solid from; 0 = off
uniform sampler2D u_webcam;
uniform bool u_webcamEnabled;
uniform vec2 u_webcamScale;
uniform float u_webcamWarpTime;
uniform vec2 u_webcamLevels; // mean red, inverse contrast span
uniform float u_webcamBaseLuma;
uniform vec3 u_webcamOrigin; // form origin in view space

layout(location = 0) out vec4 colour;
layout(location = 1) out vec4 normalOut;

float webcamMaterial(vec3 normal) {
  // Sample a single coherent projection from the visible, deformed surface.
  float distance = max(-u_webcamOrigin.z, 1.);
  vec2 p = (v_pos.xy*distance/max(-v_pos.z, distance*.55) - u_webcamOrigin.xy)*u_webcamScale;
  float depth = (v_pos.z-u_webcamOrigin.z)*max(u_webcamScale.x,u_webcamScale.y);
  float t = u_webcamWarpTime;
  // Preserve the features at the centre. Outer folds throw the reflection
  // farther sideways as their facets turn away from the viewer.
  float outer = smoothstep(.16, .47, length(p*vec2(1., .8)));
  vec2 curl = vec2(sin(p.y*6. + depth*3. + t), sin(p.x*5. - depth*3. - t));
  vec2 uv = vec2(.5) - p + curl*mix(vec2(.022, .018),vec2(.075, .052),outer);
  vec2 shard = vec2(v_dither, fract(v_dither*7.39)) - .5;
  uv += normal.xy*mix(.025,.15,outer) + shard*mix(.025,.09,outer);
  uv = 1. - abs(mod(uv, 2.) - 1.);
  return texture(u_webcam, uv).r;
}

vec3 darkMirror(vec3 n, vec3 view, vec3 key, float highlight) {
  const vec3 ink = vec3(1., .023, .085);
  const vec3 luma = vec3(.2126, .7152, .0722);
  float base = mix(dot(v_col,luma),u_webcamBaseLuma,.78);
  float detail = (webcamMaterial(n) - u_webcamLevels.x)*u_webcamLevels.y;
  float image = max(.08,1. + 1.06*detail);

  vec2 inset = .5 - abs(v_facetUV);
  float edge = min(inset.x,inset.y);
  float aa = clamp(fwidth(edge),.005,.07);
  float face = smoothstep(.018-aa,.065+aa,edge);
  float facing = max(dot(n,view),0.);
  float exposure = mix(.47,1.22,v_dither) * mix(.56,.94,facing);
  float glass = base/dot(ink,luma)*exposure*image*mix(.36,1.,face)*mix(.26,1.,v_mirrorFace);

  // Hard flashes on tilted faces and thin highlights along the bevels.
  vec3 side = normalize(vec3(.8,-.42,.4) + view);
  float sharp = highlight*highlight + .54*pow(max(dot(n,side),0.),128.);
  float bevel = (1.-face)*pow(max(dot(n,key),0.),9.)*.075;
  float flash = u_gloss*(sharp*.39 + bevel) + v_glint*.045;
  return ink*(glass + flash*mix(.38,1.,min(image,1.)));
}

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
  lit += (spec * .32 + rim * .06) * u_gloss * mix(vec3(1.),vec3(1.,.055,.15),u_crimson);
  vec3 glintColour = mix(normalize(max(v_col, vec3(.015))), vec3(1., .045, .13), u_crimson);
  lit += v_glint * (.34 + .32*rim) * glintColour;
  if (u_webcamEnabled) lit = darkMirror(n,V,L,spec);
  colour = vec4(lit, -v_pos.z);
  normalOut = vec4(n, fade); // post.frag.js fades outlines and AO with it
}
