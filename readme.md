[![Build status](https://ci.appveyor.com/api/projects/status/wwokiviriq3m52t6?svg=true)](https://ci.appveyor.com/project/ThirteenAG/gtaiv-eflc-fusionfix)
[![Discord](https://img.shields.io/badge/chat-on%20discord-7289da.svg?logo=discord)](https://discord.gg/PJEJHaB5)

<p align="center">
  <img height="100" src="https://user-images.githubusercontent.com/4904157/63623173-921aaf00-c601-11e9-8b84-fc5803269323.png">
</p>

# GTAIV.EFLC.FusionFix

This projects aims to fix some issues in Grand Theft Auto IV Complete Edition.

Changelog:

- [Fixed recoil behavior that was different when playing with keyboard/mouse and gamepad](https://github.com/GTAmodding/GTAIV-Issues-List/issues/6)
- [Fixed forced "definition off" setting in cutscenes, now cutscenes will respect the menu setting](https://github.com/GTAmodding/GTAIV-Issues-List/issues/5)
- [Partially fixed emissive shader bug, lights are now present in the game (e.g. Rotterdam Tower, Comrades Bar area)](https://github.com/GTAmodding/GTAIV-Issues-List/issues/4)
- [Fixed DLC car lights in TBoGT](https://github.com/GTAmodding/GTAIV-Issues-List/issues/3)
- [Fixed handbrake camera, that doesn't work with high fps](https://github.com/GTAmodding/GTAIV-Issues-List/issues/10)
- [Added various frame limiting options to address cutscene zoom and other fps related issues](https://github.com/GTAmodding/GTAIV-Issues-List/issues/1)
- [Fixed aiming zoom in TBoGT](https://github.com/GTAmodding/GTAIV-Issues-List/issues/19)
- Added an option to skip intro
- Added an option to skip main menu after startup
- Added an option to force borderless windowed mode
- Added an option to fix mouse

## Installation:

### Install Grand Theft Auto IV Complete Edition (v1.2.0.30 and above required)

### **Download**: [Ultimate-ASI-Loader.zip](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest/download/Ultimate-ASI-Loader.zip)

##### Unpack **dinput8.dll** to your **GTAIV Complete Edition** root directory.

### **Download**: [GTAIV.EFLC.FusionFix.zip](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/releases/latest/download/GTAIV.EFLC.FusionFix.zip)

##### Unpack content of the archive to your **GTAIV Complete Edition** root directory.

This fix was tested only with latest official update and latest [ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest/download/Ultimate-ASI-Loader.zip).

---

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

**MouseFix** - fixes small mouse movements not being recognized.

# Reporting more issues

We're making a list of **GTA IV** issues that weren't addressed in official patches.

If you can think of one, submit it [here](https://github.com/GTAmodding/GTAIV-Issues-List/issues).

If you've encountered an issue, caused by this fix, report it [here](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/issues).
