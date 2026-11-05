// Inigo Quilez cosine palettes, ported from Euphonic:
//   colour = ambience + strength * (a + b * cos(TAU * (c * t + d)))
// https://iquilezles.org/articles/palettes/

// [a, b, c, d] by Euphonic palette id
export const IQ = {
  1: [[0.9, 0.5, 0.5], [0.5, 0.5, 0.5], [1, 0.5, 1.0], [0.2, 0.2, 0.67]],
  2: [[0.4, 0.5, 0.6], [0.7, 0.5, 0.3], [0.5, 0.52, 0.53], [0.7, 0.65, 0.68]],
  3: [[1.2, 1.2, 0.2], [0.7, 0.2, 0.2], [0.5, 0.5, 1.3], [0.3, 0.3, 0.1]],
  4: [[0.5, 0.5, 0.5], [0.5, 0.5, 0.5], [1, 1, 1], [0, 0.33, 0.67]],
  5: [[0.5, 0.48, 1.2], [0.52, 0.5, 0.5], [1, 1, 0.8], [0.4, 0.4, 0.6]],
  6: [[0.5, 0.5, 0.5], [0.1, 0.5, 0.2], [0.5, 0, 0.7], [0.48, 0.5, 0.3]],
  7: [[0.85, 0.54, 0.41], [0.2, 0.4, 0.2], [2, 1, 1.1], [0.02, 0.25, 0.25]],
  8: [[0.6, 1.08, 0.6], [0.5, 0.5, 0.5], [0.5, 0.52, 0.53], [0.7, 0.65, 0.68]],
  9: [[1, 1, 1], [1.3, 1.3, 1.3], [1, 1, 1], [0.1, 0.1, 0.1]],
  10: [[0.5, 0.5, 0.5], [0.3, 0.32, 0.33], [1.01, 1.01, 1.01], [0.1, 0.1, 0.1]],
  11: [[2.9, 0.5, 0.5], [0.2, 0.2, 0.2], [1, 1, 1.0], [0.2, 0.2, 0.67]],
  12: [[0.4, 0.9, 0.9], [0.2, 0.2, 0.2], [1, 1, 1.0], [0.4, 0.2, 0.3]],
  13: [[0.6, 0.4, 0.2], [0.4, 0.4, 0.2], [0.8, 0.8, 0.81], [0.8, 1.9, 0.1]],
  14: [[0.3, 0, 0.3], [0.5, 0.3, 0.0], [0.1, 0.1, 0.1], [0.1, 0.9, 0.5]],
  15: [[0.5, 0.5, 0.5], [0.5, 0.5, 0.5], [1, 0.7, 0.4], [0, 0.15, 0.3]],
  16: [[0.3, 0.1, 0.9], [0.2, 0.1, 0.5], [0.1, 0.2, 1], [0.4, 0.35, 0.3]],
  17: [[0.01, 0.3, 0.01], [0.2, 0.1, 0.2], [0.5, 0.5, 0.2], [0.0, 0.1, 0.2]],
};

const TAU = Math.PI * 2;
export function iq(t, [a, b, c, d]) {
  return a.map((_, i) => a[i] + b[i] * Math.cos(TAU * (c[i] * t + d[i])));
}

// Euphonic's named schemes: a background palette and the palettes its
// tracks (here: routes) draw from, as [id, weight] pairs, plus ranges.
const BG = { offset: [-0.1, 0.1], strength: [1, 1], ambience: [0, 0] };
const TRACK = { offset: [-0.1, 0.1], strength: [1, 1], ambience: [0, 0] };

const scheme = (name, bg, tracks, { lockStrength, lockAmbience, weight = 1 } = {}) => ({
  name,
  weight,
  bg: { ...BG, ...bg },
  tracks: { ...TRACK, ...tracks },
  lockStrength,
  lockAmbience,
});

