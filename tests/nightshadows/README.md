# Experimental player-centered night shadows

This draft ports the published experimental shadow implementation to current
FusionFix source for maintainer review. All three new INI options default to zero.
It does not increase the engine's dynamic shadow-slot capacity.

## Contribution and provenance

- **onyxoak:** problem identification, desired behavior, reproduction reports,
  repeated in-game testing, and release coordination.
- **OpenAI Codex:** AI-assisted implementation, tests, and preparation of this
  upstream contribution. This is not represented as hand-written code by onyxoak.
- **ThirteenAG and FusionFix contributors:** the original project and foundation.

If this work is incorporated, please credit onyxoak for the investigation,
requirements, and testing, with the AI-assisted implementation disclosed.
Applicable FusionFix GPLv3 terms and third-party notices remain in effect.

Published source and original preview:
https://github.com/onyxoak/fusionfix-extra-night-shadows/releases/tag/build18-preview

## Behavior under investigation

1. Keep the occupied player's vehicle eligible for streetlamp shadows.
2. Prioritize relevant nearby shadow lights within the existing seven dynamic slots.
3. Keep light selection stable as traffic and candidate order change.
4. Exclude the occupied vehicle and its occupants only from that vehicle's own
   immediate headlight shadow pass, preventing the observed driving-only cutoff.
5. Keep Niko eligible when on foot in the player's or traffic headlights.

The light-admission workaround is moved from fixes.ixx to nightshadows.ixx so it
installs after guarded allocator setup rather than modifying the same site early.
The proposed hooks use audited Complete Edition 1.2.0.59 layout/signature guards.
Other executable versions are not validated. The deferred caster path is not
covered. Hook rejection, thread assumptions, vanilla fallback behavior, water,
interiors, helicopters, traffic density, performance, and mod conflicts need review.

Unlike the standalone preview, this contribution preserves the existing menu
warning. It does not include the external installer, disable updates, change
Extra Night Shadows defaults, or install anything into a game directory.

## Reproduction configuration

Use an isolated test installation of the audited GTA IV Complete Edition 1.2.0.59.
In the SHADOWS section of the FusionFix INI, opt in to:

```ini
ExperimentalPlayerShadowAllocation = 2
ExperimentalOwnHeadlightCasterFix = 1
ExperimentalShadowDiagnostics = 1
```

Select Extra Night Shadows mode 3 in the game configuration. Allocation mode 1 is
an observation mode; mode 0 disables the experimental allocator. Diagnostics are
written beside the FusionFix INI as GTAIV-shadow-candidate18.log.

Test at night with a car under a lamp, then drive north/south and east/west,
rotate the car/camera, exit with headlights on, and walk Niko through both the
player vehicle's beams and traffic headlights. Compare the same location, time,
weather, camera, and vehicle with experimental options disabled and enabled.
Record actual gameplay; the promotional mockup is not before/after evidence.

## Validation status

The prior baseline release compiled as Release/Win32 with MSVC 14.51.36231.
The user reports that everything tested in the installed release works as intended on one PC/mod setup, with occasional flickering of distant streetlamps still observed.
That report is not independent validation or evidence of universal correctness.

For this current-upstream port, these existing tests were rebuilt and passed:

- Caster policy, PE guards, and startup diagnostics: 5,548 checks.
- Light geometry: 39 checks.
- Allocation adapter: 1,845,461 checks per run, including rejected mutations.
- Allocation pass: 4,960 checks.
- Seven-slot budget: 754,197 checks.
- Floating-point state: 4,864 checks.
- Stable selector: 3,421,980 checks.

The full current-upstream production build and live game behavior have not been
verified. The production-linked hook-order fixture passed on the earlier baseline
but has not been rerun against current upstream dependencies. This is a draft.

## Running tests

From PowerShell, pass the local Visual Studio installation explicitly:

```powershell
.\tests\nightshadows\Run-LightGeometryTests.ps1 -VisualStudioRoot 'C:\path\to\VisualStudio'
.\tests\nightshadows\Run-CasterTests.ps1 -VisualStudioRoot 'C:\path\to\VisualStudio' -GameExe 'C:\path\to\GTAIV.exe'
```

Run the other Run-*.ps1 files similarly. Scripts accepting Architecture can test
x86 or x64. GameExe is needed only by scripts that inspect the executable on disk;
those tests do not launch or attach to the game. No game executable is included.
Run-HookOrderTests.ps1 requires the production Release/Win32 object files first.

For a production build, initialize the upstream submodules recursively and follow
the upstream build workflow. With matching Visual Studio C++ tools and dependencies:

```bat
premake5.exe vs2026 --with-version=5.0.1.1
msbuild build\GTAIV.EFLC.FusionFix.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:PostBuildEventUseInBuild=false /m
```

The post-build override prevents automatic deployment. Current upstream dependency
changes may require additional build preparation; the CI workflow is authoritative.
