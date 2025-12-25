[![Actions Status: Release](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/actions/workflows/msvc_x86.yml/badge.svg)](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/actions)
[![AppVeyor](https://img.shields.io/appveyor/build/ThirteenAG/GTAIV-EFLC-FusionFix?label=AppVeyor%20Build&logo=Appveyor&logoColor=white)](https://ci.appveyor.com/project/ThirteenAG/gtaiv-eflc-fusionfix)

<p align="center">
  <a href="https://fusionfix.io/iv" target="_blank"><img width="600" src="https://raw.githubusercontent.com/ThirteenAG/GTAIV.EFLC.FusionFix/refs/heads/master/installer/ff_iv_outline.svg"></a>
  <br />
  <a href="https://discord.gg/2ckFCS572Z" target="_blank"><img width="200" src="https://raw.githubusercontent.com/ThirteenAG/GTAIV.EFLC.FusionFix/refs/heads/master/installer/discord.svg"></a>
  &nbsp;&nbsp;&nbsp;
  <a href="https://t.me/fusionfix" target="_blank"><img width="200" src="https://raw.githubusercontent.com/ThirteenAG/GTAIV.EFLC.FusionFix/refs/heads/master/installer/telegram.svg"></a>
  &nbsp;&nbsp;&nbsp;
  <a href="https://www.youtube.com/@FusionFix10" target="_blank"><img width="200" src="https://raw.githubusercontent.com/ThirteenAG/GTAIV.EFLC.FusionFix/refs/heads/master/installer/youtube.svg"></a>
  &nbsp;&nbsp;&nbsp;
  <a href="https://x.com/fusionfix10" target="_blank"><img width="200" src="https://raw.githubusercontent.com/ThirteenAG/GTAIV.EFLC.FusionFix/refs/heads/master/installer/x.svg"></a>
  &nbsp;&nbsp;&nbsp;
  <a href="https://fusionfix.io/iv" target="_blank"><img width="200" src="https://raw.githubusercontent.com/ThirteenAG/GTAIV.EFLC.FusionFix/refs/heads/master/installer/website.svg"></a>
  &nbsp;&nbsp;&nbsp;
</p>

This projects aims to fix some issues in Grand Theft Auto IV: The Complete Edition, as well as add various new features for modern systems.

Also available for [Max Payne 3](https://github.com/ThirteenAG/MaxPayne3.FusionFix#readme) and [other games](https://thirteenag.github.io/wfp).

## Installation:

> [!NOTE]
> Install **Grand Theft Auto IV: The Complete Edition** from your library, then:
>
> * Download [GTAIV.EFLC.FusionFix.zip](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/releases/latest/download/GTAIV.EFLC.FusionFix.zip) and unpack its contents to the game's root directory, where the exe is located.
> * Or alternatively download [GTAIV.EFLC.FusionFixWebInstaller.exe](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/releases/latest/download/GTAIV.EFLC.FusionFixWebInstaller.exe) or [GTAIV.EFLC.FusionFixOfflineInstaller.exe](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/releases/latest/download/GTAIV.EFLC.FusionFixOfflineInstaller.exe), open it, select the game's directory and wait for it to finish. Note: If you get an error while installing, this is likely due to the installer not having write access. Go into the properties of the games directory and change your system's users to have write permission and hit apply, now installation should work. 
>

> [!WARNING]
> Non-Windows users (Proton/Wine) need to perform a **DLL override**.
>
> <details>
> <summary>Click here for details</summary>
> <br>
>
> You need to tell Wine explicitly that the `dinput8.dll` file is to be used. There's more than one way to achieve it. Note that Proton Experimental already loads it by default, so you may skip this step.
>
> ###
>
> **Method 1**: `WINEDLLOVERRIDES` variable lets you temporarily specify DLL overrides. It can be used from a command line as well as in the Steam launcher. In case of command line, simply prepend the usual start command with `WINEDLLOVERRIDES="dinput8=n,b"` . For Steam, head to game’s properties and set `LAUNCH OPTIONS` to `WINEDLLOVERRIDES="dinput8=n,b" %command%`.
>
>  ![steam-wine-dll-override](https://cookieplmonster.github.io/assets/img/setup/steam-wine-dll-override.png)
>
> **Method 2**: Use `winecfg` tool to make a permanent override for a specific Wine prefix. In case of Proton, Steam creates the Wine prefix for Grand Theft Auto IV: The Complete Edition in `$HOME/.steam/steam/steamapps/compatdata/12210/pfx`. Then you need to run `winecfg` with that path:
> ```
> WINEPREFIX="$HOME/.steam/steam/steamapps/compatdata/12210/pfx" winecfg
> ```
> Select `Libraries` tab and fill the combo box with the name of the library you wish to override and hit `Add`. You can verify that it’s been added to the list below with `(native, builtin)` suffix. Then close the window with `OK` button.
>
> ![winecfg-dll-override](https://cookieplmonster.github.io/assets/img/setup/winecfg-dll-override.png)
>
> Related Wine documentation:
> * [More on DLL overrides](https://wiki.winehq.org/Wine_User's_Guide#DLL_Overrides)
> * [More on WINEDLLOVERRIDES method](https://wiki.winehq.org/Wine_User's_Guide#WINEDLLOVERRIDES.3DDLL_Overrides)
> </details>
>
> Additionally:
> * Install and run Protontricks.
> * Select "Grand Theft Auto IV" -> "Select the default Wineprefix" -> "Install a Windows DLL or component".
> * Search for "d3dx9_43", select it and click OK.

> [!IMPORTANT]
> Only The Complete Edition is fully supported. Legacy versions of the game (e.g., v1.0.7.0 or v1.0.8.0) additionally require the installation of the [Legacy Addon](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/releases/latest/download/GTAIV.EFLC.FusionFixLegacyAddon.zip), which is done by unpacking its contents to the game's root directory.
>
> Multiplayer is not supported. [Take-Two has agreed that it generally will not take legal action against third-party projects involving Rockstar’s PC games that are single-player, non-commercial, and respect the intellectual property (IP) rights of third parties. This does not apply to (i) multiplayer or online services; (ii) tools, files, libraries, or functions that could be used to impact multiplayer or online services, or (iii) use or importation of other IP (including other Rockstar IP) in the project; or (iv) making new games, stories, missions, or maps.](https://support.rockstargames.com/articles/5NVOAYjcTomO8v6SX2k76k/pc-single-player-mods)
> Forking this project or creating any derivative software intended for use with multiplayer of any kind is forbidden. Any persons or entities who do so are not associated with this project.
> 
> No support will be provided for illegal copies of the game. Purchase an official copy from either [Steam](https://store.steampowered.com/app/12210/) or [Rockstar Games Launcher](https://store.rockstargames.com/game/buy-grand-theft-auto-iv).

## Donation links

<div align="left">
<a href="https://github.com/sponsors/ThirteenAG"><img src="https://img.shields.io/badge/Sponsor_on_GitHub-5c5c5c?style=for-the-badge&logo=github&logoColor=white" width="250"></a><br>
<a href="https://ko-fi.com/thirteenag"><img src="https://img.shields.io/badge/Support_on_Ko--Fi-ff5e5b?style=for-the-badge&logo=kofi&logoColor=white" width="250"></a><br>
<a href="https://paypal.me/SergeyP13"><img src="https://img.shields.io/badge/Donate_with_PayPal-009cde?style=for-the-badge&logo=paypal&logoColor=white" width="250"></a><br>
<a href="https://www.patreon.com/ThirteenAG"><img src="https://img.shields.io/badge/Support_on_Patreon-ff424d?style=for-the-badge&logo=patreon&logoColor=white" width="250"></a><br>
<a href="https://boosty.to/thirteenag/donate"><img src="https://img.shields.io/badge/Support_on_Boosty-f15e2d?style=for-the-badge&logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADIAAAA9CAYAAADvaTpkAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAALiIAAC4iAari3ZIAAAWCSURBVGhDtZpJjBVFHIdnBFRAAYMCBoO4RFwm4gLGZQiKGlwOatTgejAx6MWLBy4m4hJiYvQgiVEPRFFiRC8oB42RZQgDGDIs6rhHQQ2ERRAUiWzj96uqN5n35r3u2vpLvtS/t+pXvfy7uvq1hdDX1zcd/8ZjeCRQsRKHueqycpIrfenE03AI6geFKLrb29uPujgroQ2Z5coYjuNqG+bHuyFcEudQTLNTUfyKm22Yn5AzMh0n2DCKDVxW+1ycnZCGpFxWYqUrK8GrIVxWIyh0o8dyANfbsBp8z8hleLENo9iKP9uwGnwborNxqg2jWMP9cczFleDbkJtcGUOlabdGaUMypN1tWFnareFzRpR2z7ZhFGurTLs1fBqSclmJFa6slMKGuLQ7w05FsR832LBays7IpZiSdreguialcNBOl24ymLKG6GykpN2ugLT7Om6mMR/jfLwZR9lFiVDRJxjLUfS6v1ivA/WeMxC98/Ti83iBWzUcNp6IOzCWH/EMV10hrPec2aI1u/BZDD9DbHSPakhgsauqENYbhd+YLcpZh1PdpnUU3SOpaXeVK8tQr1p9OR+uw09pzK12sgRWHIGbMJb9OMVVVwjrfWi2CGMvXu+qaA0rTcPD2iKSLqy9p7eEdabgPm0QgRJBf4+j1aWVI+36DDLci14JoQl6xr1gw9YNudGVMai322XD1nA0R1Lcb6eieZR6zAvfoIawYCKFOoqxbEc90cuYiU0zUACn4BMKmp2R1N6uBhn+dHERD2G7DZOYzcE/r1lDUtNu6SADOz6fYradSuYs7KxrCDsYTpHS2z2I62xYyN14pg2zMKPxjCgTXGLDKL7GwkEGDpay4Rw7lY2Oxoakpl0NMpSlXWWZq22YjUmNDUm5P06gzyDDg6hB8JyM6W9IhrT7G26yYXOUXSjuslNZGTbwjKSm3S+5rPa6uBU34GHUW2OjP2HsIMW/rjRH6zVMYa6rqiWsMxrH47gGx+Jk3Igx6CCYHQzHHjMrjoPYYSqLhO3LXq6K+KJ2aaWm3V60RyUCfoj6dvPsVBS9tYYoJephGIvS7n8uDoJGjKF4FVP2v7HWkJS024cpY7vz8SobRqHeRI+OyAT8HWPZjuNsnWGw3Z1Y++Iby2o06Vc3aWra3e1ib9j5eIpXsPRNsoTl6k2oIVdgypP2M1eG8iKmjGKKPbhMQTtH5j3KRzQRge4PDVLvwoEHQ/OH4g58hiN2SDNrsE+9GS7F1PeRt6j7SRNR6XqsirfNTgbAvEn4i1maxl94uavWvCGqj1UFGvN9x4Z1vITqc6WyiLPxlYvNEfrHtC8/a/FktxsD04+ZJeloOLYuQemMpLx/FPEBR+yIi9UIDdgtsFNJ6P7TfbfTTjrYQWoeb8ZOPNftQvsYgsu0IAMLXbX1sGCPXZ6VRa56A9NP2dnJrMLmI/Is2GpWycdxvMVVr/qnosZqU/kWJ7tqB6F75HsbZqMHuxWwYw2g6ek9VtMJaNDvYe4LfepuihriM3wTwlJ2qLdA8TT2n51I1Ig51Fn8rZ6jdiUewhzofjOfySivxQOamYBG3P2GVVlRGeVzbZWBJa7Okdht5sSj/z/2Zz4v2OA+s2kaJ/AOV98CMyeehRj+qZqNdFaWq4YE9Gl5KM7E2I9EerfR4HY8VHCRqyiWeahPdmpQKPoUvRhz9MFMY2ZhzANSn88uxJfNVBj6THeb+wn5oNJO/E57COBdvB19uzt6aK7BB1DPmmqgcv1Z4E30Scv6h8Nc9PkKvBuXoD7O1PWMK4WdXYNv4DZUVmrGFnzfhoNQI3XffYSPY557oAler5r8AI2SaGxYH+z1gNIP0vvAaPwBle/VvdYgxB+obyT6Q6a+l/TyVA4enAijre1/L2HL9k9Ji0sAAAAASUVORK5CYII=&logoColor=white" width="250"></a>
</div>

## New menu options

> FusionFix adds new options to the in-game menu that can be easily toggled and customized in real time. The config file allows for further customization by advanced users.

#### Display

- **FOV**, adjusts in-game field of view
- **Language**, switch languages in-game like in previous versions
- **Definition**, toggle the old fuzzy look from consoles
- **Console Gamma**, restores contrasted look from consoles
- **Console Auto Exposure**, restores the stronger auto exposure / eye adaptation effect from consoles
- **Motion Blur**, a separate toggle for motion blur with several intensity levels
- **Depth of Field**, controls how intense the distant blur is during gameplay and whether it's enabled in cutscenes
- **Tree Lighting**, offers "PC", "PC" with some improvements ("PC+") and "Console" style tree lighting
- **Tree Alpha**, offers "PC", "PC" with some improvements ("PC+") and "Console" alpha test reference for trees
- **Bloom**, a global toggle for bloom
- **Screen Filter**, an option to change color filters in the main game and episodes
- **Distant Lights**, toggle for integrated [Project2DFX](https://thirteenag.github.io/p2dfx)

#### Graphics

- **FPS Limiter**, set a custom FPS limit, select from a list of predefined values, or turn it off
- **Anti-aliasing**, a toggle for FXAA or SMAA
- **Volumetric Fog**, a more advanced fog shader that hides the edges of the world
- **Sun Shafts**, a godrays implementation faithful to the original art style
- **Extended Sunlight Reach**, allows sunlight to reach surfaces that it couldn't before, and increases cloud shading
- **Tone Mapping**, prevents highlights from being blown out, more operators can be found [here](https://github.com/Parallellines0451/GTAIV.EFLC.FusionShaders/tree/main/assets/luts/samples)
- **Shadow Filter**, toggles between sharp, soft, or contact hardening shadows
- **Graphics API**, select between DirectX 9 and Vulkan (through [DXVK](https://github.com/doitsujin/dxvk/releases)), requires a restart 

#### Game

- **Skip Intro**, added an option to skip intro
- **Skip Menu**, added an option to skip main menu after startup
- **Letterbox**, draws cutscene borders on top and bottom of the screen
- **Pillarbox**, draws cutscene borders on screen sides in ultrawide
- **Transparent Map Menu**, makes the map screen background transparent
- **FPS Counter**, shows an episode colored FPS counter on the top left of the screen
- **Windowed**, toggle windowed mode
- **Windowed Borderless**, toggle borderless mode
- **Pause Game On Focus Loss**, pauses the game when it's minimized
- **Extra Night Shadows**, forces every lamppost and player headlights to cast shadows (*original PC-only "feature"; extremely broken, not recommended*)
- **LightSyncRGB**, custom ambient lighting for IV, TLAD and TBOGT, health indication on G-Keys, police lights, ammo counter, only Logitech hardware is supported, requires Logitech G HUB app, [**watch full clip on YouTube**](https://www.youtube.com/watch?v=oLxn3q-NnZ0&hd=1)

![LightSyncRGB](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/assets/4904157/f925ad27-19ce-4dde-8801-0cec1578ade7)

- **Seasonal Events**, enable added seasonal events at specific date
- **Check For Fusion Fix Updates**, checks if there's an update available on startup

#### Audio

- **Cutscene Audio Sync**, [see issue 691](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/issues/691), only enable this if you experience audio desynchronization in cutscenes
- **Alternative Dialogues**, when enabled, forces the game to use alternative dialogues in some missions

#### Controls

- **Always Run**, allows to run by default, like in classic GTA games
- **Allow Movement When Zoomed**, allows movement when aiming with a sniper rifle
- **Extended Sniper Controls**, allows aiming with sniper rifles without using the scope, press Jump button to toggle between third-person aim and the scope
- **Camera Shake**, shake effect has been fixed to work consistently at any frame rate and can be disabled with this option
- **Centered Vehicle Camera**, makes vehicle camera centered, for more options install the [original mod](https://github.com/gennariarmando/iv-centered-vehicle-cam) separately
- **Centered On Foot Camera**, makes on foot camera centered, also for more options install the [original mod](https://github.com/gennariarmando/iv-centered-onfoot-cam)
- **Turn Indicators**, allows the player to use vehicle turn indicators and hazard lights
- **Always Show Bullet Traces**, shows bullet traces on every shot instead of randomly, like in the older games
- **Vehicle Camera Centering Delay**, controls the time until vehicle camera auto centering kicks in after moving it
- **On Foot Camera Centering Delay**, controls the time until on foot camera auto centering kicks in after moving it
- **Vehicle Camera Turn Speed**, controls the camera turn speed when driving a vehicle and turning
- **Gamepad Icons**, allows to select various controller icon styles (Xbox 360, Xbox One, PS3, PS4, PS5, Nintendo Switch, Steam Deck)
- **Raw Input**, raw input for menu and adjustments for in-game camera mouse look

## Core/Gameplay Changelog

### General

- [Comprehensive shader overhaul with fixes for several longstanding issues, and various improvements](https://www.youtube.com/watch?v=sAfKfvAIsXw)
- [Fixed recoil behavior that was different when playing with keyboard/mouse and gamepad](https://github.com/GTAmodding/GTAIV-Issues-List/issues/6)
- [Fixed forced "definition off" setting in cutscenes, now cutscenes will respect the menu setting](https://github.com/GTAmodding/GTAIV-Issues-List/issues/5)
- [Fixed DLC car lights in TBoGT](https://github.com/GTAmodding/GTAIV-Issues-List/issues/3)
- [Fixed various issues related to high frame rates](https://github.com/GTAmodding/GTAIV-Issues-List/issues/112)
- [Added various frame limiting options to address fps related issues](https://github.com/GTAmodding/GTAIV-Issues-List/issues/1)
- [Fixed aiming zoom in TBoGT](https://github.com/GTAmodding/GTAIV-Issues-List/issues/19)
- [Fixed P90 being selectable in vehicles on keyboard](https://github.com/GTAmodding/GTAIV-Issues-List/issues/90)
- [Fixed possible crash when manually dial a phone number using the keypad in TLAD](https://github.com/GTAmodding/GTAIV-Issues-List/issues/232)
- [Fixed possible crash in TLAD mission "Marta Full of Grace"](https://github.com/GTAmodding/GTAIV-Issues-List/issues/235)
- [Added an option to fix a bug that only make taxis spawn](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/issues/85)
- [Added a file overload solution that allows to replace game files without actually replacing them](https://github.com/ThirteenAG/Ultimate-ASI-Loader?tab=readme-ov-file#update-folder-overload-from-folder)
- Added IMG Loader (from update folder)
- Added GXT Overloading, allow game to load additional .gxt files to overwrite vanilla lines or add new lines, without changing vanilla gxt files
- Increased corona limit to avoid heavy flickering of game's lights
- Improved ultrawide support
- Fixed improper VRAM detection on some setups
- Fixed draw distance sliders not being applied on startup
- Fixed mouse cursor scale, and mouse cursor is hidden when using controller now
- Significantly sped up loading screens
- Added various fixed weapon, movement and other animations
- Added various fixed vehicle models
- Fixed incorrect TBoGT bullet tracer position
- Restored missing lamppost coronas and uncompressed corona texture from console
- Fixed PC grass models stuck underground
- Added new tree LOD atlas based on the PC trees
- Fixed jagged vehicle reflections
- LOD lights now appear at the appropriate time and can render in cutscene, like on the console version
- Added an option to allow various postfx effect to render behind transparent objects properly
- Make night shadows resolution be adjusted by night shadows option
- Restored higher shadow render distance from old versions of the game
- Improved cascaded shadows performance, and added an option to double their resolution
- Added dynamic shadows for trees
- Reduced water flickering issue (need to disable **Extra Night Shadows**)
- Added several cheat code to enable seasonal events, restore some unused player assets for Niko and Johnny, and spawn some vehicles in TLAD and TBoGT, they can be found hidden in relevant locations, or finding them [here](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/blob/master/source/cheats.ixx)

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
- Chinatown gun shop: Fixed a small bug where the wrong voice lines is used after exiting when not buying something
- Restored voice lines from Luis, he'll now respond to what the pilot tells him about the landmarks during the tour
- Uptown Riders now use their own voice lines rather than Angels of Death voice in TLAD's arm wrestling
- Restored Clay's voice lines which is used when he meets Johnny in hangout friend activity in TLAD
- Restored Jim, Terry, Clay own voice lines in TLAD hi_lo_cards minigame
- Restored TLAD Combat Shotgun's own unique prop model
- Fixed the busker script terminates too early in TBoGT, now Luis can give money to busker normally
- Fixed several motorcycles use incorrect paying animation at toll station in TBoGT
- Fixed ilyena call and brycebug content can't trigger after load the save (From ilyena's random character mission and stubbs' job)
- Fixed the FIB arriving too early at the abandoned mansion in Phil's mission "To live and die in Alderney"
- Fixed the heli climb QTE being impossible on really high framerates
- Workaround for a bug where Niko dies after being kicked off of Dimitri's helicopter and falling into the water (Deal Ending)
- Workaround for a bug where Jacob crashes the Annihilator on high framerates (Deal Ending)
- Workaround for burned AOD won't trigger ragdoll behavior on high frame rates in TLAD mission "Action/Reaction"
- Fixed TLAD intro scripted cutscene's cut off distant world in several shots 

### Episodic content

>Note: most of this content requires additional modifications by the end user to the game in order to fully use these features.
>
- Added an option to enable support for APC, buzzard, smuggler, floater, blade and all their abilities in IV and TLAD
- Added an option to enable support for DSR1, grenade launcher, pipe bomb, sticky bomb, AA12 explosive shells, P90 vehicle check, partially parachute and all their abilities in IV, TLAD and TBoGT
- Added an option to enable explosive rounds on annihilator in IV
- Added an option to enable camera bobbing in clubs, cell phone switching, altimeter in helicopters and parachute, explosive sniper and fists cheats in IV and TLAD
- Added an option to raise height limit for helicopters in IV and TLAD to match TBOGT level
- Added an option to give P90 and AA12 to SWAT and FIB and M249 to police in helicopters in IV and TLAD
- Added an option to disable SCO signature check, allow game to use any episodic SCO on all episodes

### Misc

- Restored timecycles from consoles with improved auto exposure and restored missing moon in certain weather
- Fixed glass shards lacking colors
- Fixed particles flickering in some situations
- Restored console particle file, and fixed some broken particles
- Restored environment reflection intensity from consoles
- Added an option to restore the more reflective cars from consoles
- Fixed incorrect water texture and post processing noise tiling on "Water Quality" levels other than medium
- Restored Xbox360 contrast slider behavior
- Disabled Z-write behavior for emissive shaders to fix some visual issues
- Improved phone screen resolution
- Fixed offset mirrors
- Fixed washed out mirror reflections on some graphics cards
- Significantly reduced pop-in on higher graphics settings
- Restored functionality of some rain parameters from visualsettings.dat
- Fixed black rain droplets and missing refraction
- Restored console rain density
- Sped up menu and pause transitions
- Pressed radar zoom out button now will make it stay zoomed out for a few seconds
- Fixed helicopter searchlights flicker violently when more than one helicopter is present
- Fixed radio reset after cutscene
- Workaround for lights & motion blur flicker intermittently when player is drunk
- Fixed player can't give beggar money normally on PC
- Fixed TLAD phone keypad rendering on top of the plastic cover
- Fixed TLAD an incorrect traffic cops head texture
- Fixed some cutscene and interiors models
- Fixed and improved some lights and emissive materials
- Fixed a font uses incorrect style in TBoGT
- Added an option to enable shadows for some objects and vegetation that lack them
- Added options to customize shadow blur, bias and cascade blending 
- Various other fixes, like regular cops & fat cops will now spawn with their hat prop disabled when in a vehicle, like on the console version

### Some details

- **AimingZoomFix** - set to **1** for proper fix, so it behaves like on Xbox360 (memorize the zoom level from the last time aiming was canceled), set to **2** to have this fixed feature enabled in IV and TLAD, set to **-1** to disable this feature. **0** disables the fix, as usual.
- **FpsLimit** - enter a desired value to specify the number of frames per second you want for the game. Like 30, 45, 60 or more. This will affect all gameplay. Refresh rate offsets may also be used.
- **CutsceneFpsLimit** - enter a desired value to specify the number of frames per second you want for the game to use on cutscenes only.
- **DefaultCameraAngleInTLAD** - TLAD uses a different camera angle on bikes, this option can force the original IV camera angle. [Enabled](https://i.imgur.com/PqFHJfU.jpg) / [Disabled](https://i.imgur.com/5kM5Sgn.jpg)
- **PedDeathAnimFixFromTBOGT** - when you perform a counter attack after a dodge in IV & TLAD, the ped after falling and dying performs an additional death animation, which doesn't happen in TBOGT. [Enabled](https://imgur.com/EYsiGPe) / [Disabled](https://imgur.com/CR3LEdR)
- **DisableCameraCenteringInCover** - [see issue 20](https://github.com/GTAmodding/GTAIV-Issues-List/issues/20).
- **VehicleBudget** - allows to increase the budget of vehicles, used to avoid only taxi will spawn. For more information about TaxiBug and VehicleBudget, please watch [TJGM's video](https://www.youtube.com/watch?v=wr8m9kL3U20&t=237s).
- **PedBudget** - allows to increase the budget of pedestrians.

## Shaders Changelog

#### General

- Fixed z-fighting
- Restored deferred object and LOD fading
- Restored console screen door transparency

#### Lighting

- Fixed volumetric lights occlusion
- Improved light volume transparency
- Restored XBOX foliage translucency
- Improved PC foliage lighting
- Fixed mismatched AO behavior of shadow casting lights
- Fixed lights that were made invisible with patch 1.0.6.0
- Fixed black normal map halos on several surfaces such as asphalt, sidewalks and rocks

#### Shadows

- Restored rotated disk filter from versions prior to 1.0.6.0
- Fixed large shadow rectangles visible from high altitudes
- Fixed shadowmap being erroneously blurred before the lighting pass
- Fixed shadows stretching at certain camera angles
- Fixed shadow view distance being lower than the actual rendered distance
- Fixed distant shadows with cutoff edges
- Restored normal offset bias from versions prior to 1.0.6.0
- Added contact hardening shadows
- Fixed displaced night shadows
- Fixed blur artifacts between shadow cascades under some conditions
- Added basic cascade blending
- Fixed pitch black shadows under vehicles
- Added wind sway for real tree shadows
- Fixed incorrectly offset shadows on water
- Fixed flickering when shadows of transparent objects overlap
- Fixed dark lines in night shadows

#### Post processing

- Split depth of field, motion blur and stippling filter into separate passes to prevent overlap
- Fixed color banding, most noticeably in the sky
- Added alternative bloom threshold that doesn't cause color shifting
- Added a mask to selectively filter screen door transparency
- Fixed depth of field and bloom becoming weaker at resolutions higher than 720p
- Reduced depth of field haloing and increased bokeh intensity
- Improved bloom weights and stability in motion
- Fixed excessively blurry screen compared to consoles caused by leftover anti aliasing code
- Restored console bloom dilation
- Fixed flickering auto exposure under some conditions
- Fixed motion blur becoming weaker at high frame rates
- Fixed motion blur affecting props attached to the player, like guns and parachutes
- Fixed phone camera aspect ratio
- Fixed TLAD noise aspect ratio

#### Reflections

- Restored tree and terrain reflection behavior from XBOX
- Fixed excessive specularity of several meshes in Alderney
- Restored corona depth test in water reflections
- Added fading to coronas in paraboloid reflections
- Fixed anisotropic filtering affecting vehicle reflection intensity
- Fixed distorted mirror reflections at certain camera angles
- Improved vehicle and ped normal map quality

#### Particles

- Fixed soft particles
- Fixed particles flickering in some interiors
- Fixed stuttery particle animations caused by patch 1.0.5.0
- Fixed rain being almost invisible, especially at night
- Fixed rain streaks becoming shorter at high framerates

#### Water

- Fixed flickering outlines around shores and thin objects
- Fixed offset water reflections
- Restored water foam from consoles
- Improved shallow water transparency and improved foam texture
- Fixed flat, mirror-like water surface on some graphics cards

#### Misc

- Fixed outlines around objects when using some versions of DXVK
- Partially fixed building windows visible near the far plane if emissive depth write is disabled
- Restored gta_cutout_fence mipmap bias from versions prior to 1.0.6.0
- Fixed incorrect texture filtering used in several shaders
- Added AO to gta_normal_spec_reflect_emissive if emissivity is 0 so it can be used to disable night shadows for certain objects
- Added support to instantaneously turn lamppost bulbs on or off alongside their lighting

# Contributing

If you have an idea for a fix, add a module with its implementation to [source](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/tree/master/source) directory and open a pull request. See [contributing.ixx](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/blob/master/source/contributing.ixx) for reference.

# Reporting more issues

We're making a list of **GTA IV** issues that weren't addressed in official patches.

If you can think of one, submit it [here](https://github.com/GTAmodding/GTAIV-Issues-List/issues).

If you've encountered an issue, caused by this fix, report it [here](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/issues).
