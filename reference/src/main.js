import {
  Obj,
  Renderer,
  Program,
  Mesh,
  Uniform,
  DollyCamera,
  Triangle,
  Box,
  Geometry,
  GeometryAttribute,
  RenderTarget,
  Texture,
  TransformFeedback,
  Vec2,
  Vec3,
} from "wtc-gl";

import simulateVert from "/src/shaders/simulate.vert.58db18d8cf667e58f3ff.js";
import prismVert from "/src/shaders/prism.vert.69d98e04067c96121924.js";
import prismFrag from "/src/shaders/prism.frag.6b2b9c9249cab5f7d491.js";
import postFrag from "/src/shaders/post.frag.6ec196705eda4af0d715.js";
import dofFrag from "/src/shaders/dof.frag.a52f65f76ce4fa4f830f.js";
import {
  debugPointsVert,
  debugPointsFrag,
  debugTileVert,
  debugTileFrag,
} from "/src/shaders/debug.870c0c8a590e7dd5816f.js";
import { SCHEMES, pickScheme, rollScheme, randomSeed } from "/src/palettes.deb96c64401b3a860ab6.js";
import { PRESETS } from "/src/presets.971142d966ba714d7c69.js";
import { createControls } from "/src/controls.e178dbc37ffc6f240a33.js";

// container shapes, in the order simulate.vert.js numbers them
const SHAPES = ["slab", "sphere", "torus", "capsule", "wave"];

const TAU = Math.PI * 2;
const COUNT = 100000; // prisms simulated
const ROUTES = 28; // ribbons; each has its own palette roll

// --- tweakables (bound to the pane)
const params = {
  scheme: SCHEMES.indexOf(pickScheme()),
  seed: randomSeed(), // palette roll within the scheme
  paletteScale: 1,
  paletteOffset: 0,
  unify: 0, // blends every ribbon toward one shared palette
  // a second palette the flow alternates with
  secondPalette: false,
  switchTime: 20, // seconds of (simulated) time between switches
  scheme2: SCHEMES.indexOf(pickScheme()),
  seed2: randomSeed(),
  preset: "",
  playing: true,
  speed: 1,
  count: COUNT,
  // current
  flowScale: 0.085,
  flowSpeed: 3.2,
  evolve: 0.035,
  drift: 0.5,
  steer: 4,
  spawnRadius: 1.8,
  lifeMin: 6,
  lifeMax: 14,
  // prisms
  size: 0.3,
  stretch: 1,
  spread: 0.35,
  shimmer: 0.015,
  gloss: 1,
  uniformity: 0, // 1 = every prism the same size and shape
  nearFade: 0, // prisms closer than this dither away; 0 = off
  // camera
  fov: 35,
  dof: false, // depth of field
  focus: 0, // focal plane, relative to the orbit target's distance
  aperture: 4,
  maxBlur: 10, // css px
  // look
  lines: 0.55,
  ao: 1,
  aoRadius: 0.9,
  fog: 140,
  // container
  shape: "slab",
  shapeSize: 12,
  cling: 0,
  dark: window.matchMedia?.("(prefers-color-scheme: dark)").matches ?? false,
};

// what a preset falls back to for anything it leaves out
const DEFAULTS = { ...params };

// --- renderer
const dpr = Math.min(window.devicePixelRatio, 2);
const d = new Vec2(window.innerWidth, window.innerHeight);
const renderer = new Renderer({
  dpr,
  width: d.width,
  height: d.height,
  clearColour: [0, 0, 0, 0],
});
const { gl } = renderer;
renderer.getExtension("EXT_color_buffer_float");
document.body.appendChild(gl.canvas);

const camera = new DollyCamera(
  {
    element: gl.canvas,
    target: new Vec3(0, 0, 0),
    minDistance: 10,
    maxDistance: 140,
    ease: 0.12,
    inertia: 0.85,
  },
  { fov: params.fov, near: 0.5, far: 400, aspect: d.width / d.height },
);
const DEFAULT_VIEW = { position: [0, 0, 58], target: [0, 0, 0] };
camera.setPosition(...DEFAULT_VIEW.position);

