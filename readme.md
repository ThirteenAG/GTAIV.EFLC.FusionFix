> [!IMPORTANT]
> Want to help release the next version of Fusion Fix? Past few months a lot of work was done by @Parallellines0451 and other contributors to bring volumetric fog feature to Fusion Fix, along with other things, and I (@ThirteenAG) am having troubles finishing my remaining tasks(due to some irl circumstances) to complete the next major build. I decided to start a crowdfunding campaign to help me make this release faster. The end goal of 1500$ would allow me to literally "drop everything" and focus on finishing up of what we started implementing in the beginning of 2025.
>
> Current changelog:
>- Added volumetric fog
>- Added radial depth of field
>- Added smoother light volumes
>- Added tone mapping
>- Fixed night shadow bias and restored console softness
>- Fixed water reflection offset
>- Fixed water artifacts visible from high altitudes
>- Adjusted corona view distance in paraboloid reflections
>- Improved "PC+" tree lighting
>- Improved sun shafts mask
>- Improved console gamma curve
>- Restored visualsettings rain.defaultlight functionality
>- Restored console rain density
>- Reduced procobj pop-in at higher settings
>- Added a PC style tree lod atlas
>- Added a timed events toggle
>- Fixed a crash in the TLAD benchmark sequence
>
> There might be other additions to this later, because it's unclear at the moment what's possible to implement (e.g. moving during sniper zoom).
>
> If there's no interest in helping me out, the release will still happen at some point, I just can't tell exactly when at the moment.
> Thank you!
>
>  Make a donation: <a target="_blank" href="https://github.com/sponsors/ThirteenAG">GitHub Sponsors</a> | <a
        target="_blank" href="https://ko-fi.com/thirteenag">Ko-Fi</a> | <a target="_blank"
        href="https://paypal.me/SergeyP13">PayPal</a> | <a target="_blank"
        href="https://www.patreon.com/ThirteenAG">Patreon</a> | <a target="_blank"
        href="https://boosty.to/thirteenag/donate">Boosty</a>
>
> Progress: 246 / 1500$
>
> Thanks to all who've donated so far, it's been a great help and we're making progress!

