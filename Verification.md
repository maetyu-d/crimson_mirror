# Verification — 4 November 2026

## Version 1.11 — crimson video feedback

- Added a GPU history loop between scene shading and depth of field. Previous form traces decay exponentially and drift inward at 1.25% per second; the form's actual beat envelope scales retained impressions from 17% to 40%. A bounded peak envelope prevents additive brightness growth. Background colour and film grain are excluded using the post-process shader's shared sky and finishing functions.
- The UI is drawn after history/lens composition, keeping node names, live traces and controls crisp. Added one minimal Feedback node with its actual decay curve and return wire, linked to Prisms, Material and Response. Existing graft geometry remains attached.
- Added Look → Afterimage, Off–3 seconds (decay time constant), default 1.25 seconds; `--afterimage` supplies the same range. Frame time drives decay independently of simulation speed. Freeze holds the loop. History resets on resolution, preset, dark-mode and patch/clean framing changes; Off bypasses the pass and invalidates memory. Export redraws do not advance history.
- Two full-float history textures preserve quiet decay; the extra logical-resolution pass runs entirely on the GPU. Readback occurs only in explicit validation.
- Built without application warnings. Default 40,000-prism feedback and Off comparisons, plus 100,000-prism depth of field at the maximum 3-second setting, passed with finite body, graft and history data and GL error 0. Active history peaks were 0.499135 and 0.434881. The actual GPU feedback shader passed pulse decay at 60/240 updates per second, freeze, sustained-input bounding and reset checks; maximum error was 0.000002.
- Visually inspected `captures/crimson-feedback.png`, `captures/crimson-feedback-off.png` and `captures/crimson-feedback-depth.png`. The Feedback node and Afterimage control fit. At the same deterministic pose, afterimages changed 22.83% of the open form area by more than 12 combined RGB levels and increased mean red by 8.40 levels. The original direct audio-to-image response check retained 3.52353× hit/trough brightness and 79.5888% changed pixels.

Previous version 1.10 app and source archives are retained in `releases/1.10.0/`.

## Version 1.10 — minimal patch text

- Removed lane headings, node type subtitles, source/frequency labels, port names, numeric diagnostics, status words, the permanent shortcut footer and click-to-inspect prose. Node names are short and appear once per card. Selecting a node only highlights its connections.
- Replaced textual diagnostics with compact meters, clock hands, shape outlines, particle-budget dots and a size/stretch prism glyph driven by the current analyser, controls and form state. Stereo traces, spectrum, envelope history, LFO, palette and surface grafts remain live. Precise parameter labels and values remain in the collapsed Controls drawer.
- Removed the redundant popup footer hint and the obsolete node-inspection help line.
- Built without application warnings. The 64-frame sphere/capsule integration passed with 40,000 finite live prisms, 2,232 finite tissue segments, GL error 0 and root projection error 0.000031 logical pixels. Visually inspected `captures/minimal-patch-text.png`; the patch has titles and graphics only, with no footer or port captions.

Previous version 1.9 app and source archives are retained in `releases/1.9.0/`.

## Version 1.9 — surface grafts and open composition

- Replaced point-like emitter junctions with surface-following fans. Six sites project just inside the evolving signed distance field; 12 samples per site follow its tangential curl flow. Cubic Hermite interpolation keeps the graft smooth between samples. Staggered forks, tangent-matched contacts, surface-aligned flat prism faces, irregular widths and fine tapered tips blend the roots into the body.
- Two GPU transform-feedback passes produce 72 surface samples and 2,232 tissue segments. The same instanced geometry, palette, material, depth, light, AO, fog, lens and audio features serve the roots and form. Normal playback adds no GPU readback.
- Reduced the patch layout another 11% at the default window size and reframed the form into the open area on the right. Patch shading dissolves before the exposed joins. Clean view/export keeps the original centred composition.
- Corrected root projection to use the rendered preset field of view and off-axis camera. The AO position reconstruction uses the same view offset and effective field of view.
- Built without application warnings. Audio/decoder/scope, musical response/control/smoothing and evolution test executables passed. CTest was unavailable, so the executables were run directly.
- Default slab, sphere/capsule, capsule/wave, 100,000-prism depth-of-field and alternate-camera GPU checks passed with finite body/tissue data and GL error 0. Root reprojection error was 0.000031 logical pixels. Maximum surface fit error across these checks was 0.823 world units; numerical gradient validation remained at mean 0.000651 / maximum 0.049393.
- Visually inspected `captures/integrated-default.png`, `captures/integrated-grafts.png` and `captures/integrated-depth.png` for composition and exposed surface joins. The audio-to-image comparison passed: 3.52353× hit/trough brightness and 79.5888% changed pixels.
- The packaged 1.9.0 app opened successfully; the final native window showed Built-in music with active scope, spectrum, envelope and response readings, and the Controls drawer collapsed. Ad hoc signature verification and both ZIP integrity checks passed; packaged shaders match source.