export const SCHEMES = [
  scheme("Neon sunset",
    { palettes: [[1, 1], [4, 1]], length: [0.01, 0.3], offset: [0, 0.3], ambience: [-0.5, 0] },
    { palettes: [[1, 5], [4, 1]], length: [0.01, 0.4], ambience: [-0.5, 0] },
    { weight: 20 }),
  scheme("Peach pit",
    { palettes: [[7, 1]], length: [0.01, 0.5], offset: [0, 0.3], strength: [0.5, 1], ambience: [-0.2, 0] },
    { palettes: [[11, 20], [12, 1], [7, 1]], length: [0.01, 0.4], strength: [0.5, 1], ambience: [-0.5, 0] },
    { weight: 20 }),
  scheme("Silver dream",
    { palettes: [[10, 1]], length: [0.01, 0.5], offset: [0, 0.3], ambience: [-0.5, 0] },
    { palettes: [[9, 2], [10, 5]], length: [0.01, 5.4], strength: [0.3, 1], ambience: [-0.5, 0] }),
  scheme("Checkmate",
    { palettes: [[10, 1], [9, 1]], length: [0.01, 0.1], offset: [0, 3], ambience: [-0.5, 0] },
    { palettes: [[9, 2], [10, 5]], length: [0.6, 4], strength: [0.3, 1], ambience: [-0.5, 0] }),
  scheme("Halcyon",
    { palettes: [[13, 1]], length: [0.1, 0.5], offset: [0, 6] },
    { palettes: [[13, 5], [12, 2]], length: [0.1, 1], strength: [0.3, 1], ambience: [-0.5, 0] },
    { weight: 20, lockStrength: true, lockAmbience: true }),
  scheme("Somatic twilight",
    { palettes: [[14, 1]], length: [0.5, 3], offset: [0, 3], strength: [0.5, 1], ambience: [-0.1, 0.3] },
    { palettes: [[18, 5], [14, 10]], length: [0.6, 4], strength: [0.8, 1] },
    { weight: 20, lockStrength: true, lockAmbience: true }),
  scheme("Pastello",
    { palettes: [[15, 1], [2, 1]], length: [0.1, 0.5], offset: [0, 3], strength: [0.4, 0.5], ambience: [0.3, 0.3] },
    { palettes: [[15, 5]], length: [0.01, 0.2], offset: [-0.5, 0.5], strength: [0.4, 1.5], ambience: [-0.2, 0.4] },
    { weight: 20 }),
  scheme("Chromatic",
    { palettes: [[2, 1]], length: [0.1, 0.5], offset: [0, 3], strength: [0.4, 1], ambience: [0, 0.3] },
    { palettes: [[2, 5], [9, 2], [10, 5]], length: [0.1, 0.6], offset: [-0.5, 0.5] },
    { weight: 10 }),
  scheme("Living color",
    { palettes: [[10, 1]], length: [0.05, 0.2], offset: [0, 3], strength: [0.4, 1], ambience: [-0.5, 0] },
    { palettes: [[1, 5], [5, 2], [4, 2]], length: [0.01, 0.3], offset: [-0.5, 0.5] },
    { weight: 4 }),
  scheme("Fields of gold",
    { palettes: [[3, 1], [8, 1]], length: [0.05, 0.2], offset: [0, 3], strength: [0.6, 0.8], ambience: [0, 0.1] },
    { palettes: [[8, 5], [3, 5]], length: [0.05, 0.2], offset: [-0.5, 0.5], strength: [0.6, 0.8], ambience: [0, 0.1] },
    { weight: 2 }),
  scheme("Powder",
    { palettes: [[10, 1]], length: [0.05, 0.2], offset: [0, 3], strength: [0.6, 0.8], ambience: [0.3, 0.6] },
    { palettes: [[14, 5], [12, 5]], length: [0.1, 0.6], offset: [-0.5, 0.5], strength: [0.3, 0.8], ambience: [-0.3, 0.8] },
    { weight: 2 }),
  scheme("Night sky",
    { palettes: [[16, 1]], length: [0.1, 0.3], offset: [0, 6], strength: [0.3, 0.5] },
    { palettes: [[16, 5], [13, 5], [2, 5]], length: [0.1, 0.6], offset: [-0.5, 0.5], strength: [0.3, 0.8], ambience: [-0.3, 0.8] },
    { weight: 2 }),
  scheme("Sunset",
    { palettes: [[13, 1]], length: [0.1, 0.5], offset: [0, 6], strength: [0.8, 0.8], ambience: [0.2, 0.2] },
    { palettes: [[13, 5]], length: [0.1, 0.6], offset: [-0.5, 0.5], strength: [0.3, 0.8], ambience: [-0.3, 0.8] },
    { weight: 20 }),
  scheme("Coral reef",
    { palettes: [[12, 1]], length: [0.1, 0.5], offset: [0, 6], strength: [0.3, 0.5], ambience: [-0.4, -0.2] },
    { palettes: [[7, 8], [8, 5], [11, 1]], length: [0.01, 0.3], offset: [-0.5, 0.5], strength: [0.3, 0.8], ambience: [0, 0.2] },
    { weight: 2 }),
  scheme("Green dream",
    { palettes: [[8, 1], [3, 1]], length: [0.1, 0.5], offset: [0, 6], strength: [0.2, 0.5], ambience: [0, 0.5] },
    { palettes: [[3, 8], [8, 5]], length: [0.01, 0.3], offset: [-0.5, 0.5] },
    { weight: 10, lockStrength: true, lockAmbience: true }),
  scheme("Hypercolor",
    { palettes: [[4, 1]], length: [0.02, 0.3], offset: [0, 6], strength: [0.4, 1], ambience: [-0.3, 0.3] },
    { palettes: [[4, 8]], length: [0.01, 0.3], offset: [-0.5, 0.5] },
    { lockStrength: true, lockAmbience: true }),
  scheme("Convive",
    { palettes: [[15, 1]], length: [0.1, 0.3], offset: [6, 6], strength: [0.01, 0.3], ambience: [0, 0.1] },
    { palettes: [[16, 8], [14, 8], [13, 8]], length: [0.01, 0.3], offset: [-0.5, 0.5], strength: [0.8, 1], ambience: [0.1, 0.3] },
    { weight: 4 }),
  scheme("Deep forest",
    { palettes: [[17, 1]], length: [0.1, 0.3], offset: [6, 6], strength: [0.5, 1], ambience: [0, 0.1] },
    { palettes: [[17, 8], [8, 1], [3, 1]], length: [0.01, 0.3], offset: [-0.5, 0.5], strength: [0.5, 1], ambience: [0, 0.1] },
    { weight: 10 }),
];