// the volume the currents fill: as wide as the view, and as deep as the
// shape size says (8 either side at the default of 12)
const boundsFor = (aspect) => [
  Math.max(14, 14 * aspect + 2),
  13,
  (params.shapeSize * 2) / 3,
];

// --- palettes: two slots of ROUTES rows, 4 texels a row (a, b, c, d in rgb;
// length, offset, strength, ambience in alpha). Newborn prisms adopt the
// current slot, so a new scheme flows out of the route origins.
const paletteData = new Float32Array(4 * ROUTES * 2 * 4);
const paletteTexture = new Texture(gl, {
  data: paletteData,
  width: 4,
  height: ROUTES * 2,
  format: gl.RGBA,
  internalFormat: gl.RGBA32F,
  type: gl.FLOAT,
  generateMipmaps: false,
  minFilter: gl.NEAREST,
  magFilter: gl.NEAREST,
  flipY: false, // rows are routes; keep them where they were written
});
let slot = 0;

function writeSlot(s, rolled) {
  rolled.routes.forEach((r, i) => {
    const base = (s * ROUTES + i) * 16;
    const alphas = [r.length, r.offset, r.strength, r.ambience];
    r.coeffs.forEach((v, k) =>
      paletteData.set([...v, alphas[k]], base + k * 4),
    );
  });
  paletteTexture.needsUpdate = true;
}

// --- simulation (transform feedback)
const seeds = new Float32Array(COUNT * 4).map(Math.random);
const positions = new Float32Array(COUNT * 4);
const velocities = new Float32Array(COUNT * 4);
for (let i = 0; i < COUNT; i++) {
  // staggered births so routes never pulse in step
  positions[i * 4 + 3] = -Math.random() * params.lifeMax;
  velocities[i * 4 + 3] = 1;
}

const simUniforms = {
  u_time: new Uniform({ name: "time", value: 0, kind: "float" }),
  u_dt: new Uniform({ name: "dt", value: 0, kind: "float" }),
  u_routes: new Uniform({ name: "routes", value: ROUTES, kind: "float" }),
  u_slot: new Uniform({ name: "slot", value: slot, kind: "float" }),
  u_flowScale: new Uniform({
    name: "flowScale",
    value: params.flowScale,
    kind: "float",
  }),
  u_flowSpeed: new Uniform({
    name: "flowSpeed",
    value: params.flowSpeed,
    kind: "float",
  }),
  u_evolve: new Uniform({
    name: "evolve",
    value: params.evolve,
    kind: "float",
  }),
  u_drift: new Uniform({ name: "drift", value: params.drift, kind: "float" }),
  u_steer: new Uniform({ name: "steer", value: params.steer, kind: "float" }),
  u_spawnRadius: new Uniform({
    name: "spawnRadius",
    value: params.spawnRadius,
    kind: "float",
  }),
  u_life: new Uniform({
    name: "life",
    value: [params.lifeMin, params.lifeMax],
    kind: "float_vec2",
  }),
  u_bounds: new Uniform({
    name: "bounds",
    value: boundsFor(d.width / d.height),
    kind: "float_vec3",
  }),
  u_shape: new Uniform({ name: "shape", value: 0, kind: "float" }),
  u_shapeSize: new Uniform({
    name: "shapeSize",
    value: params.shapeSize,
    kind: "float",
  }),
  u_cling: new Uniform({ name: "cling", value: params.cling, kind: "float" }),
};

const simulation = new TransformFeedback(gl, {
  program: new Program(gl, {
    vertex: simulateVert,
    fragment: `#version 300 es
precision highp float;
out vec4 c;
void main() { c = vec4(0.); }`,
    uniforms: simUniforms,
    transformFeedbackVaryings: ["v_position", "v_velocity", "v_meta"],
  }),
  attributes: {
    a_position: { data: positions, size: 4, varying: "v_position" },
    a_velocity: { data: velocities, size: 4, varying: "v_velocity" },
    a_meta: { data: new Float32Array(COUNT * 4), size: 4, varying: "v_meta" },
  },
  staticAttributes: {
    a_seed: new GeometryAttribute({ data: seeds, size: 4 }),
  },
});

