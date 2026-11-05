# Prism Currents — C++ audio visualiser

A native C++17 / OpenGL adaptation of [Liam Egan’s Prism Currents](https://codepen.io/editor/shubniggurath/pen/01a164e2-494a-79a0-948d-59baebda2dc9), with audio file playback, live input and a built-in generative demo. The evolving crimson form has an audio-reactive afterimage loop and a live node patch with surface-following roots. No browser or network connection is required to run it.

## Run on this Mac

Double-click **Launch Prism Currents.command** to build and open **Prism Currents.app**. See [Build](#build) for prerequisites and terminal commands.

The default canvas shows the live patch. **G** or the small **Patch** button switches to the clean visual view. **Controls** at the bottom right opens a compact panel with **Audio** and **Look** tabs. **Close**, clicking outside, or Escape dismisses that panel; **Tab** hides the entire interface. There is no canvas logo or title. **H** shows shortcuts when needed.

## Live node patch

The patch imagines the visualiser as a fictional visual programming language. Its 16 nodes, 27 connections including the history loop, and two scene feeds expose the actual fixed C++ pipeline. A compact patch leaves open space on the right for the form and its living connections:

`Audio → Spectrum / Envelope → Smooth → Response → Curl / Integrate → Prisms → generated visual`

Sensitivity, the simulation clock, the container and the material feed the appropriate stages. Each node has a short title and a compact graphic: stereo scope, spectrum, envelope, response meters, clock hands, form outlines, particle-budget dots, a prism glyph or the live material palette. These use the running analyser, controls and evolution values. There are no lane headings, type annotations, port labels, numeric diagnostics, explanatory panels or footer hints. Precise parameter values remain in Controls.

The mesh and material outputs emerge from the node edges as braided crimson roots. Staggered branches flatten and fan into the evolving surface, then run along it with the same curl flow. Tangent-matched joins, surface-aligned prism faces, irregular widths and tapered tips soften the transition into the visual. Their colour gradually settles into the form's material. They share its palette, lighting, depth, ambient occlusion, fog, optional depth of field and audio pulses. Hiding the patch or exporting a clean image omits this node-owned tissue.

Two small transform-feedback passes generate 72 surface samples and 2,232 tissue segments on the GPU. They reuse the simulation's signed distance field and curl functions; there is no interactive GPU-to-CPU readback or second copy of the form mathematics. The patch view uses an off-axis camera projection to expose the joins. Root placement and ambient occlusion use that exact projection, including preset field of view, so roots remain attached when nodes or the camera move. Clean view uses the original centred composition.

`Evolution → LFO → Morph → Curl / Prisms`

EvolutionClock advances from frame time independently of the audio-driven simulation speed. SlowLFO supplies a 60-second form cycle, 18-second breathing, 37-second field variation and 29-second prism-profile variation. FormMorph blends signed distance fields continuously through slab, torus, sphere, capsule and wave, starting with the selected base form. It also varies the boundary size, flow scale, prism size and stretch. Its outlines and meters follow the current form transition and breathing.

**Evolve form** is on by default in **Controls → Look**. Switch it off to hold the selected base form and base parameters. Choosing a preset or base form starts a fresh cycle. **P / Freeze** holds both the evolution clock and particle simulation while audio analysis continues. Audio still modulates the evolving form, so slow structural changes and fast musical response work together.

`Prisms / Material / Response → Feedback ↺ → generated visual`

The Feedback node exposes the actual previous-frame loop with an exponential decay curve and a return wire. Faint copies of earlier forms drift slowly inward; musical attacks leave stronger impressions. The history stores the shaded form relative to its background, keeping the sky and film grain out of the loop. A bounded envelope prevents held poses from continually brightening. The same lens pass treats the live visual and its ghosts, while the nodes and controls are drawn afterwards and stay sharp.

**Controls → Look → Afterimage** sets the decay time constant from **Off–3 seconds**, default **1.25 seconds**. The loop uses elapsed frame time independently of simulation speed; Freeze holds its memory and inward drift. Resizing, changing presets, switching dark mode or changing between patch and clean framing starts fresh history. Clean-view exports hold the current history without advancing it; exports from the patch use a fresh centred form to omit the node-owned roots and their ghosts.

Drag node headers to rearrange the patch, click to highlight connections, and press **Home** to reset the layout. Selecting or hovering over a node highlights its connected cables. Moving packets and cable brightness follow signal activity. **P** freezes the visual simulation while the audio analysis stays live. Node positions are a visual layout; the cable topology represents the existing program and cannot be rewired. Parameter changes remain in Controls, and the corresponding node graphics update with them.

Choose **Controls → Open** or drop a **WAV, MP3 or FLAC** onto the window. **Demo** in the Audio tab plays a small generated 112 BPM study. The app starts in a silent visual preview until you choose an audio source.

**Controls → Input** lists the Mac’s input devices. Select a microphone, audio interface or an already installed loopback device. macOS requests microphone permission on the first selection. Live input is analysed without monitoring it to the speakers. To visualise another app’s audio, route that app to your loopback device and select the matching input here.

**Look** offers the 16 original motion/geometry presets, a new **Deep crimson** palette and the 18 reference palette schemes, plus slab, sphere, torus, capsule and wave containers. The app opens in deep red/crimson shades with matching controls and meters. Changing a motion preset keeps your selected palette, including when palettes alternate. The default uses 40,000 prisms; the slider supports up to 100,000. Additional prisms grow in gradually after increasing the count.

The flow now uses analytic simplex-noise derivatives, reducing its noise evaluations from 18 to 3 per particle. Ambient occlusion and depth of field run once per logical pixel before the completed scene is upscaled; controls retain full Retina resolution. Uniform locations are cached. Before the live patch and feedback additions, this optimisation improved the uncapped default benchmark on this Mac from 45.4 to 81.5 FPS, and the 100,000-prism depth-of-field preset from 15.1 to 33.4 FPS. Normal playback remains capped at 60 FPS.

## Controls

| Key / gesture | Action |
|---|---|
| O | Open audio |
| Space | Play / pause audio; freeze currents when no source is loaded |
| P | Freeze / resume form evolution and currents |
| M | Mute / unmute output; analysis continues before the output fader |
| G | Hide / show the live patch |
| Home | Reset node positions |
| Tab | Hide / show the entire interface |
| F | Toggle presentation view |
| A | Toggle automatic orbit |
| R | Roll new colours |
| D | Toggle dark background |
| S | Save a clean PNG using the native save dialog |
| H | Show / hide help |
| Escape | Close the popup or help, or restore hidden controls |
| Left drag / scroll / right drag | Orbit / zoom / pan |

The file progress bar supports seeking. Output volume affects playback and the demo; live input has no speaker output. Audio modulates the crimson brightness without washing it toward white.

- **Sensitivity: Off–16×**, default 1×. Automatic level tracking adapts to the source; manual sensitivity trims both sustained energy and transient pulses. Zero suppresses all visual audio cues without muting the music or changing the actual peak input meter.
- **Reactivity: Off–8×**, default 1.35×. Zero restores unmodulated flow. The low end permits very subtle motion and colour changes; the upper range gives more extreme geometry modulation. Flow speed remains bounded for stability.
- **Smoothing: Off–2,000 ms**, default Off. Smooths the 28 bands, energy, bass/mid/treble and transients together before they drive the visuals. Higher values soften attacks and lengthen transitions. Peak dBFS remains an actual input reading. The displayed time is the filter time constant; Off adds no smoothing beyond the fast analysis envelope.

Sensitivity and Reactivity use cubic slider curves, reserving more travel for values below 1×. Small multipliers show up to four decimal places. **Hold Shift while dragging any slider for tenfold finer adjustment.** Smoothing also gives extra travel to shorter times. Slider drags keep control of the pointer when it leaves the panel, so adjusting a value does not rotate the camera.

Afterimage uses a separate pair of full-float GPU history textures and one logical-resolution composite pass. Switching it Off bypasses that pass and clears the retained memory.

On macOS, enable **Controls → Look → Webcam texture** to split the camera image across tilted crimson shards with dark edges and sharp glints. Allow Camera access when prompted. The Material node shows a mirrored crimson preview, and the texture also works with the interface hidden using Tab. Turning the option off stops capture and restores the selected palette.

## Audio mapping

- A 2,048-point Hann-windowed stereo FFT supplies 28 logarithmic bands, from 30 Hz to 16 kHz, one for each current route. Channel powers are averaged so opposite-polarity stereo does not cancel the analysis.
- Bass (30–220 Hz) widens and compresses the body. Mids (220 Hz–2.4 kHz) twist its upper and lower folds in opposite directions. Treble (2.4–16 kHz) makes the fine fibres shimmer and flicker. The same bass and mid deformation carries the surface grafts, so the feeds stay attached as the form moves.
- A separate 512-sample amplitude envelope catches attacks quickly. Linear band amplitudes and a slow source-level reference preserve drum dynamics that the earlier broad dB scale compressed. A small noise gate keeps silence quiet.
- Detected transients accelerate and steer the current, stretch prisms, expand the ribbons and brighten the crimson material. Sustained energy drives the underlying motion; the flow calms between hits. The signal comes from the audio samples, with no clock-generated beat animation.
- Approximately 10 ms attack and 105 ms release smoothing keep the response quick. The live patch displays these signals; G hides it for a clean visual view.
- The audio callback uses a bounded, single-producer/single-consumer queue. FFT work runs on the graphics thread. Decoding and device streams use 48 kHz float stereo.

## Build

Requires CMake 3.20+ and a C++17 compiler. On macOS, install the Xcode command line tools. raylib, GLFW and miniaudio are included, so dependency downloads are unnecessary.

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native -j 6
./build-native/PrismCurrents.app/Contents/MacOS/PrismCurrents
```

The build copies shaders, fonts and credits into the app bundle. You can move the app independently of the source tree. The macOS build is verified; Windows and Linux paths are included but unverified. On those platforms, use drag-and-drop or the command line to open audio, and provide the platform’s normal OpenGL/GLFW development packages.

## Verify

```sh
./build-native/prism_tests
./build-native/prism_response_tests
./build-native/prism_evolution_tests
./build-native/PrismCurrents.app/Contents/MacOS/PrismCurrents \
  --response-test --capture "$PWD/captures/demo-response-hit.png"
./build-native/PrismCurrents.app/Contents/MacOS/PrismCurrents \
  --render-test --frames 90 --capture "$PWD/captures/render.png"
./build-native/PrismCurrents.app/Contents/MacOS/PrismCurrents \
  --smoke-test --frames 90 /absolute/path/to/test.wav
```

`--render-test` feeds the analyser a deterministic synthetic signal without opening an audio device, then checks GPU particle data, OpenGL errors and analytic noise derivatives against numerical derivatives of the preserved scalar noise at 512 GPU sample points. Add `--show-ui` for an interface capture, `--preset 0..15` to inspect a motion preset, or `--shape 0..4` to inspect a container. Add `--benchmark` to disable the frame cap and VSync and report elapsed frame time after startup; GPU completion is included. `--smoke-test` exercises real device playback with output muted. A nonzero exit status means a check failed. The test executable also accepts additional one-second MP3/FLAC fixtures to verify their decoders.

`tools/prepare_reference.mjs` regenerates the native palette/preset data and adapted shaders from the downloaded reference using Node.js; normal builds use the generated assets and do not need Node.js.

See `THIRD_PARTY.md` for the original author and library credits, and `Verification.md` for the completed checks.

`prism_response_tests` measures the actual demo at full and 1.5% amplitude, checks repeated onsets and substantial motion changes, verifies transient attack/release and rejects false rhythmic onsets in a steady quiet tone. `--response-test` selects a trough and hit from the demo, holds the particle pose fixed, and compares the rendered brightness and changed pixels. It optionally exports both images; the quieter image receives a `-quiet` suffix.

For interface captures, add `--show-ui` to include the live patch and buttons, `--hide-patch` for the clean view with buttons, `--show-controls` to expand the settings panel, or `--show-look` for the Look tab. The S shortcut continues to export the visual scene without nodes or controls.

The response tests also cover the extended ranges, low-end beat attenuation, smoothing attack/release, frame-rate independence and source-change reset. For a configured integration run, use `--render-test --sensitivity 0.01 --reactivity 0.02 --smoothing 500`; the smoothing argument is in milliseconds.

The evolution tests cover clock hold/bypass, frame-rate independence, continuous keyframe/loop transitions, all five selected base forms and ten minutes of bounded modulation. Use `--form-time 24` to start at a specific phase, or `--no-evolution` to retain a fixed base form for comparisons. `--render-test --benchmark --frames 3600` exercises a complete 60-second form cycle with deterministic audio and an uncapped renderer.

The render checks also validate the actual feedback shader with a pulse followed by an empty sky: one-second decay at 60 and 240 updates/second, frozen history, held-input brightness and invalidated-history clearing. Active image history is checked for finite, bounded values. Use `--afterimage 0` for bypass or `--afterimage 3` for maximum persistence.