Previous version 1.8 app and source archives are retained in `releases/1.8.0/`.

## Version 1.8 — living connections

- Replaced the two screen-space cables and floating socket rings/collars/labels with 3D branching prism tissue. Two braided trunks join six actual ribbon emitters, following their current position and local flow direction through the evolving container. The richer feeding-trunk colour grades back to each emitter's exact palette at the tips.
- Tissue shares the form's instanced cube mesh, palette, material shader, light, depth buffer, ambient occlusion, fog and optional lens pass. Branches taper into the emitters and pulse with the same audio features.
- Node port coordinates unproject into camera-relative world roots each frame, preserving attachment during node movement, viewport resizing, camera orbit/pan/zoom and preset changes.
- Six emitter frames and 1,188 tissue segments are generated in two small GPU transform-feedback passes using the simulation's shared source. Interactive operation performs no GPU readback. Validation reads the segment buffers only during explicit render tests, checks finite values/positive dimensions, and checks root screen-projection error.
- Clean view and clean PNG export omit the node-owned tissue.
- Built without application warnings. Default/rounded, 100,000-prism depth-of-field and a second-camera preset all retained finite prism and tissue data with GL error 0. Node-root projection error ranged from 0.0298 to 0.0472 logical pixels. The existing audio-to-image response check passed (3.52× hit/trough brightness; 79.59% changed pixels).

Previous version 1.7 app and source archives are retained in `releases/1.7.0/`.

## Version 1.7 — evolving form

- Added a frame-time evolution clock independent of audio-driven particle speed. The form traverses all five signed-distance containers over 60 seconds with smoothstep interpolation, alongside 18-second breathing, 37-second flow-scale variation and 29-second prism-profile variation.
- The GPU uses the same blended container for boundaries, surface normals, wall margins and route origins. Rendering uses the modulated prism size and stretch. Base parameters are copied each frame, preventing cumulative drift.
- Added EvolutionClock, SlowLFO and FormMorph to the live patch, with real phase/shape/blend/breathing/field readouts and connections into CurlField and PrismInstances. The patch now has 15 nodes and is about 13% smaller at the default window size. The direct scene jacks remain.
- Evolve form is enabled by default in Look; disabling it restores the selected base values. Freeze holds both clocks. Choosing a preset/base form starts the evolution cycle from that form.
- New evolution tests passed: clock hold/bypass, frame-rate independence, actual geometry changes, continuous keyframe/loop weights, all five base forms and 600 seconds of bounded modulation per base. Existing audio/decoder/scope and musical-response/control tests also passed.
- A 3,600-frame deterministic audio/GPU run traversed the complete 60-second form cycle and retained 40,000 finite live prisms with GL error 0. Gradient validation stayed at mean 0.000651 / maximum 0.049393. A separate 25.5-second sphere/capsule phase and evolution-disabled integration also passed.
- Visually inspected `captures/evolving-patch.png`, `captures/evolving-form-later.png` and `captures/evolving-form-controls.png`: all three evolution nodes and their ports fit, the later form is visibly compact and rounded, and the new Look toggle fits with the other controls.

Previous version 1.6 app and source archives are retained in `releases/1.6.0/`.

## Version 1.6 — cables enter the visual