// --- prisms: one instanced box per simulated vertex
const uniforms = {
  u_time: simUniforms.u_time,
  u_palette: new Uniform({
    name: "palette",
    value: paletteTexture,
    kind: "texture",
  }),
  u_routes: simUniforms.u_routes,
  u_size: new Uniform({ name: "size", value: params.size, kind: "float" }),
  u_stretch: new Uniform({
    name: "stretch",
    value: params.stretch,
    kind: "float",
  }),
  u_spread: new Uniform({
    name: "spread",
    value: params.spread,
    kind: "float",
  }),
  u_shimmer: new Uniform({
    name: "shimmer",
    value: params.shimmer,
    kind: "float",
  }),
  u_paletteScale: new Uniform({
    name: "paletteScale",
    value: params.paletteScale,
    kind: "float",
  }),
  u_paletteOffset: new Uniform({
    name: "paletteOffset",
    value: params.paletteOffset,
    kind: "float",
  }),
  u_unify: new Uniform({ name: "unify", value: params.unify, kind: "float" }),
  u_gloss: new Uniform({ name: "gloss", value: params.gloss, kind: "float" }),
  u_uniformity: new Uniform({ name: "uniformity", value: params.uniformity, kind: "float" }),
  u_nearFade: new Uniform({ name: "nearFade", value: params.nearFade, kind: "float" }),
  // post
  u_resolution: new Uniform({
    name: "resolution",
    value: d.scaleNew(dpr).array,
    kind: "float_vec2",
  }),
  u_dpr: new Uniform({ name: "dpr", value: dpr, kind: "float" }),
  u_tanFov: new Uniform({ name: "tanFov", value: 1, kind: "float" }),
  u_lines: new Uniform({ name: "lines", value: params.lines, kind: "float" }),
  u_ao: new Uniform({ name: "ao", value: params.ao, kind: "float" }),
  u_aoRadius: new Uniform({
    name: "aoRadius",
    value: params.aoRadius,
    kind: "float",
  }),
  u_fog: new Uniform({ name: "fog", value: params.fog, kind: "float" }),
  u_dark: new Uniform({
    name: "dark",
    value: params.dark ? 1 : 0,
    kind: "float",
  }),
  u_fogStart: new Uniform({ name: "fogStart", value: 0, kind: "float" }),
  u_bgA: new Uniform({ name: "bgA", value: [1, 1, 1], kind: "float_vec3" }),
  u_bgB: new Uniform({ name: "bgB", value: [0, 0, 0], kind: "float_vec3" }),
  u_bgC: new Uniform({ name: "bgC", value: [0, 0, 0], kind: "float_vec3" }),
  u_bgD: new Uniform({ name: "bgD", value: [0, 0, 0], kind: "float_vec3" }),
  u_bgP: new Uniform({ name: "bgP", value: [0, 0, 1, 0], kind: "float_vec4" }),
  b_scene: new Uniform({ name: "scene", value: null, kind: "texture" }),
  b_normal: new Uniform({ name: "normal", value: null, kind: "texture" }),
  // depth of field
  u_focus: new Uniform({ name: "focus", value: 58, kind: "float" }),
  u_aperture: new Uniform({ name: "aperture", value: params.aperture, kind: "float" }),
  u_maxBlur: new Uniform({ name: "maxBlur", value: params.maxBlur, kind: "float" }),
  b_composite: new Uniform({ name: "composite", value: null, kind: "texture" }),
};

const prismGeometry = new Box(gl);
prismGeometry.addAttribute(
  "i_position",
  simulation.attribute("a_position", { instanced: 1 }),
);
prismGeometry.addAttribute(
  "i_velocity",
  simulation.attribute("a_velocity", { instanced: 1 }),
);
prismGeometry.addAttribute(
  "i_meta",
  simulation.attribute("a_meta", { instanced: 1 }),
);
prismGeometry.addAttribute(
  "i_seed",
  new GeometryAttribute({ data: seeds, size: 4, instanced: 1 }),
);
prismGeometry.setInstancedCount(params.count);

const scene = new Obj();
const prisms = new Mesh(gl, {
  geometry: prismGeometry,
  program: new Program(gl, {
    vertex: prismVert,
    fragment: prismFrag,
    uniforms,
  }),
  frustumCulled: false,
});
prisms.setParent(scene);

