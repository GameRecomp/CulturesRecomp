# Research

This file covers background on the series and what we learned from analysing the binaries. It includes
facts only, with their source. Open questions are marked **(open)**. Detailed notes on single
topics go into [`notes/`](notes/).

No disassembly, decompiled code or game text goes into this repository. See
[CONTRIBUTING.md](../CONTRIBUTING.md).

## The series

Source: German and English Wikipedia (series article *Cultures (Computerspielreihe)*,
*Cultures (video game)*, *Cultures 2: The Gates of Asgard*), plus the GOG installers themselves.

| Year | Title (DE)                              | Title (EN)                        | Kind       | Engine | In scope |
|------|-----------------------------------------|-----------------------------------|------------|--------|----------|
| 2000 | Cultures – Die Entdeckung Vinlands      | Cultures: Discovery of Vinland    | game       | C1     | yes      |
| 2001 | Die Rache des Regengottes               | (Revenge of the Rain God)         | add-on     | C1     | yes, via Gold |
| 2001 | Cultures Gold                           | —                                 | bundle: base + add-on + maps | C1 | yes (German ISO) |
| 2002 | Cultures 2 – Die Tore Asgards           | Cultures 2: The Gates of Asgard   | game       | C2     | yes      |
| 2002 | Reise nach Nordland                     | Cultures: Northland               | game (planned as C2 add-on) | C2 | yes |
| 2003 | Das Achte Weltwunder                    | Cultures: 8th Wonder of the World | game (planned as C2 add-on) | C2 | yes |
| 2003 | Die Abenteuerbox                        | —                                 | bundle: Northland + 8th Wonder | C2 | covered by its parts |
| 2003 | Cultures – Die Saga                     | —                                 | compilation: Northland + 8th Wonder + *Drachenland* campaign + community campaigns; Vinland on a 2nd CD | C2 (+C1 CD) | yes (German retail) |
| 2010 | Cultures Online                         | Cultures Online                   | Flash browser game, shut down 2018 | — | no |
| 2013 | —                                       | Northland + 8th Wonder (GOG bundle, Kalypso) | re-release | C2 | yes (the GOG installers) |
| 2014 | —                                       | Cultures: Discovery of Vinland (iOS) | port    | —      | no       |
| 2015 | —                                       | Northland, 8th Wonder (Steam, Daedalic) | re-release | C2 | not yet (builds not hashed) |

- Developer: Funatics (Oberhausen). Vinland was made with Joymania. The publishers were THQ and Phenomedia
  for C1, and JoWooD for C2. Northland and 8th Wonder were self-published in Germany after JoWooD ended
  the contract. GMX Media published the English versions.
- Runesoft made Mac and Linux ports of Northland (no multiplayer, no editor), and there is a Mac version of 8th Wonder.
  These ports could be a cross-check for behaviour. They are **not** targets.
- Wikipedia states that Vinland and Northland/8th Wonder use different engines. That is why
  *Die Saga* ships Vinland on a separate CD. Our binary analysis confirms this, and also shows that
  Cultures 2, Northland and 8th Wonder share one engine (see below).

## Editions

Exclusive content per edition (the overview table is in the README):

- **Die Rache des Regengottes** (add-on, 2001): a new campaign of seven single-player missions with branching
  mission flow, many new multiplayer maps (16 for up to 6 players per c3-net), and a mercenary system that
  hires warriors from allied tribes. Sources: Gameswelt announcement, c3-net.
- **Cultures Gold** (DE, 2002 per OGDB): base game + add-on + a few new maps. A shop listing (DB-KISS) names
  4 single-player and 3 multiplayer maps, including a Christmas map. That count is not confirmed independently **(open)**.
  Our ISO contains `Data_m/F2_*` and `M3_*` maps and the extra movies `Intro2.bik`/`Extro2.bik`.
- **Northland / 8th Wonder:** each ships a map editor. 8th Wonder adds the Egyptians, the earthquake effect
  and a revised editor (Wikipedia).