- Removed the RenderOut node, thumbnail, output label and framebuffer-preview API. The remaining 12 nodes retain the existing audio and simulation signal path.
- PrismInstances and Material now send their mesh and colour cables to bare sockets in the full-size generated scene. The terminals drift with the real simulation clock, and activity/glow follow the existing motion, material and transient values. Node dragging keeps the source ends attached.
- Grid and patch shading dissolve across the right side to expose the scene at the receiving ends. There is no separate output card or preview viewport.
- Rebuilt without application-source warnings and visually inspected `captures/direct-scene-patch.png`. The 120-frame deterministic audio/GPU integration check passed with 40,000 finite live prisms, GL error 0 and the existing gradient validation unchanged (mean 0.000651, maximum 0.049393).

Previous version 1.5 app and source archives are retained in `releases/1.5.0/`.

## Version 1.5 — live node patch

- Added a fictional patch notation with 13 draggable nodes and 21 typed connections for the real analyser, smoothing, response, simulation, instanced geometry, material and output stages.
- Stereo scope samples come from the analyser's existing main-thread history; no second audio consumer or audio callback work was added. Scope tests cover stereo preservation, stopped input and source reset.
- The output monitor borrows the existing completed GPU texture. The material swatch uses the active renderer palette, including palette changes, rather than a separate hard-coded colour selection.
- G toggles the patch; Tab toggles the entire interface; Home resets positions. Clicking nodes opens a contextual explanation. Existing audio, response, smoothing and camera controls remain available, and S exports a clean visual.
- Built with no application warnings. Both audio/decoder/scope and musical-response/control test executables passed. The local CTest command was unavailable, so the executables were invoked directly.
- Default patch integration: 40,000 finite live prisms, GL error 0. A 100,000-prism depth-of-field case with 500 ms smoothing also passed; its output monitor showed the completed lens image. Analytic gradient validation retained mean error 0.000651 and maximum 0.049393.
- Visually inspected `captures/live-patch.png` and `captures/live-patch-lens.png`. The packaged app was restarted and built-in music resumed; real waveform, spectrum, history, motion and material readings changed with playback. G and Tab toggles, Escape dismissal and the P clock hold/resume were exercised. The app was left running with the patch visible.
- Automated pointer inspection/dragging was interrupted by the computer-use tool's `windowNotFoundAtPosition` error after starting demo playback; those gestures have not received an end-to-end automated check. Keyboard interaction and live output remained available.

Previous version 1.4 app and source archives are retained in `releases/1.4.0/`.

## Version 1.4 — minimal interface

- Removed the top-left logo, title and subtitle, including the startup branding frame.
- Replaced the permanent inspector with a compact, collapsed-by-default Controls panel at the bottom right. Audio and Look tabs retain the existing source, transport, response, smoothing and appearance controls.
- Removed the spectrum, frequency legend, signal meters, FPS display, onset indicator, persistent shortcut hints and save/hide button group. Saving and the other shortcuts remain available; H opens a plain shortcuts dialog and Tab hides the Controls button as well.
- Simplified labels and reduced the panel to 244 × 312 logical pixels for the default Audio view. A file's seek bar adds 32 pixels. The Look view is 244 × 440. The panel closes with Close, Escape or an outside click.
- Existing audio/decoder and response/control tests passed. GPU render validation retained 40,000 finite live prisms with no GL errors.
- Visually inspected the collapsed canvas and both expanded tabs; labels, sliders and buttons fit within each panel. The collapsed preview is retained at `captures/minimal-interface.png`; the temporary expanded-tab captures were cleared after inspection to preserve disk space.
- The signed version 1.4 app opened independently of the build directory. Demo playback resumed successfully, and the controls were left collapsed. App and source ZIP integrity checks passed.

Previous version 1.3 app and source archives are retained in `releases/1.3.0/`. Superseded temporary QA image files were cleared to preserve disk space; historical test results below are retained.

## Version 1.3 — wider controls and smoothing

