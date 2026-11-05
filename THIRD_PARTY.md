# Source and credits

Prism Currents is a native C++ audio-reactive adaptation of **Prism Currents by Liam Egan (shubniggurath)**:
https://codepen.io/editor/shubniggurath/pen/01a164e2-494a-79a0-948d-59baebda2dc9

The original downloaded JavaScript and GLSL files are preserved in `reference/`.
The native renderer retains its transform-feedback simulation, instanced prisms,
28 routes, cosine palettes, 16 presets, lighting, outlines, screen-space ambient
occlusion and optional depth of field. Adapted shaders are in `assets/shaders/`.
The reference was downloaded on 4 November 2026. [CodePen licenses public Pens
under MIT](https://blog.codepen.io/documentation/licensing/). The notice for the
original Pen and the portions adapted from it is in
[`assets/shaders/PRISM-CURRENTS-LICENSE.txt`](assets/shaders/PRISM-CURRENTS-LICENSE.txt).

Version 1.1 adds a native Deep crimson palette, analytic differentiation of
the original simplex polynomial, cached uniforms and logical-resolution
post-processing followed by a Retina framebuffer blit. The original scalar
noise is retained for GPU numerical validation.

- The original simplex noise implementation credits Ashima Arts / Stefan Gustavson (MIT). Its license notice is preserved in `assets/shaders/NOISE-LICENSE.txt`.
- Container distance functions and cosine palette methods: Inigo Quilez, as credited by the reference.
- Depth-of-field approach: Dennis Gustafsson, as credited by the reference.
- raylib 5.5 and bundled GLFW: zlib / permissive licenses preserved under `vendor/raylib`.
- miniaudio and its bundled dr_wav, dr_mp3 and dr_flac decoders: public domain or MIT-0, license text included at the end of `vendor/raylib/src/external/miniaudio.h`.
- Karla and Cousine fonts: SIL Open Font License 1.1, included in `assets/fonts/OFL.txt`.
