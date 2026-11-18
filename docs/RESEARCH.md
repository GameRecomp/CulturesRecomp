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
- C1: the EN and DE GOG executables differ (2001-06 vs 2003-02 build), and the Gold one is a third build.
  Whether this is only strings or also code changes is **(open)**. A diff of discovered functions in M2 answers it.

## File system behaviour to emulate

- Paths use backslashes and mixed case (`Data_FMV\Intro.bik` vs. the actual file `data_fmv/intro.bik`;
  the Spanish Northland ships `GAME.EXE`). The VFS has to be case-insensitive.
- Saves: `Save/` (C1), `Saves/` (C2 engine). The VFS redirects writes to a per-user directory, so the
  install directory can stay read-only.

## Open questions (summary)

1. Is `.cms_t` in the GOG `Cultures2.exe` executed?
2. How do Northland and 8th Wonder play MPEG-1: DirectShow, MCI or their own decoder?
3. Which DirectDraw interfaces and pixel formats does each engine use exactly?
4. Does `gedx8musicdrv.dll` use the DirectMusic *synth* (DLS) or only segment playback, and which interfaces?
5. How much code differs between the three C1 executables?
6. What do the `D3D8.DLL` references do?
7. Do the `*MP.exe` builds differ in more than networking? (Single-player builds also contain `CDPNet`.)
8. Saga: is the extra code in `Game.exe` a disc check, and where does `GameSaga.exe` (2005) come from?
9. Where is the Drachenland campaign stored: in `data0001.lib`, and is it selectable from the main menu?