// --- post: colour+depth and normals, read by the composite
const targetOptions = {
  colour: 2,
  type: gl.FLOAT,
  format: gl.RGBA,
  internalFormat: gl.RGBA32F,
  minFilter: gl.NEAREST,
  magFilter: gl.NEAREST,
};
const sceneTarget = new RenderTarget(gl, {
  width: d.width * dpr,
  height: d.height * dpr,
  ...targetOptions,
});

const postMesh = new Mesh(gl, {
  geometry: new Triangle(gl),
  program: new Program(gl, {
    vertex: `#version 300 es
in vec3 position;
void main() { gl_Position = vec4(position, 1.0); }`,
    fragment: postFrag,
    uniforms,
    depthTest: false,
  }),
});

// --- depth of field: when on, the composite goes to a target and this pass
// blurs it to the screen by the scene's depth
const compositeTarget = new RenderTarget(gl, {
  width: d.width * dpr,
  height: d.height * dpr,
  type: gl.HALF_FLOAT,
  format: gl.RGBA,
  internalFormat: gl.RGBA16F,
  depth: false,
});

const dofMesh = new Mesh(gl, {
  geometry: new Triangle(gl),
  program: new Program(gl, {
    vertex: `#version 300 es
in vec3 position;
void main() { gl_Position = vec4(position, 1.0); }`,
    fragment: dofFrag,
    uniforms,
    depthTest: false,
  }),
});

// --- debug: tiles showing the simulation buffers and render targets, toggled
// from the pane (see TRANSFORM-FEEDBACK.md). The order is the shader's mode
const DEBUG_TILES = [
  { key: "position", label: "position (xyz → rgb)", sim: true },
  { key: "ageLife", label: "age / life", sim: true },
  { key: "velocity", label: "velocity", sim: true },
  { key: "meta", label: "meta (slot, generation)", sim: true },
  { key: "seed", label: "seed (static)", sim: true },
  { key: "scene", label: "scene colour" },
  { key: "depth", label: "depth" },
  { key: "normals", label: "normals" },
  { key: "composite", label: "dof composite" },
];
// kept out of params so presets don't carry it
const debug = Object.fromEntries(DEBUG_TILES.map((t) => [t.key, false]));

// the simulation output, copied to float textures: texel i is particle i
const DEBUG_GRID = [400, Math.ceil(COUNT / 400)];
const debugTarget = new RenderTarget(gl, {
  width: DEBUG_GRID[0],
  height: DEBUG_GRID[1],
  colour: 4,
  type: gl.FLOAT,
  format: gl.RGBA,
  internalFormat: gl.RGBA32F,
  minFilter: gl.NEAREST,
  magFilter: gl.NEAREST,
  depth: false,
});

const debugUniforms = {
  u_grid: new Uniform({ name: "grid", value: DEBUG_GRID, kind: "float_vec2" }),
  u_rect: new Uniform({ name: "rect", value: [0, 0, 0, 0], kind: "float_vec4" }),
  u_mode: new Uniform({ name: "mode", value: 0, kind: "float" }),
  u_total: new Uniform({ name: "total", value: COUNT, kind: "float" }),
  u_count: new Uniform({ name: "count", value: params.count, kind: "float" }),
  u_speedRange: new Uniform({ name: "speedRange", value: 1, kind: "float" }),
  u_bounds: simUniforms.u_bounds,
  u_fog: uniforms.u_fog,
  b_scene: uniforms.b_scene,
  b_normal: uniforms.b_normal,
  b_composite: uniforms.b_composite,
  ...Object.fromEntries(
    ["Position", "Velocity", "Meta", "Seed"].map((name, i) => [
      `b_dbg${name}`,
      new Uniform({
        name: `dbg${name}`,
        value: debugTarget.textures[i],
        kind: "texture",
      }),
    ]),
  ),
};

// reads the same attributes the prisms do, so it's all still on the GPU
const debugPoints = new Mesh(gl, {
  mode: gl.POINTS,
  geometry: new Geometry(gl, {
    a_position: simulation.attribute("a_position"),
    a_velocity: simulation.attribute("a_velocity"),
    a_meta: simulation.attribute("a_meta"),
    a_seed: new GeometryAttribute({ data: seeds, size: 4 }),
  }),
  program: new Program(gl, {
    vertex: debugPointsVert,
    fragment: debugPointsFrag,
    uniforms: debugUniforms,
    depthTest: false,
  }),
  frustumCulled: false,
});

