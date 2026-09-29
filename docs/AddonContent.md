# Additional episodes, radio stations and fonts

`source/addoncontent.ixx` supports Complete Edition 1.2.0.59, EFLC 1.1.2.0,
and GTA IV 1.0.8.0. It uses the game's content manager and XML loader.

## Episodes

Place `FusionFixEpisodes.ini` beside `GTAIV.EFLC.FusionFix.asi`. No file or
default addon is created automatically. For example:

```ini
[Episode3]
Name = My Episode
Folder = MyEpisode

[Episode4]
Name = Another Episode
Folder = AnotherEpisode
```

Sections `Episode3` through `Episode63` are supported. `Folder` is relative to
the game executable, must be unique, and must contain `setup2.xml`. Absolute
paths and parent-directory components are rejected. Restart after editing.

An episode still needs its own complete game content and compatible scripts.
The INI registers its folder; it does not convert another game's assets or
generate an episode from ordinary replacement files.

Example `MyEpisode/setup2.xml`:

```xml
<ini>
    <device>e3</device>
    <content>
        <name>My Episode</name>
        <id>5</id>
        <episode>3</episode>
        <datfile>content.dat</datfile>
    </content>
</ini>
```

Use a unique device name for each folder. Content IDs and episode IDs are
different: content IDs 0–4 are reserved for stock content; addon content IDs
must be unique across all installed packs and lie between 5 and 63. The
`episode` value must match the INI section number. Extra packs in the same
folder may omit `episode`; FusionFix enables them only with their owner.
Do not reuse stock device names or content IDs.

Launch a registered episode with the game's `-episode` argument, for example:

```text
GTAIV.exe -episode 3
```

The startup selector adds previous/next page buttons when registered addons
have mounted successfully. Each additional page shows up to two episode names.
Mouse and keyboard/controller navigation are supported; Back returns to the
stock page. The stock selector artwork is reused. No additional menu assets
or changes to the stock XMLs are required.

Each addon has independent saves (`SGE0300` through `SGE0315` for episode 3).
The save list exposes the selected addon's saves, and stock episodes exclude
these addon filenames. Profile settings retain their stock filename. Existing
stock saves are not renamed or migrated. Content-ID conflicts disable the
affected addon instead of replacing another pack. Diagnostics are emitted to
the debugger with the `FusionFix addons:` prefix.

FusionFix's episode-relative features use the configured folder. IMG overrides
under `update/MyEpisode/` are restricted to that episode.

## Radio stations

The radio patch is automatic and independent of the episode INI. It guards
both the plain and colored station-icon lookups, so an additional station
without a corresponding texture does not dereference a missing texture.
Station definitions and audio still come from the game's audio metadata and
content files. This is the behavior of the referenced radio patch; it does not
increase a station-array limit or manufacture station definitions.

## Fonts

`source/addonfonts.ixx` expands font descriptors, glyph maps and draw batches
on all three supported executables. Add complete `FONT_ID` blocks to the
active `fonts.dat`, using IDs 9 through 255, and add matching textures named
`font9`, `font10`, etc. to the active fonts WTD. Use the stock font atlas layout
and metrics format. Keep the original font definitions and textures.

IDs 0 through 8 retain their stock meanings. Custom IDs may be sparse or out
of order. A missing texture makes that font unavailable; selecting it falls
back to stock font 0. Select a custom font through the normal font-style API.
Language-specific font dictionaries must also contain the custom textures
if that language is supported by the addon. Definitions and texture bindings
are refreshed when the game reloads its fonts.

## Verification

Every installed hook pattern has been checked against the three executable
databases. The texture lookup is deliberately restricted to the radio texture
loader, since its sequence also occurs elsewhere in the legacy executables.
Compilation and static checks do not replace in-game testing: test mounting,
new game, manual save/load, autosave, returning to stock episodes, and stations
with and without icons on each executable. Also test selector paging, returning
to its stock page, mouse/controller selection, custom font rendering, and font
reloads after changing language or episode.
