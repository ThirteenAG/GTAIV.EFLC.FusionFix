# GXT Compiler/Decompiler

[![Join Discord](https://img.shields.io/badge/discord-join-7289DA.svg?logo=discord&longCache=true&style=flat)](https://discord.gg/WgAg9ymHbj)

[![Support Author](https://img.shields.io/badge/support-author-blue)](https://bit.ly/3sX2oMk) [![Help Ukraine](https://img.shields.io/badge/help-ukraine-yellow)](https://bit.ly/3afhuGm)

A tool to compile TXT files into GXT and decompile GXT back to TXT using the original syntax of TXT source files.

Usage: `gxt -i [input_file(s)] -o [output_file] [[optional parameters]]`

## Parameters
- `-i [input_file(s)]`: list of TXT files which you want to compile into GXT (can be one or multiple files) or a GXT file you want to decompile into TXT
- `-o [output_file]`: name of a GXT/TXT file that will be generated

### Optional parameters
- `-t [table_file]`: charset table file that is used for GXT. Syntax example is listed below. Tables for some games are contained in `tables'` directory
- `-k (CHAR8|CHAR16|CRC32|JENKINS|CHAR8_TIME|CHAR12_TIME)`: type of keys used in GXT. CRC32 for SA, JENKINS for IV, CHAR8_TIME and CHAR12_TIME - MH2, CHAR8 - rest of the games. **Default**: CHAR8
- `-w(0|1)`: 0 - use one byte chars, 1 - use two byte chars. Only some versions of SA and IV should have this set to 0. Rest of the games use 1. **Default**: 1
- `-f (KEY1,KEY2,KEY3...)`: list of comma separated keys that would have no conversion applied to them
- `-b`: enable big endian. Only useful for MH2 Wii.

#### Compilation only
- `-c (CODEPAGE)`: TXT codepage ID if the file isn't UTF-8 BOM, UTF-16 LE BOM or UTF-16 BE BOM. **Default**: 1252
- `-h(0|1|2)`: 0 - don't write GXT header, 1 - write GXT header, 2 - write GTA 2 header. Only SA and IV should have this set to 1. **Default**: 0
- `-s(0|1)`: 0 - don't sort tables, 1 - sort tables. All of the games have tables sorted except LCS and VCS. **Default**: 1
- `-m(0|1)`: 0 - don't sort keys in the main table, 1 - sort keys in the main table. All games should have this enabled except IV. **Default**: 1
- `-u(0|1)`: 0 - leave key case as is, 1 - turn all keys to uppercase. Should be 0 only for GTA 2. **Default**: 1
- `-d(0|1)`: 0 - don't remove repeated spaces, 1 - remove repeated spaces. **Default**: 1. Normally you'd want to have this on, but for Korean version this was disabed.
- `-~`: enable highlighting \~TAGS\~ with 0x8000. This is only used for non-latin languages in some games. Examples: III PS2 Japanese, III PC Japanese, IV Japanese and Russian (probably more Japanese translations too, except mobile version)
- `-l (LANGUAGE)`: language id. Only useful for GTA 2

#### Decompilation only
Commands that start with `--original-` are used for adding untranslated text as comments to translations like this:

```
[CRMCANC]
{CANCEL}
ABBRECHEN
```

- `--original-i [input_file]`: GXT file of an untranslated text to have comments of the original text added to each line
- `--original-t [table_file]`: charset table that is used for the untranslated GXT
- `--original-k (CHAR8|CHAR16|CRC32|JENKINS|CHAR8_TIME|CHAR12_TIME)`: type of keys used in the untranslated GXT. **Default**: whatever is supplied in -k
- `--original-w(0|1)`: 0 - use one byte chars, 1 - use two byte chars. **Default**: whatever is supplied in -w
- `--original-f (KEY1,KEY2,KEY3...)`: list of comma separated keys that would have no conversion applied to them
- `--original-b`: enable big endian
- `--original-s`: sort keys using order from the original untranslated GXT

## Docs and examples
<details><summary>GXT Syntax</summary>

```
[KEY] {commentary}
Text
  
[KEY2:TABLE]
More Text {more comment}
```

</details>

<details><summary>Table File Syntax</summary>

```
Unicode_HEX_CODE=GXT_HEX_CODE
```

You can get Unicode Hex Codes here: https://unicode-table.com/en/
  
Example:

```
0152=008C
0153=009C
0160=008A
```

</details>

<details><summary>Compilation examples for different games</summary>
Look into bat files of this repository: https://github.com/Sergeanur/GXT
</details>