const debugTile = new Mesh(gl, {
  geometry: new Triangle(gl),
  program: new Program(gl, {
    vertex: debugTileVert,
    fragment: debugTileFrag,
    uniforms: debugUniforms,
    depthTest: false,
  }),
});

// tiles flow in rows from the top left, clear of the pane; each has an html
// label over it
const debugLabels = document.createElement("div");
debugLabels.className = "debug-tiles";
document.body.appendChild(debugLabels);
let debugLayout = [];
let debugLayoutKey = "";

function layoutDebug() {
  const shown = DEBUG_TILES.filter((t) => debug[t.key]);
  const key = `${shown.map((t) => t.key)} ${d.width}x${d.height}`;
  if (key === debugLayoutKey) return;
  debugLayoutKey = key;
  const gap = 8;
  const width = Math.round(Math.min(520, Math.max(320, d.width * 0.32)));
  const right = Math.max(width + 2 * gap, d.width - 300); // the pane's side
  let x = gap;
  let y = gap;
  let rowHeight = 0;
  debugLayout = shown.map((t) => {
    const aspect = t.sim ? DEBUG_GRID[1] / DEBUG_GRID[0] : d.height / d.width;
    const height = Math.round(width * aspect);
    if (x > gap && x + width > right) {
      x = gap;
      y += rowHeight + gap;
      rowHeight = 0;
    }
    const tile = { ...t, mode: DEBUG_TILES.indexOf(t), x, y, width, height };
    x += width + gap;
    rowHeight = Math.max(rowHeight, height);
    return tile;
  });
  debugLabels.replaceChildren(
    ...debugLayout.map((t) => {
      const el = document.createElement("div");
      el.className = "debug-tile";
      el.dataset.key = t.key;
      el.style.cssText = `left:${t.x}px;top:${t.y}px;width:${t.width}px;height:${t.height}px`;
      el.textContent = t.label;
      return el;
    }),
  );
}

function renderDebug() {
  layoutDebug();
  if (!debugLayout.length) return;
  if (debugLayout.some((t) => t.sim)) {
    renderer.render({ scene: debugPoints, target: debugTarget });
  }
  debugUniforms.u_count.value = params.count;
  debugUniforms.u_speedRange.value = params.flowSpeed + Math.abs(params.drift);
  for (const t of debugLayout) {
    // the composite only exists while depth of field is on
    const off = t.key === "composite" && !params.dof;
    debugLabels
      .querySelector(`[data-key="${t.key}"]`)
      ?.classList.toggle("off", off);
    if (off) continue;
    debugUniforms.u_mode.value = t.mode;
    debugUniforms.u_rect.value = [
      (t.x / d.width) * 2 - 1,
      1 - ((t.y + t.height) / d.height) * 2,
      ((t.x + t.width) / d.width) * 2 - 1,
      1 - (t.y / d.height) * 2,
    ];
    renderer.render({ scene: debugTile, clear: false });
  }
}

// --- schemes
let bgTarget = null;

// the params keys of each palette, and what's actually set for each
const PALETTE_KEYS = [
  ["scheme", "seed"],
  ["scheme2", "seed2"],
];
const palettes = [
  { scheme: -1, seed: -1 },
  { scheme: -1, seed: -1 },
];
let showing = 0; // which palette newborns are taking
let switchClock = 0;

// roll a palette into the next slot, so it flows out from the route origins
function show(which) {
  showing = which;
  switchClock = 0;
  const { scheme, seed } = palettes[which];
  const rolled = rollScheme(SCHEMES[scheme], ROUTES, seed);
  slot = 1 - slot;
  writeSlot(slot, rolled);
  simUniforms.u_slot.value = slot;
  const { coeffs, length, offset, strength, ambience } = rolled.bg;
  bgTarget = [...coeffs, [length, offset, strength, ambience]];
}

function setPalette(which, index, seed = randomSeed()) {
  const [schemeKey, seedKey] = PALETTE_KEYS[which];
  params[schemeKey] = index;
  params[seedKey] = seed;
  palettes[which] = { scheme: index, seed };
  if (which === showing) show(which);
  pane?.refresh();
}