[![Actions Status: Release](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/actions/workflows/msvc_x86.yml/badge.svg)](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/actions)
[![AppVeyor](https://img.shields.io/appveyor/build/ThirteenAG/GTAIV-EFLC-FusionFix?label=AppVeyor%20Build&logo=Appveyor&logoColor=white)](https://ci.appveyor.com/project/ThirteenAG/gtaiv-eflc-fusionfix)

<p align="center">
  <img width="400" src="https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/assets/4904157/f45efc2e-f4f5-4d4d-847b-0f0d19897ef4.png">
</p>

# GTAIV.EFLC.FusionFix

This projects aims to fix some issues in Grand Theft Auto IV: The Complete Edition.

Also available for [Max Payne 3](https://github.com/ThirteenAG/MaxPayne3.FusionFix#readme) and [other games](https://thirteenag.github.io/wfp).

![](https://github.com/user-attachments/assets/f9c6c978-3b19-422a-b561-4cf31716620b)

## Installation:

> [!NOTE]
> Install Grand Theft Auto IV: The Complete Edition (v1.2.0.30 and above required)
>
> **Download**: [GTAIV.EFLC.FusionFix.zip](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/releases/latest/download/GTAIV.EFLC.FusionFix.zip)
>
> Unpack content of the archive to your **GTAIV: The Complete Edition** root directory, where the exe is located.

> [!WARNING]
> Non-Windows users (Proton/Wine) need to perform a [DLL override](https://cookieplmonster.github.io/setup-instructions/#proton-wine).
> 
> Additionally:
> * Install and run Protontricks.
> * Select "Grand Theft Auto IV" -> "Select the default Wineprefix" -> "Install a Windows DLL or component".
> * Search for "d3dx9_43", select it and click OK.

> [!IMPORTANT]
> This fix was tested only with latest official update and latest [ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest/download/Ultimate-ASI-Loader.zip) (included in the archive).

## Donation links

<div align="left">
<a href="https://github.com/sponsors/ThirteenAG"><img src="https://img.shields.io/badge/Sponsor_on_GitHub-5c5c5c?style=for-the-badge&logo=github&logoColor=white" width="250"></a><br>
<a href="https://ko-fi.com/thirteenag"><img src="https://img.shields.io/badge/Support_on_Ko--Fi-ff5e5b?style=for-the-badge&logo=kofi&logoColor=white" width="250"></a><br>
<a href="https://paypal.me/SergeyP13"><img src="https://img.shields.io/badge/Donate_with_PayPal-009cde?style=for-the-badge&logo=paypal&logoColor=white" width="250"></a><br>
<a href="https://www.patreon.com/ThirteenAG"><img src="https://img.shields.io/badge/Support_on_Patreon-ff424d?style=for-the-badge&logo=patreon&logoColor=white" width="250"></a><br>
<a href="https://boosty.to/thirteenag/donate"><img src="https://img.shields.io/badge/Support_on_Boosty-f15e2d?style=for-the-badge&logo=data:image/png;base64,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&logoColor=white" width="250"></a>
</div>

---

### Core/Gameplay Changelog

### New menu options

- **Skip Intro**, added an option to skip intro
- **Skip Menu**, added an option to skip main menu after startup
- **Borderless**, added an option to switch between windowed and borderless modes
- **Motion Blur**, separate toggle for motion blur
- **Bloom**, separate toggle for bloom
- **FPS Limit**, set a custom FPS limit, select from a list of predefined values, or turn it off
- **Antialiasing**, a toggle for FXAA or SMAA ([comparison](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/assets/4904157/6104ec1f-9e5a-46c4-891a-6a5e37d85f86))
- **Sun Shafts**, a godrays implementation faithful to the original art style
- **Console Gamma**, emulates consoles' contrasted look
- **Screen Filter**, an option to change color filters in main game and episodes
- **Distant Blur**, controls how intense the distant blur is during gameplay
- **Depth Of Field**, toggles the effect globally
- **Tree Lighting**, offers "PC", "PC" with fixed orange glow ("PC+") and "Console" style tree lighting
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
- Added sun shafts
- Added various fixed animations and vehicle models and lamppost coronas
- Fixed glass shards lacking colors

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

- Added options to customize shadow blur, bias, cascade blending and filter quality
- Added options to toggle lamppost and headlight shadows
- Added an option to toggle vehicle night shadows
- Added an option to enable extra dynamic shadows
- Added an option to enable dynamic shadow for trees
- Increased shadow render distance to pre 1.0.6.0 levels
- Fixed cascaded shadowmap resolution

### Episodic content

Note: most of this content requires additional modifications by the end user to the game, in order to fully use these features.
- Added an option to enable support for APC and buzzard and all their abilities in IV and TLaD
- Added an DSR1, pipe bomb, sticky bomb, AA12 explosive shells, P90 vehicle check, partially parachute in IV, TLaD and TBoGT
- Added an option to enable explosive rounds on annihilator
- Added an option to enable camera bobbing in clubs, cell phone switching, altimeter in helicopters and parachute, explosive sniper and fists cheats
- Added an option to raise height limit for helicopters in base game and TLad to match TBoGT
- Added an option to give P90 and AA12 to SWAT and FIB and M249 to police in helicopters
- Added an option to disable SCO signature check

### Misc

- Added an option to customize tree alpha
- Added an option to customize corona reflection intensity
- Added an option to fix autoexposure
- Fixed grass height
- Fixed camera stuttering while sprinting and turning with a controller
- Fixed rifle firing delay
- Added an option to fix rain droplets rendering
- Improved phone screen resolution
- Various other fixes, like LOD lights appear at the appropriate time like on the console version
- Restored auto exposure from consoles

### Details

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

### Shaders Changelog

**Special thanks to [Parallellines0451](https://github.com/Parallellines0451) [AssaultKifle47](https://github.com/akifle47), [RaphaelK12](https://github.com/RaphaelK12), [robi29](https://github.com/robi29) and [\_CP_](https://github.com/cpmodding) for directly contributing with fixes, to [Shvab](https://github.com/d3g0n-byte) for making RAGE Shader Editor.**

**Fusion Fix prevents [certain dlls from injecting into the game's process, to avoid issues](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/blob/master/source/dllblacklist.ixx#L15).**

#### General
- Fixed z-fighting
- Fixed object pop-in caused by version 1.0.6.0
- Improved screen door transparency
#### Lighting
- Fixed volumetric lights occlusion
- Restored console foliage translucency
- Fixed orange glow under trees
- Fixed mismatched intensity of shadow casting lights causing visible pop-in
- Fixed lights that were made invisible with version 1.0.6.0
- Fixed black normal map halos on several surfaces such as asphalt, sidewalks and rocks
#### Shadows
- Restored rotated disk filter from versions prior to 1.0.6.0
- Fixed large shadow artifacts visible from high altitudes
- Fixed shadowmap being erroneously blurred before the lighting pass
- Fixed shadows stretching at certain camera angles
- Fixed shadow view distance being lower than the actual rendered distance
- Fixed cutoff penumbras of distant shadows
- Restored normal offset bias from versions prior to 1.0.6.0
- Added contact hardening shadows
- Fixed disconnected night shadows
- Fixed blur artifacts between shadow cascades under some conditions
- Added "pseudo" shadow cascade blending to reduce the disparity between cascades
- Fixed pitch black static vehicle shadows
- Added wind sway for dynamic tree shadows
- Fixed incorrectly offset shadows on water
- Fixed flickering when shadows of transparent objects overlap
- Added parameters to control shadow softness and bias
#### Post processing
- Split depth of field, motion blur and stippling filter into separate passes to prevent overlap
- Fixed color banding, most noticeably in the sky
- Added a mask to selectively filter screen door transparency
- Fixed depth of field and bloom not scaling correctly at resolutions higher than 720p
- Improved bloom weights and stability in motion
- Fixed excessively blurry screen compared to consoles caused by leftover anti aliasing code
- Restored console bloom and auto exposure
- Fixed flickering auto exposure
- Fixed motion blur losing intensity at high framerates
- Fixed incorrect TLAD noise tiling on water quality levels other than medium
- Fixed TLAD noise aspect ratio
- Added a console-like gamma toggle
#### Reflections
- Restored console behavior for tree and terrain reflections
- Fixed excessive specularity of several meshes in Alderney
- Fixed distorted vehicle reflections
- Restored corona depth test in water reflections
- Added a parameter to control corona reflection intensity
- Fixed anisotropic filtering affecting vehicle reflection intensity
- Restored console vehicle reflection behavior relative to dirt level
- Restored console environment reflection intensity
- Fixed distorted mirror reflections at certain camera angles
- Restored console mirror blur
#### Particles
- Fixed soft particles
- Fixed stuttery particle animations caused by version 1.0.5.0
- Fixed rain being almost invisible, especially at night
- Fixed rain streaks becoming shorter at high framerates
#### Water
- Fixed flickering outlines around shores and thin objects
- Fixed invisible water foam
- Improved shallow water transparency and improved foam texture
- Fixed incorrect water texture tiling on quality levels other than medium
- Fixed flat, mirror-like water surface on AMD graphics cards
#### Misc
- Fixed outlines around objects when using DXVK
- Partially restored console object fade speed
- Fixed terrain pop-in
- Reduced procobj pop-in for the default view and detail distance values
- Partially fixed building windows visible near the far plane if emissive depth write is disabled
- Restored fence mipmap bias from versions prior to 1.0.6.0
- Fixed incorrect texture filtering used in several shaders
- Added a parameter to control the alpha threshold of tree leaves
- Added support to control vehicle dirt color, rain streak length and textures
- Added AO to gta_normal_spec_reflect_emissive if emissivity is 0 so it can be used to disable night shadows for certain objects
- Partially fixed mirror depth
- Added support to instantaneously turn lamppost bulbs on or off

# Contributing

If you have an idea for a fix, add a module with its implementation to [source](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/tree/master/source) directory and open a pull request. See [contributing.ixx](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/blob/master/source/contributing.ixx) for reference.

# Reporting more issues

We're making a list of **GTA IV** issues that weren't addressed in official patches.

If you can think of one, submit it [here](https://github.com/GTAmodding/GTAIV-Issues-List/issues).

If you've encountered an issue, caused by this fix, report it [here](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/issues).
