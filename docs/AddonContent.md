# Installing additional content

FusionFix discovers content packs and additional episodes on CE 1.2.0.59,
EFLC 1.1.2.0, and GTA IV 1.0.8.0. Packs must use the game's `setup2.xml` and
`content.dat` formats and contain compatible assets. Loose models alone do
not provide the required definitions.

## Quick start

1. Create `DLC` beside `GTAIV.exe`.
2. Extract each pack into its own subfolder, with `setup2.xml` directly inside
   it. Keep the pack's internal paths and assets together.
3. Restart the game. Individual packs do not need INI entries.

```text
GTAIV/
  GTAIV.exe
  plugins/
    GTAIV.EFLC.FusionFix.asi
    GTAIV.EFLC.FusionFix.ini
  DLC/
    Rune/
      setup2.xml
      content.dat
      rune_audio.xml
      common/
      pc/
    VPack5/
      setup2.xml
      content.dat
      audio.xml
      common/
      pc/
```

Only immediate subfolders are scanned. `DLC/Pack/Pack/setup2.xml` is one
folder too deep. Extract downloaded archives first.

To choose another location, edit `plugins/GTAIV.EFLC.FusionFix.ini`:

```ini
[FILELOADER]
DLCPath = DLC
```

Relative paths start at the game executable's directory; absolute paths are
also accepted. A blank `DLCPath` disables autodetection. Restart after adding,
removing, or editing packs.

## Shared packs versus episodes

The `<episode>` field in each XML `<content>` block determines where it loads:

| XML content | Behavior |
| --- | --- |
| No `<episode>` | Loads in GTA IV, TLAD, TBoGT and addon episodes. No startup-menu entry. |
| `<episode>3</episode>` through `<episode>63</episode>` | Loads only in that addon episode. Adds a startup-menu choice using its `<name>`. |

### A car or other shared pack

Example `DLC/ExampleCarPack/setup2.xml`:

```xml
<ini>
    <device>examplecars</device>
    <content>
        <name>Example Car Pack</name>
        <id>375824756</id>
        <datfile>content.dat</datfile>
        <audiometadata>audio.xml</audiometadata>
        <enabled />
    </content>
</ini>
```

Keep the audio metadata entry only if the pack supplies that file. Its
`content.dat` must register the required IDE, handling, carcols, IMG and other
files using normal game directives. Test a car pack from a stock episode and
use the model names supplied by the pack.

Do not add an episode ID merely to make cars load. That makes the content
exclusive to a separate episode, which needs compatible scripts and other
complete episode content to start correctly.

### An additional episode

Example `DLC/MyEpisode/setup2.xml`:

```xml
<ini>
    <device>e3</device>
    <content>
        <name>My Episode</name>
        <id>375825012</id>
        <episode>3</episode>
        <datfile>content.dat</datfile>
    </content>
</ini>
```

Use a unique episode ID from 3 through 63. IDs 0, 1 and 2 belong to stock
content. A folder may contain one addon episode and multiple shared content
blocks. Blocks without `<episode>` remain shared even when another block in
the same XML defines an episode. Include the episode ID in each block that
should be exclusive to it.

Successfully mounted episodes appear on additional startup-selector pages,
two per page. Use the previous/next arrows with the mouse or normal
keyboard/controller navigation. Back returns to the stock page. The selector
uses stock artwork and the XML episode name; no custom menu XML is required.
The game's `-episode 3` launch argument can also select episode 3 directly.

Addon episodes have separate saves: episode 3 uses `SGE0300` through `SGE0315`.
Shared packs use the saves of the episode being played. Stock saves and
profile-settings filenames are not renamed.

### Existing FusionFixEpisodes.ini installations

Existing `[Episode3]` through `[Episode63]` sections with a `Folder` entry
still register folders outside the discovery directory. However, `setup2.xml`
now determines the name and episode ID. The section number and old `Name`
setting do not convert shared content into an episode. A folder found through
both methods is registered only once.

You can move those packs into `DLC` and remove their old INI entries. New
installations do not need `FusionFixEpisodes.ini`.

## Content IDs and save compatibility

Content IDs and episode IDs are separate. Use unique decimal content IDs from
5 through 4294967295; IDs 0–4 are reserved for stock content. Large IDs such
as `375824756` are accepted without truncating their identity to one byte.