// --- presets: everything in params except transient state, with the scheme
// by name so presets survive schemes being added or reordered
const NOT_PRESET = ["playing", "preset"];
const round = (v) => (typeof v === "number" ? Math.round(v * 1e4) / 1e4 : v);

function presetFromParams() {
  const preset = {};
  for (const [key, value] of Object.entries(params)) {
    if (NOT_PRESET.includes(key)) continue;
    preset[key] =
      key === "scheme" || key === "scheme2"
        ? SCHEMES[value].name
        : round(value);
  }
  preset.camera = {
    position: camera.position.array.map(round),
    target: camera.target.array.map(round),
  };
  return preset;
}

// --- camera moves between presets: an orbit around an eased target, so the
// view swings round rather than cutting through the flow
const PRESET_MOVE = 2.5; // seconds, for the camera and the settings alike
let cameraMove = null;
const easeInOut = (k) => (k < 0.5 ? 4 * k * k * k : 1 - (-2 * k + 2) ** 3 / 2);

function orbitOf(position, target) {
  const o = [0, 1, 2].map((i) => position[i] - target[i]);
  const radius = Math.hypot(...o);
  return {
    radius,
    theta: Math.atan2(o[0], o[2]),
    phi: Math.acos(Math.min(Math.max(o[1] / radius, -1), 1)),
    target: [...target],
  };
}

function moveCamera({ position, target }, animate = true) {
  // drop any coasting left from a drag
  camera.sphericalDelta.theta = camera.sphericalDelta.phi = 0;
  camera.panDelta.reset(0, 0, 0);
  const to = orbitOf(position, target);
  if (!animate) {
    camera.target.reset(...target);
    camera.setPosition(...position);
    return;
  }
  const from = orbitOf(camera.position.array, camera.target.array);
  // the short way round
  const turn = ((((to.theta - from.theta) % TAU) + TAU * 1.5) % TAU) - Math.PI;
  to.theta = from.theta + turn;
  cameraMove = { from, to, t: 0 };
}

function updateCameraMove(dt) {
  if (!cameraMove) return;
  const { from, to } = cameraMove;
  cameraMove.t = Math.min(1, cameraMove.t + dt / PRESET_MOVE);
  const k = cameraMove.t;
  const e = easeInOut(k);
  const lerp = (a, b) => a + (b - a) * e;
  const radius = lerp(from.radius, to.radius);
  const theta = lerp(from.theta, to.theta);
  const phi = lerp(from.phi, to.phi);
  const target = from.target.map((v, i) => lerp(v, to.target[i]));
  const r = radius * Math.sin(phi);
  camera.target.reset(...target);
  camera.setPosition(
    target[0] + r * Math.sin(theta),
    target[1] + radius * Math.cos(phi),
    target[2] + r * Math.cos(theta),
  );
  if (k >= 1) cameraMove = null;
}

// grabbing the view hands it straight back to the controls
for (const type of ["pointerdown", "wheel", "touchstart"]) {
  gl.canvas.addEventListener(type, () => (cameraMove = null), {
    passive: true,
  });
}

// --- numeric settings ease to a new preset's values alongside the camera;
// everything else (shape, dark, toggles) switches at once
let settingsMove = null;

function moveSettings(values, animate) {
  const from = {};
  const to = {};
  for (const [key, value] of Object.entries(values)) {
    if (!(key in params) || NOT_PRESET.includes(key)) continue;
    if (
      animate &&
      typeof value === "number" &&
      typeof params[key] === "number"
    ) {
      from[key] = params[key];
      to[key] = key === "count" ? Math.min(value, COUNT) : value;
    } else params[key] = value;
  }
  settingsMove = { from, to, t: 0 };
}

function updateSettingsMove(dt) {
  if (!settingsMove) return;
  const { from, to } = settingsMove;
  settingsMove.t = Math.min(1, settingsMove.t + dt / PRESET_MOVE);
  const e = easeInOut(settingsMove.t);
  for (const key in to) {
    const v = from[key] + (to[key] - from[key]) * e;
    params[key] = key === "count" ? Math.round(v) : v;
  }
  prismGeometry.setInstancedCount(params.count);
  pane.refresh();
  if (settingsMove.t >= 1) settingsMove = null;
}

