# Distant Lights

The Game tab places **Distant Lights** directly below **Extra Night Shadows** in both the pause and title menus.

| Option | Behavior |
| --- | --- |
| Default (0) | Native PC distant streetlights and traffic models, with the streetlight lookup corrected. |
| Console (1) | Xbox 360 streetlight paths and moving traffic sprites, with the streetlight lookup corrected. Native PC road lights and traffic models are suppressed during this mode. |

Default is the initial value and the value selected by all presets. Mode changes apply live. The setting is stored as `[MISC] DistantLights` in `GTAIV.EFLC.FusionFix.cfg` through the shared settings API.

## Data

Console reads the original big-endian Xbox file from `pc/data/paths/distantlights_360.dat` under the active Fusion Overloader root, or the game's `update` directory if no root is supplied. The native PC `distantlights.dat` remains unchanged and loaded. Authored path gaps are retained.

The included Xbox IV/EFLC data is 19,736 bytes, SHA-256 `81562cfc0f7d5b4477bcbbdd8245aa5bc32889d0f59203bcf5f538344c3c7148`.

The separate loader validates file size, counts, block partitions, bounds and point ranges. Missing or invalid Xbox data falls back to Default rendering. A load is attempted once; restart after adding or replacing the file. The filename must be available as a loose file under the resolved root.

## Rendering

Inland streetlights and traffic in both modes fade smoothly with the camera's height above each light or imposter car. They are invisible when the camera is level with or below them, half faded in 50 metres above them and fully faded in 100 metres above them before other rendering effects. The reference includes path elevation and the placement offset (+12 for streetlights, +2 for Console traffic, +0.6 for PC imposter cars). This height fade and the native near-distance fade use whichever is dimmer, without multiplying them together. The native distance transition remains 150-300 metres for PC streetlights, 150-250 metres for Console sprites and 250-325 metres for PC imposter traffic. Coastal paths and aircraft have no added height fade.

The `distantlights.ixx` module bypasses the PC streetlight world-height cutoff and adjusts the distance passed to the native streetlight colour routine for inland paths. Native placement, random sampling and colour rounding are retained. The separate override in `lodlights.ixx` remains commented out.

Default imposter traffic uses a per-car height limit on the native instance opacity in both day and night rendering. The inland world-height cutoff is bypassed in both passes. Model assets, movement, path direction, coastal visibility and distance fading remain native.

Console traffic uses opposing white/red streams, 33/45.5-unit spacing, 43/57-ms timer divisors and path Z+2. Streetlights use 30-unit spacing and path Z+12. Whole streetlight paths follow staggered dusk/dawn thresholds. The corrected lookup wraps negative coordinate indices into the original 16-entry table on both platforms. Native PC streetlight spacing remains 60 units.

Console culling uses block spheres and stored path spheres, then the native Xbox near-distance test. The installed PC graphics pipeline renders the sprites using Console texture orientation and the normal shader filtering settings. The shared game random generator supplies the Xbox range and rounding for road-light variation.

Native aircraft orbit positions, colors and flashing are reused. Console draws roads and aircraft in separate batches. Roads remain full size in every episode; TBoGT aircraft and flashes use half size, while IV/TLAD aircraft use full size. The renderer detects whether FusionFix has bypassed the native aircraft-halving branch and normalizes the incoming basis accordingly. Default retains upstream aircraft rendering.

The native PC road tables are temporarily suppressed only during Console submission and restored on return. Data arrays are never replaced. Hook installation requires unique signatures in the inspected Complete Edition layout; unavailable bindings retain PC rendering.

## Validation

Run `python tests/verify-menu.py` and the upstream ASI settings validator. From a Win32 Visual Studio developer prompt, compile the three C++ regression programs with C++20 or later and assertions enabled. Run `distantlights` with the supplied Xbox DAT, `pc-distantlights` with an original PC DAT, and `distantlights-render` without arguments.

The renderer regression covers both native and FusionFix aircraft branch states, empty and full 64-sprite boundaries, no mixed road/aircraft batches, independent TBoGT sizes, no lost/duplicated submissions, Console UVs, and Default renderer delegation. In-game checks should cover live menu switching, saved values, missing-data fallback, camera-height boundaries, all episodes, and the active graphics pipeline.