The engine stores content IDs in one byte and tracks them in a 64-bit save
mask. FusionFix maps XML IDs to internal slots 5–63 and automatically records
the mapping in `FusionFixContentIDs.ini` beside the ASI. You do not need to
create or edit this file. Small IDs retain their number when its slot is free.

There are **59 addon content slots**, counted per `<content>` block, not per
folder or episode. Large XML IDs avoid identity collisions; they do not expand
the save mask. Allocated slots remain reserved after removing packs so a new
pack cannot silently reuse an identity referenced by old saves. When slots
are exhausted, the affected folder is skipped and a diagnostic is emitted.

Keep `FusionFixContentIDs.ini` with the installation and back it up with saves.
Do not delete or reassign entries while retaining saves that use those packs.
When moving an installation, copy this file along with the same packs. If the
mapping cannot be saved, affected packs are not loaded.

Device names must also be unique, use only letters, digits and underscores,
and be at most 13 characters long. Do not reuse stock device names such as
`common`, `platform`, `extra`, `e1`, or `e2`. Duplicate content IDs, device names,
or episode IDs cause the later discovered folder to be skipped. Folders are
processed in sorted path order; collisions are not an override mechanism.

## Radio stations

Station definitions, audio and icons must come from normal game audio metadata
and content files. FusionFix guards plain and colored station-icon lookups,
allowing missing icons without dereferencing a missing texture. It does not
generate station definitions or raise the station-array capacity.

On CE 1.2.0.59, FusionFix also fixes a radio save-size mismatch: the loader
expects 23 station records, while the unpatched writer uses the installed
station count. Saves now always contain 23 records. Missing stations receive
empty records; stations beyond the first 23 do not have playback history
persisted. The live station count is unchanged.

This preserves the CE save layout when stations are added or removed. It does
not repair already corrupt saves or remap playback histories by station name
when stations are reordered. The legacy executables' save layouts are unchanged
by this CE-specific patch.

## Additional fonts

Add complete `FONT_ID` blocks to the active `fonts.dat`, using IDs 9 through
255, and matching textures named `font9`, `font10`, etc. to the active fonts
WTD. Use the stock atlas layout and metrics format; retain original definitions
and textures. Select custom fonts through the normal font-style API.

IDs 0 through 8 retain stock meanings. Custom IDs may be sparse or unordered.
Selecting a font without a texture falls back to stock font 0. Language-specific
font dictionaries must also contain custom textures for supported languages.
Definitions and bindings refresh when the game reloads its fonts.

## Troubleshooting

- **No cars in the spawn list:** check that `setup2.xml` is directly inside a
  pack folder under `DLC`, omit `<episode>` for shared packs, and check the IDE
  and IMG registrations in `content.dat`. FusionFix does not populate a
  trainer's custom spawn lists itself.
- **No startup-menu entry:** shared packs intentionally have none. For an
  episode, check the XML episode ID, uniqueness and successful mounting.
- **A pack is skipped:** check XML syntax, duplicate IDs/device names, write
  access to the mapping file, and free content slots. Engine limits require
  folder paths under 256 bytes, `datfile` under 32 bytes, audio filenames under
  64 bytes, and loading-screen/texture paths under 64 bytes including the
  device prefix.
- **Content loads but does not run correctly:** autodetection does not supply
  missing scripts, features from another loader, or asset conversions. Check
  the package's requirements and files.
- **Need diagnostics:** capture debugger output beginning with
  `FusionFix addons:`. Font texture diagnostics use `FusionFix fonts:`.

Episode-relative features use the discovered folder. For `DLC/MyEpisode`, IMG
overrides under `update/DLC/MyEpisode/` are restricted to that episode.

## Implementation and verification

Content discovery, mounting and selector integration are in
`source/addoncontent.ixx`; font expansion is in `source/addonfonts.ixx`.
Content hooks use verified patterns for all three executables. The radio-save
patch requires CE's specific header and writer patterns and does not apply
to legacy executables.

Build and XML checks do not replace in-game testing. Test shared packs in each
installed episode, addon selection, new game, manual save/load, autosave, and
radio changes on the executable being used.