function applyPreset(name, animate = true) {
  const preset = PRESETS[name];
  if (!preset) return;
  // anything the preset leaves out (e.g. settings added after it was saved)
  // goes back to its default, rather than staying as the last preset had it.
  // Schemes and seeds aren't defaulted: a missing one keeps the current scheme
  // and rolls a new seed
  const { scheme, seed, scheme2, seed2, camera: view, ...rest } = { ...DEFAULTS, ...preset };
  moveCamera(view?.position && view?.target ? view : DEFAULT_VIEW, animate);
  moveSettings(rest, animate);
  params.count = Math.min(params.count, COUNT);
  prismGeometry.setInstancedCount(params.count);
  const indexOf = (n, fallback) => {
    const i = SCHEMES.findIndex((s) => s.name === n);
    return i >= 0 ? i : fallback;
  };
  showing = 0;
  setPalette(1, indexOf(preset.scheme2, params.scheme2), preset.seed2 ?? randomSeed());
  setPalette(0, indexOf(preset.scheme, params.scheme), preset.seed ?? randomSeed());
  params.preset = name;
  pane.refresh();
}

// copies a ready-to-paste entry for presets.js
async function copyPreset() {
  const body = JSON.stringify(presetFromParams(), null, 2).replace(
    /\n/g,
    "\n  ",
  );
  const entry = `  "untitled": ${body},\n`;
  try {
    await navigator.clipboard.writeText(entry);
    return true;
  } catch {
    console.log(entry); // clipboard blocked (e.g. no focus): grab it from here
    return false;
  }
}

// --- resize
function resize() {
  d.reset(window.innerWidth, window.innerHeight);
  renderer.dimensions = d;
  uniforms.u_resolution.value = d.scaleNew(dpr).array;
  camera.perspective({ aspect: d.width / d.height });
  sceneTarget.resize(d.width * dpr, d.height * dpr);
  compositeTarget.resize(d.width * dpr, d.height * dpr);
}
window.addEventListener("resize", resize);

// --- interaction
let time = 0;

const stats = { fps: 60, ms: 16.7 }; // smoothed, for the performance folder

// set by the pane; the next frame is saved as a PNG right after it's drawn,
// while the canvas still holds it
let saveImage = false;
function downloadFrame() {
  gl.canvas.toBlob((blob) => {
    if (!blob) return;
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    const stamp = new Date().toISOString().slice(0, 19).replace(/[:T]/g, "-");
    a.download = `prism-currents-${stamp}.png`;
    a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  }, "image/png");
}

const pane = createControls(
  params,
  SCHEMES.map((s) => s.name),
  {
    stats,
    presetNames: Object.keys(PRESETS),
    // the pane reports its own refreshes as changes, so only act on real ones
    onScheme: (which, i) =>
      i !== palettes[which].scheme && setPalette(which, i),
    onSeed: (which, seed) =>
      seed !== palettes[which].seed &&
      setPalette(which, palettes[which].scheme, seed),
    onReroll: (which) => setPalette(which, palettes[which].scheme),
    onSwitch: () => params.secondPalette && show(1 - showing),
    onCount: () => prismGeometry.setInstancedCount(params.count),
    onPreset: applyPreset,
    onCopyPreset: copyPreset,
    onSaveImage: () => (saveImage = true),
    debug,
    debugTiles: DEBUG_TILES,
  },
);
// reaching for a control hands the settings back mid-move
pane.element.addEventListener("pointerdown", () => (settingsMove = null));

window.addEventListener("keydown", (e) => {
  if (e.target instanceof HTMLInputElement) return;
  // copy the current look as a presets.js entry, unless text is selected
  if ((e.ctrlKey || e.metaKey) && e.code === "KeyC") {
    if (getSelection()?.toString()) return;
    e.preventDefault();
    copyPreset().then((ok) => ok && console.info("preset copied"));
    return;
  }
  if (e.code === "Space") {
    params.playing = !params.playing;
    pane.refresh();
  }
  if (e.code === "KeyR") setPalette(showing, palettes[showing].scheme);
  if (e.code === "KeyD") {
    params.dark = !params.dark;
    pane.refresh();
  }
});