- **Die Abenteuerbox** (2003): Northland + 8th Wonder in one box, with no new content (Wikipedia).
- **Die Saga** (2003): see [below](#die-saga).
- **Re-releases:** GOG sells Cultures, Cultures 2, Northland and 8th Wonder (Northland + 8th Wonder also as a bundle,
  2013, Kalypso). Steam has Northland and 8th Wonder (2015, Daedalic). No re-release adds content.

Sources: [Wikipedia (de), Cultures (Computerspielreihe)](https://de.wikipedia.org/wiki/Cultures_(Computerspielreihe)),
[OGDB: Cultures Gold](https://ogdb.eu/index.php?gameid=56848&section=game),
[DB-KISS: Cultures Gold](https://www.db-kiss.de/games/details/428.html),
[Gameswelt: Die Rache des Regengottes](https://www.gameswelt.de/cultures/news/die-rache-des-regengottes-erscheint-im-april-2001-60496),
[c3-net: Rache des Regengottes](https://www.c3-net.de/games/show/11), [c3-net: Die Saga](https://www.c3-net.de/games/show/20),
[Steam bundle](https://store.steampowered.com/sub/63734/), [GOG DB bundle](https://www.gogdb.org/product/1207659199).

## Die Saga

The copy we have is the German retail release, as a zip of an installed directory. Findings:

- **Content:** Northland and 8th Wonder (intro movies `intro01.mpg`, `intro02.mpg`), the new **Drachenland**
  campaign (5 missions per Wikipedia; its own `intro.mpg`), and two community campaigns in
  `DataX/UserCampaigns/` with 22 + 6 = 28 missions (`.c2m`). It also has the map editor. `data0001.lib` is 306 MiB,
  more than either game alone. Cultures 1 was on a separate bonus CD, which is not part of this copy.
- **Multiplayer:** the readme says Saga players cannot play with owners of Northland or 8th Wonder.
- **Executables:** same C2 engine and same imports as Northland/8th Wonder (DirectDraw, DirectSound 8,
  DirectMusic via the identical `gedx8musicdrv.dll`, DirectPlay 8). There are two single-player builds:
  - `Game.exe`, built on Nov 5 2003. `Autorun.exe` starts it (registry key `SOFTWARE\Funatics\Saga`). It imports
    ten functions that `GameSaga.exe` does not: `IsBadCodePtr`, `IsBadReadPtr`, `IsBadWritePtr`,
    `SetUnhandledExceptionFilter`, `SetErrorMode`, `QueryPerformanceCounter`/`Frequency`, `GetTickCount`, `GetFileSize`
    and `GetForegroundWindow`. It also references `datax\extro.dat`, which is not installed. This looks like a
    disc check **(open)**.
  - `GameSaga.exe`, built on Jul 18 2005. It has an intact MSVC Rich header, so it is a genuine Funatics build, probably from a later re-release
    or patch **(open)**. It lacks the extra imports above.
  - Plan: lift both. `GameSaga.exe` is the preferred first target if `Game.exe` turns out to need the CD.

## What the GOG Cultures 1 contains

The GOG *Cultures* (EN and DE) is the **base game only**. Its `Data_m/` is empty, and the executable
does not reference `Intro2.bik`/`Extro2.bik`. The German **Gold Edition** ISO contains the add-on
*Die Rache des Regengottes*: maps `F2_*`, `M3_*` in `Data_m/` and the extra movies `Intro2.bik` and
`Extro2.bik`. It is the only source of the add-on that we have, so `gold-de` is a first-class variant.

## Engine families

Both engines (and every Saga binary) were built with MSVC 6.0 (linker 6.0; Rich header build numbers in the 8xxx–9xxx range,
exact service packs **(open)**). They are C++, have no relocations, and use image base `0x400000`. No
executable is packed except the special case below. All `.text` entropies are ≈ 6.66.

| Feature          | C1 (`Cultures.exe`)                         | C2 (`Cultures2.exe`)                     | C2 (`Game.exe`, Northland / 8th Wonder / Saga) |
|------------------|---------------------------------------------|------------------------------------------|-----------------------------------------|
| `.text` size     | ~0.64 MiB                                   | ~0.90 MiB                                | ~0.92–0.94 MiB                          |
| Graphics         | DirectDraw (loaded at runtime via `LoadLibrary("DDRAW.DLL")`, `DirectDrawCreate`/`Ex`, QI `IDirectDraw2`), 16-bit surfaces | same code base | same |
| Input            | DirectInput (`DirectInputCreateA`)          | same                                     | same                                    |
| Sound            | Miles (`mss32.dll`) + `DSOUND` ordinal 1 (`DirectSoundCreate`) | Miles (`mss32.dll`)          | `DSOUND` ordinal 11 (`DirectSoundCreate8`), **no Miles** |
| Music            | Red Book CD audio via MCI (`mciSendCommandA`) | DirectMusic via `gedx8musicdrv.dll` (DLS + `.sgt` segments in `DataX/DM/`), and also MCI CD audio | DirectMusic via `gedx8musicdrv.dll` (`DataX/DM2/`) |
| Video            | Bink (`binkw32.dll`, `.bik`)                | Bink (`DataX/FMV/<lang>/*.bik`)          | **MPEG-1 program stream** (`DataX/FMV/<lang>/*.mpg`). Playback API **(open)**; candidates are DirectShow via `CoCreateInstance`, or MCI |
| Network          | DirectPlay 4 through Funatics' own `DPlayDLL.dll` (C++ class `DPLAYDLL_CLASS`, 64 exports) | DirectPlay 8 (`IDirectPlay8Peer` via COM) + Winsock (`WSOCK32`, `WS2_32` by ordinal) | DirectPlay 8 + `WSOCK32` |
| Archives         | `data_l/data.lib`, `data_m.lib`, `data_v.lib` | `DataX/Libs/data0001.lib`              | `DataX/Libs/data0001.lib`               |
| Config           | registry (`Reg*`), `game.ini`, `campaign.ini` | `cultures2.ini` (`set_language`), registry | `Game.ini`, registry              |
| Extra binaries   | —                                           | `Cultures2MP.exe` (multiplayer build)    | `GameMp.exe` (multiplayer build), `Editor.exe` (map editor; GDI, `COMCTL32`, `comdlg32`, `WINSPOOL`) |
| Other imports    | `VERSION`, `SHELL32` (`ShellExecuteA`), `ole32` (`CoCreateInstance` for DirectMusic etc.), `GDI32` palette/text | + `CoCreateGuid`, `ShellExecuteExA` | same |

All four C2-engine titles carry the same engine strings: the `CDPNet`/`CDPLogic` network layer, the AI task names
and `data\engine2d\inis\...`. Northland, 8th Wonder and the Saga are, technically, Cultures 2 with new data, an editor
and a different video/sound backend. **Consequence:** HLE modules, patches and knowledge are shared across the
four C2-engine titles. Function addresses differ per executable, so the manifests do not share them.

DirectX requirement strings: the C1 executable says "DirectX 7.0 required!" and detects the DX version.
`D3D8.DLL` appears as a string in all of them, probably only for version detection **(open)**. No
executable imports `d3d*` or `ddraw` statically.

Rendering constraint: all engines insist on 16-bit colour in windowed mode ("can only work with 8 or 16
bit bitmaps in window mode when desktop bitdepth is 16 bit"). The `ddraw` HLE therefore exposes a 16-bit
primary surface. The pixel format (565 vs 555) is chosen by the game from `DDPIXELFORMAT` **(open: which
formats each engine accepts)**.

## DLL base addresses

All middleware DLLs carry relocations (checked with `recomp info`); the executables do not.

| DLL | Game | Preferred base | Entry |
|-----|------|----------------|-------|
| `binkw32.dll` | cultures1 (GOG EN, Gold) | `0x10000000` | `0x1002c37c` |
| `binkw32.dll` | cultures1 (GOG DE, a different Bink build with sections `BINK`, `BINK32`, …) | `0x30000000` | `0x3002dac5` |
| `DPlayDLL.dll` | cultures1 | `0x10000000` | `0x1000b00e` |
| `Mss32.dll` | cultures1, cultures2 | `0x21100000` | `0x21134697` (C1) / `0x2112f6b5` (C2) |
| `Mp3dec.asi` | cultures1 | `0x26f00000` | `0x26f0a995` |
| `binkw32.dll` | cultures2 | `0x30000000` | `0x3002dac5` |
| `gedx8musicdrv.dll` | cultures2, northland, eighthwonder, saga | `0x10000000` | `0x100051c4` |

In Cultures 1 (GOG EN and Gold), `binkw32.dll` and `DPlayDLL.dll` collide, so one of them has to be rebased. On
Windows the loader would relocate the second one. Both `DPlayDLL.dll` builds now carry `base = 0x11000000` in the
manifest, and `recomp discover` works on the rebased image (the result agrees with the relocations, see below). `Mss32.dll` and `Mp3dec.asi` are HLE / not loaded, so they do not matter.

## Cultures 2 GOG executable

`Cultures2.exe` (GOG, identical in EN and DE) differs from every other binary:

- PE timestamp 2017-06-23, while the embedded build string is "Apr 11 2002 18:37:04", the same as `Cultures2MP.exe`.
- There is no Rich header. Import DLL names are lowercase, and there is a separate `.idata` section. `.text` has a page-aligned virtual size.
  The PE was rewritten by a tool.
- It has two extra sections, `.cms_t` (88 KiB, code-like entropy 6.4) and `.cms_d` (196 KiB).
- No direct `call`/`jmp rel32` from `.text` lands in `.cms_t`, and none goes back. There are 119 aligned 32-bit
  values in `.text` that fall into the `.cms_*` ranges. These could be data or indirect references.

Working hypothesis: GOG removed the original copy protection and left the protector's sections behind.
**(open)** Find out whether `.cms_t` is ever executed. If it is, it gets lifted like any other code.
If it is not, discovery excludes it. `Cultures2MP.exe`, which is untouched, is the control sample.

## Media formats

Findings from M1, verified by extracting every medium byte-identically against innoextract, unshield and unzip.

- **GOG installers (all 10):** Inno Setup data version `5.5.0 (u)`. The loader offset table (magic `rDlPtS…`,
  revision 1, CRC-32 valid) sits at file offset `0x3a7d4` in every installer. File data is embedded from `0x3b400`.
  The setup headers sit at the end of the exe. Header blocks use LZMA1. File chunks use **LZMA2** (header compression
  value 4). The GOG Galaxy helpers (`goggame-*.dll`, `webcache.zip`, …) are ordinary `{app}` files.
  Empty directories (`Data_m` in Cultures 1, `__support/app` in German Cultures 2) come from Inno directory entries.
- **Gold CD:** InstallShield 5 (header version `0x01005201`), one file group "Program Files". `DATA1.CAB` alone
  holds all 85 files (volume header: first file 0, last file 84). `DATA2.CAB` and `DATA3.CAB` (87 MB each,
  byte-identical, no InstallShield header) are filler, probably part of the CD's copy protection. The image uses the
  primary volume only (no Joliet). The long names that `isoinfo -R` shows come from Rock Ridge.
- **Saga zip:** stored entries (no compression), UTF-8 flagged names (`Österreich`, `Münzsammler`). Some local
  headers carry the name in another encoding than the central directory, so `unzip` warns. We use the central
  directory, as `unzip` does. Everything sits below one top-level folder `Cultures Saga/`.

## CD audio

- Cultures 1 plays music from Red Book CD tracks through MCI. GOG ships `MUSIC/Track02.ogg` … `Track16.ogg`
  and a replacement `winmm.dll` that redirects MCI CD commands to those files.
- Our `winmm` HLE does the same and maps CD track *n* to `MUSIC/TrackNN.{ogg,flac,wav}`.
- The Gold ISO has **no** audio tracks, because an ISO holds the data track only. The
  `gold-de` variant plays without music unless the user puts tracks into `MUSIC/` (ripped from their CD).
  The GOG music files cannot be shipped by us.
- C2 also contains a `CCDAudioManager`. Whether any C2-engine title uses CD audio in practice is **(open)**.

## Language handling

- C2 engine: the executable is language-neutral (`set_language N` in the ini, and per-language subfolders
  such as `FMV/Eng`). The single exception is the Spanish 8th Wonder `Game.exe`, which is an older build (2003-06-19).
- C1: the EN and DE GOG executables have different PE timestamps (2001-06 vs 2003-02) but identical Rich headers,
  and the Gold one is a third build. `recomp codediff` (M2) shows that EN and DE differ in **two functions** only
  (65 bytes of the whole file, plus strings); Gold differs in about 80 functions (see "Code discovery").

## File system behaviour to emulate

- Paths use backslashes and mixed case (`Data_FMV\Intro.bik` vs. the actual file `data_fmv/intro.bik`;
  the Spanish Northland ships `GAME.EXE`). The VFS has to be case-insensitive.
- Saves: `Save/` (C1), `Saves/` (C2 engine). The VFS redirects writes to a per-user directory, so the
  install directory can stay read-only.

## Code discovery (M2)

`recomp discover` results for the Cultures 1 binaries, with the manifest hints applied (addresses as in the manifest,
`DPlayDLL.dll` rebased to `0x11000000`). "Classified" is code + data + padding as a share of all code sections: every
byte of every C1 binary is classified. The DLL rows also hold when discovery ignores their relocations (`--no-relocs`):
then every fixup site lies inside an instruction or data, and every relocated pointer into code hits an instruction
start or data.

| Binary | Code bytes | Functions | No-return | Jump tables | Vtables | SEH scope tables | Classified |
|--------|-----------:|----------:|----------:|------------:|--------:|-----------------:|-----------:|
| `Cultures.exe` gog-en / gog-de | 669 844 | 3983 | 12 | 79 | 119 | 10 | 100 % |
| `Cultures.exe` gold-de | 673 780 | 3993 | 12 | 79 | 119 | 10 | 100 % |
| `binkw32.dll` gog-en / gold-de | 202 314 | 772 | 16 | 45 | 0 | 3 | 100 % |
| `binkw32.dll` gog-de | 211 590 (31 sections) | 753 | 16 | 46 | 0 | 3 | 100 % |
| `DPlayDLL.dll` gog-en | 66 290 | 347 | 18 | 48 | 0 | 14 | 100 % |
| `DPlayDLL.dll` gog-de / gold-de | 61 922 | 317 | 18 | 48 | 0 | 9 | 100 % |

What the last bytes were, and how each case is settled:

- **Text in `.text`:** the CRT's 8-byte `VC20XC00` marker in front of `__except_handler3` (every binary) and RAD's
  copyright notice in `binkw32.dll`, which has CR/LF in it and ends in `int3` padding or at the end of its section.
  Discovery now takes such a run of at least 8 printable characters as a string when it ends at a NUL, at padding, at
  known code or at the section end.
- **Unreferenced CRT code that runs into a second entry:** `strchr` in `DPlayDLL.dll` (nothing calls it, but
  `strstr` calls its second entry `__from_strstr_to_strchr`). Gap code may fall into a known function only when it
  starts at a 16-byte boundary after padding, where MSVC puts functions; elsewhere such a run is more likely data in
  front of the function (the `VC20XC00` case).
- **Unreferenced x87 error stubs in `binkw32.dll`** (about 60 bytes per build): no pointer or relocation leads to
  them, they are not aligned, and they run into shared tails. They are `seeds` in the manifest.
- **`no-return?` candidates:** the CRT's x87 argument loader (`0x49d8f5` GOG, `0x49e855` Gold, and one per
  `binkw32.dll` build) returns; the routine after each call to it is a second entry. These are `returns` hints. The
  CRT's `_exit` in `DPlayDLL.dll` (`0x1100b1f4` gog-en, `0x1100b143` gog-de/Gold) is a `no_return` hint, as in
  `Cultures.exe`.
- **Indirect jumps that stay unresolved** (3 in `Cultures.exe`, 2 or 6 in `binkw32.dll`, 4 in `DPlayDLL.dll`) are
  not `switch` tables and are dispatched at run time: the CRT's x87 dispatch through a per-function handler table
  (`jmp [ebx]`), a virtual tail call (`jmp [eax+0x30]`), the CRT's unwind/SEH helpers (`jmp eax`) and `longjmp`
  (`jmp [ebx+0x18]`), and in the gog-de `binkw32.dll` jumps through function pointers in `.data`. Their targets are
  all discovered through the tables and pointers that hold them.
- **FDIV stubs in `binkw32.dll`:** the CRT's Pentium FDIV workaround has 16 five-byte stubs ending in `int 6`. With
  relocations they are separate no-return entries; without, they decode as one run. Both are code either way.

Facts that the lifter (M3/M4) depends on:

- **No RTTI** in any C1 binary (no `.?AV` type descriptors; built without `/GR`). Vtables are found through the
  constructors' `mov [obj], offset vtable` stores into `.rdata` instead (119 in `Cultures.exe`, 1937 slots).
- **C++ EH is almost unused**: one `FuncInfo` (magic `0x19930520`) in `Cultures.exe`, one in `binkw32.dll`, three
  in `DPlayDLL.dll`. SEH (`__except_handler3` frames) appears in the CRT only.
- **Functions are packed** without alignment padding; padding (`int3`, `nop`, MSVC's multi-byte `lea` nops) only
  occurs inside the CRT's assembly routines and in RAD's code.
- **Data inside `.text`** is limited to `switch` jump tables (MSVC puts them after the function), their index byte
  tables, the CRT's hand-written `memcpy`/`memmove` tables, and a few bytes of CRT signature data. The tables of
  `memcpy`/`memmove` are indexed with negative or 1-based indices (`jmp [ecx*4 + table]` after `neg ecx`, or
  `[eax*4 + table - 4]`), so the address in the instruction is not where the table starts.
- **MMX is used**: 220 MMX instructions in `Cultures.exe` and about 4150 in `binkw32.dll` (Bink's decoder). There is no SSE,
  3DNow! or `cmov`. x87 is everywhere (about 1900 instructions in `Cultures.exe`). M4 therefore needs MMX, not only x87.
- **No-return functions:** the CRT's `_exit` (`doexit(code, 1, 0)`) cannot be proven no-return without
  constant propagation, so it is a manifest hint (`0x49cd22` GOG, `0x49dc82` Gold; `DPlayDLL.dll` likewise). The game's own fatal-error
  routine follows from it. Calls that fall straight into another function are reported as `no-return?`
  candidates, but they are not applied automatically: the CRT's x87 math routines (`sin` → `_CIsin` …) have a
  second entry right after a returning call. Each candidate is settled by a `no_return` or a `returns` hint.
- **Indirect control flow:** about 720 indirect calls (vtables, `mov esi, [IAT]; call esi`) and 3 unresolved
  indirect jumps in `Cultures.exe`. 214 of its 230 imports are called directly through the IAT.
- **Build differences (research question 5):** GOG EN vs DE: 3981 of 3983 functions identical. GOG vs Gold: 98.1 % of
  the functions (91.4 % of the code bytes) are identical up to addresses. About 77 functions changed and 10 more
  were added in Gold (the add-on). Function addresses differ between GOG and Gold, so the manifests keep per-build
  hints. The German GOG release also ships other library builds than EN/Gold: its `binkw32.dll` shares only 54 %
  of its functions with the EN one, and its `DPlayDLL.dll` (identical to Gold's) 82 %.
- **C2 engine (first look, not yet a target):** the same discovery classifies 99.99 % of `Cultures2MP.exe` and
  99.91 % of Northland's `Game.exe` with no hints. GOG's rewritten `Cultures2.exe` marks every section as
  plain data (no execute or code flag); discovery then treats the entry point's section as code and reaches 99.68 %.
  No path from the discovered `.text` code leaves `.text` (so none enters `.cms_t`), which supports the
  hypothesis that `.cms_t` is dead (research question 1, still **(open)**).

## Lifting (M3)

`recomp lift` results for the Cultures 1 binaries (with the manifest hints):

| Binary | Functions (label entries) | Instructions | Trapping instructions | C |
|--------|--------------------------:|-------------:|----------------------:|--:|
| `Cultures.exe` gog-en / gog-de | 3991 (8) | 221 344 | 237 | 42 MB |
| `Cultures.exe` gold-de | 4006 (13) | 221 424 | 237 | 42 MB |
| `binkw32.dll` gog-en / gold-de | 774 (2) | 60 038 | 4162 | 11 MB |
| `binkw32.dll` gog-de | 755 (2) | 61 926 | 4163 | 11 MB |
| `DPlayDLL.dll` gog-en | 350 (3) | 21 815 | 0 | 4 MB |
| `DPlayDLL.dll` gog-de / gold-de | 320 (3) | 20 249 | 0 | 4 MB |

- Every integer, string and x87 instruction is modelled. The traps in `Cultures.exe` are MMX (about 220), the 16
  `int 6` of the CRT's FDIV stubs and one `int3`; in `binkw32.dll` almost all are MMX.
- **MMX detection:** `Cultures.exe` has one `cpuid` (`0x48fb46`, in the function at `0x48fb20`): it tests EDX bit 23,
  caches the answer in two bytes at `0x4cf01c`/`0x4cf01d` and returns it, and its callers branch on it.
  `binkw32.dll` does the same in the function at `0x100167d0` (with the CPUID-support check through EFLAGS bit 21)
  and stores the answer at `0x10041b98`. The runtime's `cpuid` reports no MMX, so both probably take their
  non-MMX paths only (**(open)**, question 10: a trap at run time would show otherwise).
- Labels that need entries of their own are few: SEH filters and handlers and code pointers into function bodies.
  Most labels in the DLLs are relocated jump-table entries, which the lifted `switch` reaches.

## Start-up (M5)

What `Cultures.exe` (GOG EN) does before its first frame, as `RC_TRACE=imports` shows it:

- The C runtime starts three times (the executable and the two lifted DLLs, whose `DllMain` runs first).
- **DirectX check**: `DirectDrawCreate` + `QueryInterface`, then `CoCreateInstance(CLSID_DirectMusic,
  IID_IDirectMusic2)` and `LoadLibraryA("DINPUT.DLL")`. If either fails, it shows "DirectX 7.0 required!" and quits.
  DirectMusic is not used otherwise: the object is released at once.
- It probes `D3D8.DLL` with `LoadLibraryA` (research question 6: only a probe so far) and the DirectPlay lobby class
  (`CoCreateInstance`, for lobby-started network games); both may be missing.
- It scans the drive letters with `GetDriveTypeA` (looking for the CD), opens the CD with MCI (`cdaudio`) and asks for
  its tracks, and reads `HKLM` keys (none exist in a fresh registry; it goes on with defaults).
- Display: DirectDraw 4 (`DirectDrawCreateEx`, `EnumDisplayModes`), exclusive mode at 640 x 480; mouse and keyboard
  through DirectInput, cursors from `.cur`/`.ani` files (`LoadCursorFromFileA`), sound through Miles' quick API,
  Bink's sound through Miles streams (`BinkOpenMiles`).

## Open questions (summary)

1. Is `.cms_t` in the GOG `Cultures2.exe` executed?
2. How do Northland and 8th Wonder play MPEG-1: DirectShow, MCI or their own decoder?
3. Which DirectDraw interfaces and pixel formats does each engine use exactly?
4. Does `gedx8musicdrv.dll` use the DirectMusic *synth* (DLS) or only segment playback, and which interfaces?
5. ~~How much code differs between the three C1 executables?~~ Answered in M2: EN/DE differ in 2 functions, Gold in about 80 (see "Code discovery").
6. What do the `D3D8.DLL` references do?
7. Do the `*MP.exe` builds differ in more than networking? (Single-player builds also contain `CDPNet`.)
8. Saga: is the extra code in `Game.exe` a disc check, and where does `GameSaga.exe` (2005) come from?
9. Where is the Drachenland campaign stored: in `data0001.lib`, and is it selectable from the main menu?
10. Are the MMX paths of `Cultures.exe` and `binkw32.dll` dead when `cpuid` reports no MMX? (See "Lifting".) Cultures 1 ran
    its menus and a scenario without reaching one. The C2 engine executes MMX without asking (`emms` at `0x45c2e9`), so
    M10 needs MMX in the lifter.
