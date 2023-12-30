[![Actions Status: Release](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/actions/workflows/msvc_x86.yml/badge.svg)](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/actions)
[![AppVeyor](https://img.shields.io/appveyor/build/ThirteenAG/GTAIV-EFLC-FusionFix?label=AppVeyor%20Build&logo=Appveyor&logoColor=white)](https://ci.appveyor.com/project/ThirteenAG/gtaiv-eflc-fusionfix)

<p align="center">
  <img height="200" src="https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/assets/4904157/f45efc2e-f4f5-4d4d-847b-0f0d19897ef4.png">
</p>

# GTAIV.EFLC.FusionFix

This projects aims to fix some issues in Grand Theft Auto IV: Complete Edition. Also available for [Max Payne 3](https://github.com/ThirteenAG/MaxPayne3.FusionFix#readme).

![](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/assets/4904157/ba388163-c26a-41ae-8378-bef4037c4fd7)

## Installation:

> [!NOTE]
> Install Grand Theft Auto IV: The Complete Edition (v1.2.0.30 and above required)
>
> **Download**: [GTAIV.EFLC.FusionFix.zip](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/releases/latest/download/GTAIV.EFLC.FusionFix.zip)
>
> Unpack content of the archive to your **GTAIV Complete Edition** root directory.

> [!WARNING]
> Non-Windows users (Proton/Wine) need to perform a [DLL override](https://cookieplmonster.github.io/setup-instructions/#proton-wine).

> [!IMPORTANT]
> This fix was tested only with latest official update and latest [ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest/download/Ultimate-ASI-Loader.zip) (included in the archive).

---

### Core/Gameplay Changelog

### New menu options

- **Skip Intro**, added an option to skip intro
- **Skip Menu**, added an option to skip main menu after startup
- **Borderless**, added an option to switch between windowed and borderless modes
- **Motion Blur**, separate toggle for motion blur
- **Bloom**, separate toggle for bloom
- **Definition**, now expanded with more options, "Classic", "Improved" and "Extra", more details below
- **Shadow Filter**, offering a variety of choices such as "Sharp", "Soft", "Softer", "Softest", and "PCSS"
- **Console Shadows**, disables headlights shadows in exchange for vehicle shadows from non-sun light sources
- **FPS Limit**, set a custom FPS limit, select from a list of predefined values, or turn it off
- **Antialiasing**, a toggle for FXAA or SMAA
- **SSAA**, a toggle to enable supersampling, requires restart, affects performance
- **Console Gamma**, emulates consoles' contrasted look
- **Screen Filter**, an option to change color filters in main game and episodes
- **Distant Blur**, controls how intense the distant blur is during gameplay
- **Depth Of Field**, toggles the effect globally
- **Tree Lighting**, offers "PC", "PC" with vertex AO ("PC+") and "Console" style tree lighting
- **Always Run**, allows to run by default, like in classic GTA games
- **LightSyncRGB**, only Logitech hardware is supported, requires Logitech G HUB app

  ![LightSyncRGB](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/assets/4904157/f925ad27-19ce-4dde-8801-0cec1578ade7)

  [**Watch full clip on YouTube**](https://www.youtube.com/watch?v=oLxn3q-NnZ0&hd=1)

- **Cover Centering**, added an option to disable camera centering in cover
- **Raw Input**, raw input for menu and adjustments for in-game camera mouse look
- **Alternative Dialogues**, when enabled, forces the game to use alternative dialogues in some missions
- **Centering Delay**, sets the time before camera starts autocentering on foot
- **FOV**, adjusts in-game field of view
- **Letterbox**, draws cutscene borders on top and bottom of the screen
- **Pillarbox**, draws cutscene borders on screen sides in ultrawide
- **Gamepad Icons**, allows to select various controller icon styles (Xbox 360, Xbox One, PS3, PS4, PS5, Nintendo Switch, Steam Deck)

### Main

- [Fixed recoil behavior that was different when playing with keyboard/mouse and gamepad](https://github.com/GTAmodding/GTAIV-Issues-List/issues/6)
- [Fixed forced "definition off" setting in cutscenes, now cutscenes will respect the menu setting](https://github.com/GTAmodding/GTAIV-Issues-List/issues/5)
- [Fixed DLC car lights in TBoGT](https://github.com/GTAmodding/GTAIV-Issues-List/issues/3)
- [Added various frame limiting options to address fps related issues](https://github.com/GTAmodding/GTAIV-Issues-List/issues/1)
- [Fixed aiming zoom in TBoGT](https://github.com/GTAmodding/GTAIV-Issues-List/issues/19)
- [Added an option to fix a bug that only make taxis spawn](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/issues/85)
- [Added a file overload solution that allows to replace game files without actually replacing them](https://github.com/ThirteenAG/Ultimate-ASI-Loader?tab=readme-ov-file#update-folder-overload-from-folder)
- Added IMG Loader (from update folder)
- Increased corona limit to avoid heavy flickering of game's lights
- Improved ultrawidescreen support

### Scripts

- Restored transparency on pool minigame
- Restored traffic cops in toll booths
- Restored non-cop pedestrians in helicopters flying around city
- Fixed Johnny's incorrect voice lines when exiting the gun shop after buying something (used to use voice lines from Higgin's Heli Tours)
- Restored Luis' voice lines when exiting the gun shop (buying/not buying)
- Restored missing voice lines from Luis & Johnny when exiting the Broker gun shop
- The gun shop owner now uses his unique model (M_M_GunNut_01) instead of the strip club bouncer model (M_Y_bouncer_01).
- The Desert Eagle and Micro SMG are now using their unique sounds when viewing them (required modifying the animation dictionary "missgunlockup.wad")
- Restored the temporary ban from the gun shop when attacking the owner in TLAD/TBOGT
- Chinatown gun shop: Fixed a small bug where the wrong voice line is used after exiting when not buying something
- Restored voice lines from Luis, he'll now respond to what the pilot tells him about the landmarks during the tour
- Workaround for the garage door bug on high framerates, the garage door only closes now when Niko is actually outside of the garage
- Fixed the heli climb QTE being impossible on really high framerates
- Workaround for a bug where Niko dies after being kicked off of Dimitri's helicopter and falling into the water (Deal Ending)
- Workaround for a bug where Jacob crashes the Annihilator on high framerates (Deal Ending)

### Shadows

- Added an option to fix flickering shadows
- Added an option to enable extra dynamic shadows
- Added an option to enable dynamic shadow for trees
- Added an option to fix cascaded shadowmap resolution

### Misc

- Added an option to fix rain droplets rendering
- Added an option to customize rain droplets blur intensity
- Various other fixes, like LOD lights appear at the appropriate time like on the console version

### Details

- **Definition** - controls the behavior of the game's stippled transparency filter. "Classic" is less blurry than PC and provides console parity. "Improved" applies the filter only to stippled objects instead of the whole screen, [like in GTA V](http://www.adriancourreges.com/blog/2015/11/02/gta-v-graphics-study/#gtav_dithering). "Extra" is the same as "Improved" but stippled transparency is removed from vegetation and fences, making them sharper.

- **AimingZoomFix** - set to **1** for proper fix, so it behaves like on xbox, set to **2** to have this fixed feature enabled in IV and TLAD, set to **-1** to disable this feature. **0** disables the fix, as usual.

- **FpsLimit** - enter a desired value to specify the number of frames per second you want for the game. Like 30, 45, 60 or more. This will affect all gameplay.

- **CutsceneFpsLimit** - enter a desired value to specify the number of frames per second you want for the game to use on cutscenes only.

- **ScriptCutsceneFovLimit** - this is the minimum fov value the game can set during script cutscenes, [like this one](https://www.youtube.com/watch?v=NzKw7ijHG10&hd=1). It prevents the game to zoom in too much on high fps. Since it's not a proper fix for the issue, you may disable it and use _hidden_ setting **ScriptCutsceneFpsLimit** that works the same way as **CutsceneFpsLimit**.

- **DefaultCameraAngleInTLAD** - TLAD uses a different camera angle on bikes, this option can force the original IV camera angle. [Enabled](https://i.imgur.com/PqFHJfU.jpg) / [Disabled](https://i.imgur.com/5kM5Sgn.jpg)

- **PedDeathAnimFixFromTBOGT** - when you perform a counter attack after a dodge in IV & TLAD, the ped after falling and dying performs an additional death animation, which doesn't happen in TBOGT. [Enabled](https://imgur.com/EYsiGPe) / [Disabled](https://imgur.com/CR3LEdR)

- **DisableCameraCenteringInCover** - [see issue 20](https://github.com/GTAmodding/GTAIV-Issues-List/issues/20).

- **VehicleBudget** - allows to increase the budget of vehicles.

- **PedBudget** - allows to increase the budget of pedestrians.

- **LightSyncRGB** - custom ambient lighting for IV, TLAD and TBOGT, health indication on G-Keys, police lights, ammo counter.

### Graphics Changelog

**Special thanks to [Parallellines0451](https://github.com/Parallellines0451) [AssaultKifle47](https://github.com/akifle47), [RaphaelK12](https://github.com/RaphaelK12), [robi29](https://github.com/robi29) and [\_CP_](https://github.com/cpmodding) for directly contributing with fixes, to [Shvab](https://github.com/d3g0n-byte) for making RAGE Shader Editor.**

### General
- Fixed z-fighting by implementing a logarithmic depth buffer
- Fixed LOD pop-in
- Removed unnecessary stippled transparency from hundreds of shaders
- Added 256 tile stipple for smoother transparency and LOD transitions
### Lighting
- Fixed volumetric light shafts occlusion
- Ported console foliage translucency
- Fixed orange glow under trees
- Added tree vertex AO toggle
- Fixed light dimming/pop-in
### Shadows
- Ported 1.0.4.0 shadow filter and improved its sample count
- Improved night shadow filter
- Fixed broken shadows following the player at high altitudes
- Fixed broken shadows cast by finely detailed objects like fences
- Added percentage closer soft shadows
- Fixed disconnected shadows/peter panning and implemented slope scale depth bias
- Reduced shadow cascade disparity
- Slightly improved shadow fadeout
- Fixed excessively strong vertex AO and static vehicle shadows
- Added wind sway for tree shadows
- Fixed improper water shadow stretching
- Added experimental support for simultaneous headlight and vehicle shadows
### Post processing
- Added a mask for the dithering filter to only smooth out stippled objects instead of the whole screen
- Fixed depth of field and bloom resolution scaling
- Fixed excessively blurred screen compared to consoles
- Added native anti aliasing
- Ported console bloom and auto exposure
- Fixed motion blur framerate scaling
- Increased motion blur quality (reduced noisiness)
- Split depth of field and motion blur into dedicated settings
- Restored console TLAD noise effect
- Restored console gamma
### Reflections
- Fixed excessive wetness/specularity of various surfaces
- Fixed blocky vehicle reflections
- Reduced corona reflection intensity
- Fixed anisotropic filtering affecting reflection intensity
- Removed influence of vehicle.ide on vehicle reflection intensity
- Increased global reflection intensity to match consoles
- Fixed distorted mirror reflections at certain camera angles
- Restored console mirror filter
### Particles
- Fixed particle seams/restored soft particles
- Improved rain visibility
- Fixed rain framerate scaling
### Water
- Fixed water texture tiling
- Fixed textureless water on AMD graphics cards
### Misc
- Improved mirror depth/placement
- Fixed invisible gta_emissivestrong lights like in the Rotterdam Tower
- Improved window crossfade
- Added lamppost on/off toggle support
- Added optional opaque wires and bridge cables
- Added a new shader exclusively for trees

# Contributing

If you have an idea for a fix, add a module with its implementation to [source](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/tree/master/source) directory and open a pull request. See [contributing.ixx](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/blob/master/source/contributing.ixx) for reference.

# Reporting more issues

We're making a list of **GTA IV** issues that weren't addressed in official patches.

If you can think of one, submit it [here](https://github.com/GTAmodding/GTAIV-Issues-List/issues).

If you've encountered an issue, caused by this fix, report it [here](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/issues).