// --- simulation
function syncSimUniforms() {
  for (const key of [
    "flowScale",
    "flowSpeed",
    "evolve",
    "drift",
    "steer",
    "spawnRadius",
    "shapeSize",
    "cling",
  ]) {
    simUniforms[`u_${key}`].value = params[key];
  }
  simUniforms.u_bounds.value = boundsFor(d.width / d.height);
  simUniforms.u_life.value = [
    params.lifeMin,
    Math.max(params.lifeMin + 0.5, params.lifeMax),
  ];
  simUniforms.u_shape.value = Math.max(0, SHAPES.indexOf(params.shape));
}

function step(dt) {
  time += dt;
  simUniforms.u_time.value = time;
  simUniforms.u_dt.value = dt;
  simulation.step();
}

// start with the slot we'll display, then run the currents for a while so
// the frame opens full rather than with every prism being born at once
const initial = new URLSearchParams(location.search).get("preset");
if (initial && PRESETS[initial]) applyPreset(initial, false);
else {
  setPalette(1, params.scheme2, params.seed2);
  setPalette(0, params.scheme, params.seed);
}
bgTarget.forEach(
  (v, i) =>
    (uniforms[["u_bgA", "u_bgB", "u_bgC", "u_bgD", "u_bgP"][i]].value = [...v]),
);
syncSimUniforms();
for (let i = 0; i < params.lifeMax * 20; i++) step(1 / 20);

// --- loop
const mix = (a, b, t) => a.map((v, i) => v + (b[i] - v) * t);
let last = performance.now();
function frame(now) {
  requestAnimationFrame(frame);
  const raw = (now - last) / 1000;
  const dt = Math.min(0.1, raw);
  last = now;
  if (raw > 0) {
    stats.ms += (raw * 1000 - stats.ms) * 0.1;
    stats.fps = 1000 / stats.ms;
  }
  const tdt = params.playing ? Math.min(dt, 1 / 30) * params.speed : 0;

  updateSettingsMove(dt);
  syncSimUniforms();
  if (tdt > 0) step(tdt);

  // alternate palettes; turning the second off brings the first back
  if (params.secondPalette) {
    switchClock += tdt;
    if (switchClock >= params.switchTime) show(1 - showing);
  } else if (showing === 1) show(0);

  // ease the background toward the current scheme
  const kb = 1 - Math.exp(-dt * 0.6);
  ["u_bgA", "u_bgB", "u_bgC", "u_bgD", "u_bgP"].forEach((key, i) => {
    uniforms[key].value = mix(uniforms[key].value, bgTarget[i], kb);
  });

  uniforms.u_dark.value +=
    ((params.dark ? 1 : 0) - uniforms.u_dark.value) * (1 - Math.exp(-dt * 3));
  document.documentElement.classList.toggle("dark", params.dark);
  for (const key of [
    "size",
    "stretch",
    "spread",
    "shimmer",
    "paletteScale",
    "paletteOffset",
    "unify",
    "gloss",
    "uniformity",
    "nearFade",
    "lines",
    "ao",
    "aoRadius",
    "fog",
    "aperture",
    "maxBlur",
  ]) {
    uniforms[`u_${key}`].value = params[key];
  }

  updateCameraMove(dt);
  if (camera.fov !== params.fov) camera.perspective({ fov: params.fov });
  camera.update();
  uniforms.u_fogStart.value = Math.max(
    0,
    camera.position.distance(camera.target) - 10,
  );
  uniforms.u_tanFov.value = Math.tan((camera.fov * Math.PI) / 360);
  uniforms.u_focus.value = Math.max(
    1,
    camera.position.distance(camera.target) + params.focus,
  );

  renderer.render({ scene, camera, target: sceneTarget });
  uniforms.b_scene.value = sceneTarget.textures[0];
  uniforms.b_normal.value = sceneTarget.textures[1];
  if (params.dof) {
    renderer.render({ scene: postMesh, target: compositeTarget, clear: false });
    uniforms.b_composite.value = compositeTarget.textures[0];
    renderer.render({ scene: dofMesh, clear: false });
  } else renderer.render({ scene: postMesh, clear: false });

  // saved images leave the debug tiles out
  if (saveImage) {
    saveImage = false;
    downloadFrame();
  } else renderDebug();
}
requestAnimationFrame(frame);
