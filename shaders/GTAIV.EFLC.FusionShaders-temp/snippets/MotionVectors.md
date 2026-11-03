# Deferred motion vectors

Deferred material shaders write the motion next to the stencil on `COLOR3`, the game's `_STENCIL_BUFFER_` target,
which the runtime creates as A16B16G16R16F: R keeps the stencil value, G and B hold current minus previous position in
texture coordinates, A keeps the stencil write's own value. Both positions exclude projection jitter. The other G-buffer
outputs and depth tests are unchanged.

The runtime supplies these registers before each deferred geometry draw:

| Stage | Registers | Value |
|---|---|---|
| VS | c240–c243 | Previous object-to-clip matrix, without jitter; same row-vector layout as c8–c11 |
| VS | c244 | `(1 / 144, bone atlas row centre, 0, 0)` |
| VS | c245 | `(previous bones valid, 0.5, 1.5, 2.5)` |
| VS | c246 | Previous frame's tree wind constant (c233) |
| PS | c200 | `(1 / width, 1 / height, -jitterX / width, -jitterY / height)` |
| PS | c201 | `(0.5, -0.5, 0.5, 0)` |

Jitter is the content shift in pixels: positive X moves right, positive Y moves down.
Pixel shaders use Direct3D 9's integer-centred VPOS coordinates without an added half-pixel offset.
Previous bones use vertex sampler 2, a point-sampled RGBA32F atlas with 144 texels per draw (48 matrices, three rows
per matrix). A missing previous draw uses the current pose and camera reprojection.

`TEXCOORD8` carries the previous clip position. Tree billboards instead pack its X, Y and W into the unused W
components of `TEXCOORD5`, `TEXCOORD6` and `TEXCOORD7`, preserving the pixel shader's ten-input limit.

The effect XML files continue to use their existing shader entries and techniques. Forward effects, water, shadows
and post processing don't write motion.
