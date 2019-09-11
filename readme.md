[![Build status](https://ci.appveyor.com/api/projects/status/wwokiviriq3m52t6?svg=true)](https://ci.appveyor.com/project/ThirteenAG/gtaiv-eflc-fusionfix)
[![Build Status](https://dev.azure.com/thirteenag/GTAIV.EFLC.FusionFix/_apis/build/status/ThirteenAG.GTAIV.EFLC.FusionFix?branchName=master)](https://dev.azure.com/thirteenag/GTAIV.EFLC.FusionFix/_build/latest?definitionId=1&branchName=master)
[![Discord](https://img.shields.io/badge/chat-on%20discord-7289da.svg?logo=discord)](https://discord.gg/y2cZFRA)

<p align="center">
  <img height="100" src="https://user-images.githubusercontent.com/4904157/63623173-921aaf00-c601-11e9-8b84-fc5803269323.png">
</p>

# GTAIV.EFLC.FusionFix

This projects aims to fix some issues in Grand Theft Auto IV and Episodes from Liberty City.

Changelog:

- [Fixed recoil behavior that was different when playing with keyboard/mouse and gamepad](https://github.com/GTAmodding/GTAIV-Issues-List/issues/6)
- [Fixed forced "definition off" setting in cutscenes, now cutscenes will respect the menu setting](https://github.com/GTAmodding/GTAIV-Issues-List/issues/5)
- [Partially fixed emissive shader bug, namely BAWSAQ building is still rendered incorrectly, but the rest of the lights are now present in the game (e.g. Rotterdam Tower, Comrades Bar area)](https://github.com/GTAmodding/GTAIV-Issues-List/issues/4)
- [Fixed DLC car lights in TBoGT](https://github.com/GTAmodding/GTAIV-Issues-List/issues/3)
- [Tree foliage casts dynamic shadows instead of static](https://github.com/GTAmodding/GTAIV-Issues-List/issues/9)
- [Fixed handbrake camera, that doesn't work with high fps](https://github.com/GTAmodding/GTAIV-Issues-List/issues/10)
- [Added various frame limiting options to address cutscene zoom and other fps related issues](https://github.com/GTAmodding/GTAIV-Issues-List/issues/1)
- [Fixed aiming zoom in TBoGT](https://github.com/GTAmodding/GTAIV-Issues-List/issues/19)

## Installation:

#### GTAIV 1.0.8.0 / GTAIV 1.0.8.0 + DLC Episodes / GTA EFLC 1.0.3.0

This fix is tested only with latest official update and [XLiveLessAddon](https://bitbucket.org/ThirteenAG/xlivelessaddon/downloads/), and I can not guarantee that the package will work without it.

The following installation instructions assume that you're either have [XLiveLessAddon](https://bitbucket.org/ThirteenAG/xlivelessaddon/downloads/) already, or willing to install it.
Online features of the game are not available with [XLiveLessAddon](https://bitbucket.org/ThirteenAG/xlivelessaddon/downloads/).

---

### **Download**: [GTAIV.EFLC.FusionFix.zip](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/releases/latest/download/GTAIV.EFLC.FusionFix.zip)

This requires ASI Loader or [XLiveLessAddon](https://bitbucket.org/ThirteenAG/xlivelessaddon/downloads/).

Unpack the content of archive to your **GTAIV** or **EFLC** directory.

### **TBoGT as DLC note**:

if you have episodes installed as DLC in your **GTAIV** folder, you need to move

`Grand Theft Auto IV/TBoGT/pc/models/cdimages/vehicles.img`

to

`Grand Theft Auto IV/DLC/TBoGT/content/pc/models/cdimages/vehicles.img`

### **MAIN section notes**:

**HandbrakeCamFix** - this only affects handbrake camera when using a keyboard, with gamepad handbrake camera is handled differently and is too fast with high fps. No solution for that at the moment. Regular camera movement is fixed as well, which is too slow without this fix on high fps.

**AimingZoomFix** - set to **1** for proper fix, so it behaves like on xbox, set to **2** to have this fixed feature enabled in IV and TLAD, set to **-1** to disable this feature. **0** disables the fix, as usual.

### **FRAMELIMIT section notes**:

**FpsLimit** - enter a desired value to specify the number of frames per second you want for the game. Like 30, 45, 60 or more. This will affect all gameplay.

**CutsceneFpsLimit** - enter a desired value to specify the number of frames per second you want for the game to use on cutscenes only. Recommended 60 or lower, this option aims to fix the cutscene zoom that happens when playing on high fps.

**ScriptCutsceneFovLimit** - this is the minimum fov value the game can set during script cutscenes, [like this one](https://www.youtube.com/watch?v=NzKw7ijHG10&hd=1). It prevents the game to zoom in too much on high fps. Since it's not a proper fix for the issue, you may disable it and use _hidden_ setting **ScriptCutsceneFpsLimit** that works the same way as **CutsceneFpsLimit**.

### **MISC section notes**:

**DefaultCameraAngleInTLAD** - TLAD uses a different camera angle on bikes, this option can force the original IV camera angle. [Enabled](https://i.imgur.com/PqFHJfU.jpg) / [Disabled](https://i.imgur.com/5kM5Sgn.jpg)

**PedDeathAnimFixFromTBOGT** - when you perform a counter attack after a dodge in IV & TLAD, the ped after falling and dying performs an additional death animation, which doesn't happen in TBOGT. [Enabled](https://imgur.com/EYsiGPe) / [Disabled](https://imgur.com/CR3LEdR)

**DisableCameraCenteringInCover** - [see issue 20](https://github.com/GTAmodding/GTAIV-Issues-List/issues/20).

### **EXPERIMENTAL section notes**:

**LodShift** allows to move position of all lods in the game, which reduces z-fighting. Use values in the range (-0.5; 0.5).

**LodForceDistance** increases the draw distance of objects affected by **LodShift**. Set to 1 to enable.

`It is not recommended to enable these options, so use them at your own risk.`

---

## Files rundown:

    TBoGT/pc/models/cdimages/vehicles.img

**Car lights fix for TBoGT.**

    pc/data/maps/props/vegetation/ext_veg.ide
    pc/data/maps/props/vegetation/ext_veg.img

**Trees shadows fix.**

    common/shaders/win32_30/gta_emissivestrong.fxc
    common/shaders/win32_30_atidx10/gta_emissivestrong.fxc
    common/shaders/win32_30_low_ati/gta_emissivestrong.fxc
    common/shaders/win32_30_nv6/gta_emissivestrong.fxc
    common/shaders/win32_30_nv7/gta_emissivestrong.fxc
    common/shaders/win32_30_nv8/gta_emissivestrong.fxc

**Corrected shaders to make the lights appear. Used in conjunction with GTAIV.EFLC.FusionFix.asi.**

    plugins/GTAIV.EFLC.FusionFix.asi
    plugins/GTAIV.EFLC.FusionFix.ini

**Recoil, definition, shader and other bugs are fixed here.**

# Reporting more issues

We're making a list of **GTA IV** issues that weren't addressed in official patches.

If you can think of one, submit it [here](https://github.com/GTAmodding/GTAIV-Issues-List/issues).

If you've encountered an issue, caused by this fix, report it [here](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/issues).