- Sensitivity now spans Off–16× and Reactivity Off–8×. Cubic curves provide extra travel near zero; slider position/value round trips and monotonicity passed at 1,001 points per range. Shift-drag adds tenfold finer adjustment, and small multipliers show up to four decimal places.
- Fixed sensitivity attenuation for transient cues. At 0.001× sensitivity, demo energy peaked at 0.000988 and the beat envelope at 0.001; at zero both were exactly zero. Actual input RMS stayed active. Checks at 0, 0.001, 0.01, 0.1, 1 and 16× passed.
- Added Off–2,000 ms smoothing to bands, energy, bass/mid/treble and onset envelopes before rendering. Off passes through immediately; 500 ms smoothing reached approximately 63% after 500 ms, produced matching results at 60 and 240 updates/second, and released towards silence. Source reset clears filter history. Actual RMS/peak readings are not filtered by the new control.
- Existing demo-dynamics, quiet-audio, transient latency/release, FFT, stereo polarity, RMS and decoder tests passed. Application source built without warnings.
- GPU integration checks passed at 0.01× sensitivity / 0.02× reactivity / 500 ms smoothing, at the maximum 16× / 8× settings, and at zero sensitivity with 2,000 ms smoothing. Each run retained 40,000 finite live prisms with no GL errors.
- Visually inspected `captures/expanded-controls.png`; all four audio sliders and the fine-adjustment hint fit in the control panel.

Previous version 1.2 app and source ZIPs are retained in `releases/1.2.0/`.

## Version 1.2 — stronger musical response

The user reported that the built-in study was not sufficiently reactive. The earlier 60 dB display scale kept most musical levels high and steady, while the beat changed size by only 18%. Version 1.2 replaces that response with linear, source-normalised amplitudes, a 512-sample attack envelope, a 2,048-point FFT, faster release and larger direct geometry/material modulation. The demo audio itself is unchanged.

Checks passed:

- Existing audio/decoder tests, including stereo polarity and RMS calibration.
- Eight seconds of the actual built-in demo: energy 0.307–0.981, motion multiplier 0.697–3.388, 40 detected transients after the first second.
- The same demo at 1.5% amplitude: energy 0.229–0.869, motion multiplier 0.510–3.130, 35 transients. Onsets continued throughout each test.
- A decaying 80 Hz transient triggered within 5.33 ms of delivered test samples; energy rose above 0.5 and released to 0.0049. This measures the analyser and excludes OS/device latency.
- A steady quiet 1 kHz tone produced no false rhythmic onsets after settling; silence produced no energy or onset cues. Reactivity zero restores an unmodulated motion multiplier of one.
- A GPU comparison used actual demo trough/hit features at the same fixed particle pose. The hit image was 3.52× brighter and changed 79.59% of pixels by the test threshold. Images are `captures/demo-response-hit-quiet.png` and `captures/demo-response-hit.png`.
- Uncapped rendering with the new dynamics: 82.72 FPS / 12.09 ms per frame, 40,000 live finite prisms, no GL errors. Normal operation remains capped at 60 FPS.
- Real demo device playback with speaker output muted also passed: 17 transients in the three-second run, motion multiplier 0.937–3.469, finite GPU particles and no GL errors.

Previous version 1.1 app and source archives are retained in `releases/1.1.0/`.

## Version 1.1 — crimson and optimisation

Measured before and after using the same 1,280 × 780 logical window on this Retina Mac, the same particle counts and deterministic audio signal. `--benchmark` disables VSync and the 60 FPS cap, excludes startup preparation, and waits for GPU completion each frame. Captures were disabled during timing.

| Scene | Before | After | Speedup |
|---|---:|---:|---:|
| Default, 40,000 prisms, 180 frames | 22.02 ms / 45.42 FPS | 12.27 ms / 81.50 FPS | 1.79× |
| “In the thick”, 100,000 prisms + depth of field, 120 frames | 66.35 ms / 15.07 FPS | 29.94 ms / 33.40 FPS | 2.22× |

Normal operation remains capped at 60 FPS. The updated interface capture reported 57 FPS while the counter was settling. Results depend on the scene and hardware.

Changes verified:

