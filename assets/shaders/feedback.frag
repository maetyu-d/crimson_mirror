// Appended to the shared post-process sky/finish functions at build time.
uniform sampler2D b_composite;
uniform sampler2D b_history;
uniform vec2 u_feedbackCentre;
uniform float u_feedbackDt;
uniform float u_persistence;
uniform float u_feedbackHit;
uniform float u_feedbackDrift;
uniform bool u_historyValid;
layout(location = 1) out vec4 nextHistory;

float magnitude(vec3 c) { return max(abs(c.r), max(abs(c.g), abs(c.b))); }

void main() {
  vec2 px = gl_FragCoord.xy;
  vec2 uv = px / u_resolution;
  vec3 current = texelFetch(b_composite, ivec2(px), 0).rgb;
  vec3 signal = current - finishColour(sky(px), px);

  // Sampling a slightly expanded image pulls its surviving traces inward.
  // Rates are in seconds, independent of audio-driven simulation speed.
  vec2 previousUV = u_feedbackCentre + (uv - u_feedbackCentre) * exp(u_feedbackDrift * u_feedbackDt);
  bool inside = all(greaterThanEqual(previousUV, vec2(0.))) && all(lessThanEqual(previousUV, vec2(1.)));
  vec3 held = vec3(0.);
  if (u_historyValid && inside)
    held = texture(b_history, previousUV).rgb * exp(-u_feedbackDt / max(u_persistence, .001));

  // An envelope, rather than additive accumulation, preserves fine detail
  // and bounds brightness even when a pose is held for a long passage.
  vec3 deposit = signal * (.17 + .23 * u_feedbackHit);
  vec3 memory = u_feedbackDt > 0. ? mix(held, deposit, step(abs(held), abs(deposit))) : held;
  float occupied = smoothstep(.015, .15, magnitude(signal));
  colour = vec4(current + held * mix(1., .18, occupied), 1.);
  nextHistory = vec4(memory, magnitude(memory));
}
