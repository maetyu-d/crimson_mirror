// Convert the downloaded reference files into native assets. No npm packages needed.
import fs from 'node:fs';
import vm from 'node:vm';
const root = new URL('../', import.meta.url);
const read = p => fs.readFileSync(new URL(p, root), 'utf8');
const write = (p, s) => fs.writeFileSync(new URL(p, root), s);
const run = (p, names) => vm.runInNewContext(read(p).replace(/export /g, '') + '\n({' + names.join(',') + '})');
const {IQ, SCHEMES} = run('reference/src/palettes.js', ['IQ', 'SCHEMES']);
const {PRESETS} = run('reference/src/presets.js', ['PRESETS']);
// A native crimson palette, separate from the preserved reference schemes.
IQ[19] = [[0.44,0.013,0.049],[0.27,0.009,0.035],[1,1,1],[0,0.12,0.025]];
SCHEMES.push({name:'Deep crimson',lockStrength:true,lockAmbience:true,
 bg:{palettes:[[19,1]],length:[0.1,0.35],offset:[0,1],strength:[0.82,1],ambience:[0,0]},
 tracks:{palettes:[[19,1]],length:[0.18,0.65],offset:[0,1],strength:[0.82,1],ambience:[0,0]}});
const num = n => Number(n).toFixed(7)+'f';
const arr = a => '{'+a.map(x => Array.isArray(x)?arr(x):num(x)).join(',')+'}';
let s = '// Generated from Liam Egan’s Prism Currents. See reference/ and THIRD_PARTY.md.\n#pragma once\n';
s += 'inline const std::array<Coefficients,18> iqPalettes = {{\n';
for (const i of [...Array.from({length:17},(_,i)=>i+1),19]) s += 'Coefficients'+arr(IQ[i])+',\n';
s += '}};\ninline const std::vector<Scheme> schemes = {\n';
for(const x of SCHEMES) {
 const spec = q => '{ {'+q.palettes.map(([id,w])=>'{'+id+','+num(w)+'}').join(',')+'},'+['length','offset','strength','ambience'].map(k=>arr(q[k])).join(',')+'}';
 s += '{"'+x.name+'",'+spec(x.bg)+','+spec(x.tracks)+','+!!x.lockStrength+','+!!x.lockAmbience+'},\n';
}
s += '};\ninline std::vector<Preset> originalPresets() { std::vector<Preset> result;\n';
const keys = new Set('speed count flowScale flowSpeed evolve drift steer spawnRadius lifeMin lifeMax size stretch spread shimmer gloss uniformity nearFade fov lines ao aoRadius fog shapeSize cling paletteScale paletteOffset unify dof focus aperture maxBlur dark secondPalette switchTime'.split(' '));
for(const [name,x] of Object.entries(PRESETS)) {
 s += '{ Preset p; p.name="'+name+'";\n';
 for(const [k,v] of Object.entries(x)) {
  if(keys.has(k)) s += 'p.look.'+k+'='+ (typeof v==='boolean'?v:k==='count'?Math.round(v):num(v))+';\n';
 }
 if(x.shape) s += 'p.look.shape='+['slab','sphere','torus','capsule','wave'].indexOf(x.shape)+';\n';
 for(const k of ['scheme','scheme2']) if(x[k]) s += 'p.'+k+'='+Math.max(0,SCHEMES.findIndex(q=>q.name===x[k]))+';\n';
 for(const k of ['seed','seed2']) if(x[k]) s += 'p.'+k+'='+x[k]+'u;\n';
 if(x.camera) for(const k of ['position','target']) s += 'p.'+k+'='+arr(x.camera[k])+';\n';
 s += 'result.push_back(p); }\n';
}
s += 'return result; }\n';
write('src/reference_data.hpp',s);
for(const file of ['simulate.vert','prism.vert','prism.frag','post.frag','dof.frag']) {
 let source = read('reference/src/shaders/'+file+'.js');
 source = source.slice(source.indexOf('`')+1,source.lastIndexOf('`')).replace('#version 300 es','#version 330 core').replace(/^precision [^;]+;\s*$/gm,'');
 if(file==='simulate.vert') source = source.replace('uniform float u_time;', 'uniform float u_time;\nuniform vec4 u_audio;\nuniform float u_reactivity;').replace('v.x += u_drift;',`v.x += u_drift;
  v *= 1. + u_reactivity * u_audio.w * .6;`).replace(
    'exp(-u_dt * u_steer)', 'exp(-u_dt * u_steer * (1. + u_reactivity * u_audio.w * 2.))');
 if(file==='simulate.vert') {
   // Differentiate the same simplex polynomial: three noise evaluations,
   // rather than 18 for the central-difference curl. Keep the scalar original
   // for the GPU derivative check; unused functions are removed by the driver.
   const scalar=source.match(/float snoise\(vec3 v\) \{[\s\S]*?\n\}/)[0];
   const gradient=scalar.replace('float snoise(vec3 v)', 'vec3 noiseGradient(vec3 v)').replace(
     /  m = m \* m;\n  return 42\.[^\n]*;/,
     `  vec4 d = vec4(dot(p0,x0),dot(p1,x1),dot(p2,x2),dot(p3,x3));
  vec4 m2 = m*m, m3 = m2*m, m4 = m2*m2;
  return 42. * (m4.x*p0 + m4.y*p1 + m4.z*p2 + m4.w*p3
    - 8. * (m3.x*d.x*x0 + m3.y*d.y*x1 + m3.z*d.z*x2 + m3.w*d.w*x3));`);
   source=source.replace(scalar,scalar+'\n\n// Exact spatial derivative of the simplex polynomial.\n'+gradient);
   source=source.replace(/vec3 curl\(vec3 p\) \{[\s\S]*?\n\}/,`vec3 curl(vec3 p) {
  float t = u_time * u_evolve;
  vec3 a = noiseGradient(p + vec3(0.,0.,t));
  vec3 b = noiseGradient(p + vec3(31.4,-12.7,-t));
  vec3 c = noiseGradient(p + vec3(-9.2,47.1,t*.7));
  return vec3(c.y-b.z, a.z-c.x, b.x-a.y);
}`);
 }
 if(file==='prism.vert') source = source.replace('uniform float u_size;', 'uniform float u_size;\nuniform float u_bands[28];\nuniform vec4 u_audio;\nuniform float u_energy;\nuniform float u_reactivity;').replace('float speed = length(i_velocity.xyz);',`float band = u_bands[int(clamp(floor(i_seed.x * u_routes), 0., 27.))];
  s *= 1. + u_reactivity * (band * .2 + u_energy * .15 + u_audio.w * .5);
  float speed = length(i_velocity.xyz);`).replace(
    'vec3 dims = s * vec3(1.6 + u_stretch * speed * .25, section);',
    'vec3 dims = s * vec3((1.6 + u_stretch * speed * .25) * (1. + u_reactivity * (band * .32 + u_audio.w * .65)), section);').replace(
    'vec3 wp = i_position.xyz + B * (position * dims);',
    'vec3 wp = i_position.xyz * (1. + u_reactivity * (u_audio.w * .10 + u_audio.x * .025)) + B * (position * dims);').replace(
    'v_col = clamp(col, 0., 1.)',
    'col *= max(.18, 1. + u_reactivity * (-.65 + u_energy * .95 + u_audio.w * .9 + u_audio.z * .3));\n  v_col = clamp(col, 0., 1.)');
 if(file==='prism.frag') source=source.replace('uniform float u_gloss;', 'uniform float u_gloss;\nuniform float u_crimson;').replace(
   'lit += (spec * .45 + rim * .08) * u_gloss;',
   'lit += (spec * .32 + rim * .06) * u_gloss * mix(vec3(1.),vec3(1.,.055,.15),u_crimson);');
 if(file==='post.frag'||file==='dof.frag') source=source.replace('uniform vec2 u_resolution;', 'uniform vec2 u_resolution;\nuniform float u_pixelScale;').replace('gl_FragCoord.xy','(gl_FragCoord.xy * u_pixelScale)');
 write('assets/shaders/'+file,source);
}
console.log('Converted',SCHEMES.length,'palette schemes,',Object.keys(PRESETS).length,'presets and five shaders.');