- Replaced the 18-noise finite-difference curl with three analytic simplex gradients. A GPU check against the preserved scalar noise at 512 deterministic points passed: mean normalised derivative error 0.000651, maximum 0.049393 with a 0.002 numerical step.
- Ambient occlusion and optional depth of field render at logical resolution, followed by a hardware linear blit to the Retina framebuffer. UI text remains at full output resolution. No particle-count reduction was used for the comparison.
- Cached uniform locations; the optional lens framebuffer is allocated only when depth of field is used.
- Added Deep crimson, matched the interface and spectrum colours, tinted highlights and kept audio colour modulation multiplicative. Motion presets retain the selected palette.
- Rebuilt with no application-source warnings. Existing audio/decoder tests passed.
- Slab, sphere, torus, capsule and wave GPU checks passed with finite live particles and no GL errors. The 100,000-prism depth-of-field case also passed.
- Visually inspected `captures/crimson-interface.png`; the updated torus export is `captures/crimson-torus.png`.

Previous version 1.0 app and source ZIPs are retained in `releases/1.0.0/`.

## Version 1.0 — original implementation checks

Built on this Apple Silicon Mac with AppleClang 21, C++17, CMake 4.3 and the bundled raylib 5.5 / GLFW / miniaudio sources. The final application source compiles without warnings; the initial dependency configuration reports raylib's existing macOS OpenGL deprecation notice.

## Audio checks passed

`build-native/prism_tests /tmp/prism-fixture.mp3 /tmp/prism-fixture.flac`

- Silence produces zero levels and no onsets.
- 82 Hz, 1 kHz and 6.5 kHz tones energise their expected frequency regions.
- FFT peak frequency is correct at both 44.1 and 48 kHz.
- Opposite-polarity stereo retains its energy.
- RMS of a 0.2-amplitude sine matches 0.1414 within tolerance.
- Increasing sensitivity preserves or increases response.
- Meters release to zero after input stops.
- Queue overflow drops new frames while preserving the 65,536 unread frames.
- A generated stereo PCM WAV decodes and seeks accurately while preserving polarity.
- One-second MP3 and FLAC conversions both decode to actual nonzero audio samples and report the expected duration.
- All 18 palette schemes generate finite values; all 16 original presets are present.

Real-device demo playback was exercised with the speaker output muted. Peak analysed RMS was 0.0908. Muted playback of the generated WAV through the actual file/device callback path also passed, with peak analysed RMS 0.2591.

## GPU checks passed

The validation reads transform-feedback positions back from the GPU, verifies that they are finite and that the active population is alive, then checks `glGetError()`.

| Scene | Live prisms | GL errors |
|---|---:|---:|
| Default slab | 40,000 / 40,000 | 0 |
| Original “In the thick”, depth of field enabled | 100,000 / 100,000 | 0 |
| Original “Macros”, torus | 56,000 / 56,000 | 0 |
| Wave sheet | 40,000 / 40,000 | 0 |
| WAV playback integration | 40,000 / 40,000 | 0 |

The interface and exported PNGs were visually inspected. The corrected default 1,280 × 780 window fits the screen, with sharp Retina text and a logical-resolution scene pass. The default scene reported approximately 38 FPS in the captured run; frame rate depends on the preset, count, depth of field and window size.

The packaged app was also opened independently of the build directory. Its native Open panel loaded the generated WAV, the track name and one-second duration appeared correctly, and its native Save panel produced `captures/native-export.png`: a clean 2,560 × 1,560 Retina image with no controls. The app was restarted into its silent initial preview after this check.

The reference effect, native interface, torus and depth-of-field captures were visually inspected. Superseded version 1.0 automatic QA captures were removed to reclaim disk space; current crimson and response captures remain in `captures/`. PNG export uses the native save dialog, and the test capture option writes to the exact requested absolute path.

Live microphone/interface/loopback capture is implemented and requests macOS permission on selection. It has not been hardware-tested in this session. Windows and Linux have not been built or tested.

The standalone macOS app is ad hoc signed; `codesign --verify --deep --strict` passed. Its linked dependencies are macOS system libraries/frameworks only.