const between = ([lo, hi], rand = Math.random) => lo + rand() * (hi - lo);

export function weighted(options, rand = Math.random) {
  const total = options.reduce((s, [, w]) => s + w, 0);
  let r = rand() * total;
  for (const [value, w] of options) if ((r -= w) <= 0) return value;
  return options[options.length - 1][0];
}

export const pickScheme = () => weighted(SCHEMES.map((s) => [s, s.weight]));

// Euphonic references palette 18, which it never defined (its coefficients
// stayed zero, giving a flat ambience colour); fall back to 14 here.
const coeffs = (id) => IQ[id] ?? IQ[14];

// mulberry32: a small seeded PRNG, so a scheme + seed always rolls the same
function seeded(seed) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

export const randomSeed = () => Math.floor(Math.random() * 1e6);

// Roll a concrete palette from a scheme: one background and one per route.
export function rollScheme(s, routeCount, seed) {
  const rand = seeded(seed);
  const locked = {
    strength: s.lockStrength ? between(s.bg.strength, rand) : null,
    ambience: s.lockAmbience ? between(s.bg.ambience, rand) : null,
  };
  const roll = (src) => ({
    coeffs: coeffs(weighted(src.palettes, rand)),
    length: between(src.length, rand),
    offset: between(src.offset, rand),
    strength: locked.strength ?? between(src.strength, rand),
    ambience: locked.ambience ?? between(src.ambience, rand),
  });
  return {
    name: s.name,
    bg: roll({ ...s.bg }),
    routes: Array.from({ length: routeCount }, () => roll(s.tracks)),
  };
}
