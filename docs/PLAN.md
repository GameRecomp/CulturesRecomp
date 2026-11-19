# Plan

## Goals

1. **All Cultures games and editions run natively** on Linux, Windows and macOS from their original executables:
   Cultures 1 (GOG EN/DE and the German Gold Edition with the add-on), Cultures 2, Northland and 8th Wonder
   (GOG, all languages listed in [TARGETS.md](TARGETS.md)), and *Cultures – Die Saga* (German retail, with the
   Drachenland campaign and the community campaigns).
2. **Faithful first, improvements second.** The game logic is the original code. Improvements (resolutions,
   window mode, quality-of-life) come as opt-in patches once a game plays through.
3. **One shared runtime and HLE layer** for all games. The second engine (C2, used by four titles) must cost far
   less than the first, because only the manifests and a few HLE modules are new.
4. **A converter** that turns a user's installer into a native build without manual steps (CLI first, browser later).
5. **Later:** Android and WebAssembly, multiplayer between recompiled builds, and the map editors.

Non-goals: shipping game code or data, asset conversion or remastering, supporting unknown or cracked
executables, network compatibility with original Windows builds.

## Order of work

```
 M0 ─ M1 ─ M2 ─ M3 ─ M4 ─ M5 ─ M6 ─ M7 ─ M8 ─ M9            Cultures 1 (pilot, engine C1)
                                         │
                                         └─ M10 ─ M11                      C2 engine: Cultures 2, Northland, 8th Wonder, Saga
                                                   │
                         M12 multiplayer · M13 editors · M14 converter · M15 polish (in parallel once M11 is done)
```

**Why Cultures 1 first:** it has the smallest executable (0.64 MiB of code). Its middleware (DirectDraw, Miles and Bink)
is exactly the set that other static-recompilation projects have already covered. The pilot therefore
proves the toolchain on the known path. **Why the C2 engine second:** one engine unlocks three games. Its
new problems (DirectMusic, MPEG-1 video, DirectPlay 8, the rewritten GOG `Cultures2.exe`) are isolated in HLE
modules, not in the recompiler.

Inside the C2 engine, the order is **Cultures 2 → Northland → 8th Wonder → Saga**. Cultures 2 shares Miles and Bink with C1.
Northland adds DirectSound 8 and MPEG. 8th Wonder is then mostly a manifest, apart from its Spanish build. The Saga is the youngest build of the same
engine and comes last. It needs only its own manifest seeds and, if `Game.exe` checks for the CD, a patch.

## Milestones

Status: ✅ done · 🔶 in progress · ⬜ not started

| #   | Milestone                                  | Status | Exit criterion |
|-----|--------------------------------------------|--------|----------------|
| M0  | Foundation                                 | ✅ | The repo, CMake and Cargo build, CI, the audit hook and all manifests exist. `recomp identify` recognises every file in TARGETS.md. |
| M1  | Read the inputs                            | ✅ | `recomp extract` unpacks all 10 GOG installers, the Gold ISO and the Saga zip into `orig/<game>/<variant>/` without innoextract/unshield. The PE loader maps every target. |
| M2  | Find the code (C1)                         | 🔶 | Coverage report for the three `Cultures.exe` builds: ≥ 99 % of `.text` classified as code, data or padding. Jump tables and vtables resolved. |
| M3  | Lifter, integer core                       | ⬜ | All integer and string instructions used by the C1 executables are lifted. Difftests are green. The generated C compiles warning-free. |
| M4  | x87 and the whole game                     | ⬜ | Every function of `Cultures.exe` and `binkw32.dll` is lifted. No trap is left in reachable code. The executable links. |
| M5  | Boot to the main loop                      | ⬜ | CRT start-up, `WinMain`, the registry and ini reads and file access through the VFS all work. The game creates its window and asks DirectDraw for a mode. |
| M6  | Picture and input                          | ⬜ | Main menu rendered (16-bit DirectDraw → SDL3). Mouse and keyboard work. Cursors are shown. |
| M7  | Sound, music and video                     | ⬜ | Bink intro plays. Miles samples and streams work. CD music plays from `MUSIC/TrackNN.ogg`. |
| M8  | In game                                    | ⬜ | First campaign mission played to the end. Save/load round-trips. A one-hour soak runs without crash on Linux and Windows. |
| M9  | **Cultures 1 release**                     | ⬜ | gog-en, gog-de and gold-de all pass the same checklist (see "Definition of done"), including the add-on campaign on gold-de. Release v0.1. |
| M10 | **Cultures 2** (C2 engine)                 | ⬜ | `Cultures2.exe` discovered, lifted and booted. The `.cms_*` question is answered. DirectMusic music plays through the lifted `gedx8musicdrv.dll`. Campaign mission 1 is played through. Release v0.2. |
| M11 | **Northland, 8th Wonder and Saga**         | ⬜ | Both `Game.exe` builds (+ 8th Wonder ES) and the Saga run. DirectSound 8 works. MPEG-1 movies play. A campaign mission each (Saga: Drachenland and one community campaign) is played through. Release v0.3. |
| M12 | Multiplayer                                | ⬜ | DirectPlay 4 (C1) and DirectPlay 8 (C2 engine) over UDP/LAN between two recompiled builds. `*MP.exe` builds lifted where they are needed. |
| M13 | Editors                                    | ⬜ | Northland and 8th Wonder `Editor.exe` run: GDI, common controls, file dialogs. |
| M14 | Converter                                  | ⬜ | `recomp convert <installer|iso>` produces a runnable build in one step. Later the browser page (the toolchain is built for wasm32). |
| M15 | Polish and more platforms                  | ⬜ | Opt-in high resolutions and widescreen, integer scaling, settings UI, performance work, then Android and WebAssembly. |

### M0 – Foundation ✅

- [x] Repository layout, MIT license, README and docs skeleton
- [x] CMake (C11) with presets (`dev`, `release`, `ci`), `cmake/lift.cmake` and generated per-game shells (`cultures-<id>`)
- [x] Runtime core with tests: 4 GiB guest memory, CPU state, dispatcher (hash table, fatal on unknown targets),
      HLE registry with trap thunks for unimplemented imports, case-insensitive VFS lookup, logging
- [x] Platform layer: SDL3 backend (window, letterboxed 16-bit present, input, clock) and a null backend
- [x] HLE module table covering every imported DLL; first functions `GetTickCount` and `timeGetTime`
- [x] Cargo workspace with `rc-pe`, `rc-manifest` and `rc-cli` (`recomp identify`, `recomp info`)
- [x] `games/*/game.toml` for all five games (incl. the Saga), with every installer and binary pinned
- [x] `tools/audit.sh`, the pre-commit hook (SPDX headers, no game files, no absolute paths) and the commit-msg hook
- [x] `tools/dev-extract.sh` (innoextract / unshield / unzip) as a stopgap until `recomp extract`
- [x] CI: audit, Rust fmt/clippy/test, C build and tests on Linux, Windows and macOS (only Linux was verified locally)

### M1 – Read the inputs ✅

- [x] PE parser: sections, imports by name and ordinal, exports, relocation directory, Rich header
- [x] Hash identification of installers, ISO and binaries
- [x] `rc-media` crate, one module per format, with a common `Archive` interface:
  - [x] Inno Setup 5.5.0 (unicode): loader table, setup header and entry lists, data entries, LZMA2 chunks,
        x86 call filter reversal, SHA-1 check of every file
  - [x] Own streaming LZMA1/LZMA2 decoder (no native dependency, wasm-friendly for M14)
  - [x] ISO 9660 (primary volume and Joliet)
  - [x] InstallShield 5 cabinets (`DATA1.HDR` + `DATA1.CAB`, chunked raw deflate, de-obfuscation)
  - [x] Zip (stored/deflate, UTF-8/CP437 names, CRC-32 check, empty directories)
- [x] `recomp list` and `recomp extract`: identify by hash, unpack to `orig/<game>/<variant>/`, verify every pinned binary
- [x] Verified: all 10 GOG installers, the Gold ISO and the Saga zip extract **byte-identical** to innoextract,
      unshield and unzip (including empty directories); `cargo test -p rc-cli -- --ignored` checks the media in `orig/`
- [x] PE loader in the runtime (`rc/pe.h`): maps headers and sections at the image base, fills the IAT with HLE
      thunks (by name and ordinal); `cultures-<id> --check` loads the user's exe and reports the missing imports
- [ ] Later (M14 or when needed): faster LZMA (currently ~25 MB/s, about 10–20 s per installer), InstallShield
      v6+ and spanned cabinets, Inno versions other than 5.5.0 (u)

### M2 – Find the code (C1) 🔶

Test material: the three `Cultures.exe` builds plus `binkw32.dll` and `DPlayDLL.dll`, obtained with
`recomp extract` from the C1 GOG installers (EN, DE) and the Gold ISO (see WORKFLOWS.md).

- [x] `rc-discover` crate with the image model (`Image`: sections mapped at their VAs and zero-filled beyond the raw
      data, IAT slot → `dll!name`, exports, entry point) and `iced-x86` (decoder + `instr_info`) as the decoder
- [ ] `recomp discover <exe>`: run discovery, print the coverage report, write the code map to
      `generated/<game>/<binary>/codemap.json` (git-ignored, input for the lifter)
- [ ] Manifest hints per `[[binary]]`: `seeds`, `no_return`, `data` ranges (hex integers), for what analysis cannot find
- [ ] Seeds: entry point, import thunks, exports (DLLs), manifest seeds
- [ ] Recursive descent with call and branch following. Handle no-return functions (`exit`, `_CxxThrowException`, …)
- [ ] MSVC 6 specifics: SEH frames (`__except_handler3`), C++ EH funcinfo, RTTI → vtables → methods,
      `switch` jump tables (`jmp [reg*4+table]`) together with their index byte tables
- [ ] Constant scanning: aligned 32-bit values in `.data` and `.rdata` that point into `.text`. This is the substitute
      for the missing relocations, and its results are cross-checked against function prologues
- [ ] Coverage report (code, data, padding, unknown) per executable. Diff of the three C1 builds (research question 5)
- [ ] Do the same for `binkw32.dll` and `DPlayDLL.dll` (both prefer base `0x10000000`, see M4: discover the rebased image)

### M3 – Lifter, integer core

- [ ] Decoder (iced-x86), IR design (registers, lazy flags, memory ops, terminators)
- [ ] Integer ALU, shifts and rotates, `mul`/`div`, string ops (`rep movs/stos/cmps/scas`), `setcc`/`cmovcc`, `bt*`, `bswap`, `cpuid`/`rdtsc` as HLE
- [ ] C emitter, deterministic file layout, `functions.h`, dispatch table
- [ ] `rc-difftest`: random register and memory states, compared with a reference CPU, per instruction class
- [ ] Traps for unmodelled instructions, each with a name and an address

### M4 – x87 and the whole game

- [ ] x87 on host doubles (stack, control word, compare and flags, `fist`/rounding modes, transcendental functions)
- [ ] MMX/SSE: not expected in these binaries; verify, and trap if any is present
- [ ] DLL rebasing: `binkw32.dll` and `DPlayDLL.dll` (C1) both prefer `0x10000000`. Each lifted DLL gets a fixed,
      conflict-free base in the manifest; the recompiler applies its relocations before discovery and lifting, and the
      runtime loader (`rc/pe.h`, currently fixed-base only) applies the same relocations when mapping it
- [ ] Lift all of `Cultures.exe` and `binkw32.dll`, compile and link. Per-function hot-reload for debugging is optional

### M5 – Boot to the main loop

- [ ] TEB/PEB, the `fs` segment, the process heap, `GetModuleHandle`/`GetProcAddress` (needed for `DDRAW.DLL`/`DINPUT`)
- [ ] VFS: case-insensitive lookup, a read-only install directory, a writable per-user overlay (`Save/`, ini, registry)
- [ ] Registry emulation (`HKLM\Software\...` paths the games read, seeded from the manifest)
- [ ] Log every unimplemented import, with the address of the caller

### M6 – Picture and input

- [ ] `ddraw`: `DirectDrawCreate(Ex)`, `IDirectDraw2/4/7`, cooperative level, display modes, primary and back buffer,
      offscreen surfaces, `Blt`/`BltFast` with colour keys, `Lock`/`Unlock`, `Flip`, palettes (8-bit paths)
- [ ] `user32` message pump, `WM_*` from SDL events, `ShowCursor`/`SetCursor` with `.cur` files
- [ ] `dinput`: keyboard and mouse, buffered and immediate
- [ ] Present: 16-bit (565/555) → texture, aspect-correct scaling, window and fullscreen

### M7 – Sound, music and video

- [ ] `mss32`: `AIL_startup`, the digital driver, samples, streams (WAV/ADPCM/MP3), volume and pan, timers
- [ ] `dsound` (also used by C1 directly) on our own SDL3-based mixer
- [ ] `winmm` MCI CD audio → `MUSIC/TrackNN.{ogg,flac,wav}`; Gold ISO: no music unless the user provides the tracks
- [ ] Bink intro and outro through the lifted `binkw32.dll` (`BinkOpenMiles` → our `mss32`)

### M8 – In game

- [ ] Play the campaign. Fix traps and HLE gaps as they show up
- [ ] Save/load and the options menu. Timing (`timeGetTime`, frame limiter) must match the original game speed
- [ ] One-hour soak test with automated input. Record and replay input for regression tests

### M9 – Cultures 1 release

- [ ] All three variants. Gold: add-on campaign, `Intro2`/`Extro2` movies
- [ ] Packaging per platform. The user points the build at their install; nothing is bundled
- [ ] Release notes with the known issues

### M10 – Cultures 2

- [ ] Research question 1: is `.cms_t` executed? Compare with the untouched `Cultures2MP.exe`
- [ ] Discovery and lifting reuse M2–M4 (new manifest seeds only)
- [ ] `ole32` COM class registry. `dmusic`: Loader, Performance, Segment, AudioPath as far as `gedx8musicdrv.dll` uses them.
      The DLS synth is either our own (DLS1 is small) or a permissively licensed one (to be decided in M10)
- [ ] `dpnet`: DirectPlay 8 with zero service providers, so the menus work and single player is unaffected
- [ ] Winsock by ordinal (only for the lobby, can be a stub)

### M11 – Northland, 8th Wonder and Saga

- [ ] Research question 2: MPEG playback path → `dshow` or MCI-MPEG HLE on top of an MPEG-1 decoder
- [ ] `DirectSoundCreate8` and the `IDirectSound8` buffers
- [ ] Northland `Game.exe` (one build for EN/DE/ES), 8th Wonder `Game.exe` EN/DE and ES
- [ ] New effects (weather, earthquake) are engine code; they should need no HLE work beyond DirectDraw
- [ ] Saga: research questions 8 and 9. Lift `Game.exe` and `GameSaga.exe`. Patch the disc check if there is one. Load the community campaigns from `DataX/UserCampaigns/`

### M12 – Multiplayer

- [ ] `dplay` (DirectPlay 4, below the lifted `DPlayDLL.dll`) and `dpnet` (DirectPlay 8) over UDP with LAN discovery
- [ ] `*MP.exe`: lift them if they are required for multiplayer (research question 7)
- [ ] Determinism check: two recompiled builds stay in lockstep for one hour

### M13 – Editors

- [ ] `Editor.exe` for Northland, 8th Wonder and the Saga: `comctl32` (toolbars, trees, lists), `comdlg32` (file dialogs), more `gdi32`

### M14 – Converter

- [ ] `recomp convert`: identify → extract → lift → invoke CMake → result in `out/<game>/`
- [ ] Prebuilt runtime packages, so users need no compiler: native back end via Cranelift (as in comparable
      projects) or shipping a C compiler toolchain (to be decided in M14)
- [ ] Browser converter (wasm32 build of the pipeline, local only, nothing uploaded)

### M15 – Polish and platforms

- [ ] Opt-in patches for higher resolutions (the C2 engine scales its map view; C1 needs research)
- [ ] Settings UI outside the game (window mode, scaling, audio device)
- [ ] Performance: native replacements for hot functions (blitters, `memcpy`-like loops)
- [ ] Android (touch → mouse), WebAssembly (single-threaded runtime)

## Definition of done (per game and variant)

- The intro movie, main menu, options, campaign mission 1 through to the end, save and load all work
- One hour of play without crash or trap on Linux and Windows. macOS builds and starts
- No unimplemented import is hit during that run (the log is clean)
- `tools/audit.sh` passes and the docs are updated (TARGETS, RESEARCH, the status table above)
- Difftests are green and the generated C compiles with `-Wall -Wextra` without new warnings

## Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| No relocations in any target, so code/data separation is hard | coverage gaps, so dispatch misses at run time | constant scanning + prologue heuristics + RTTI/vtables; fatal dispatch misses name the address → manifest seed |
| GOG `Cultures2.exe` was rewritten (`.cms_*`) | unexpected code paths | research question 1 first in M10. `Cultures2MP.exe` as control sample |
| DirectMusic is a large API | C2-engine music is late | lift `gedx8musicdrv.dll` and implement only the interfaces it calls. Music may come after first playable |
| MPEG-1 playback path unknown | Northland/8th Wonder movies | movies are skippable. Use a small MIT decoder (pl_mpeg) behind whichever API turns out to be used |
| Timing-sensitive game loop | wrong game speed, desync in multiplayer | an exact `timeGetTime` model, a frame limiter mirroring the original, record/replay tests |
| Many variants (9 single-player executables) | maintenance | variants share HLE and patches. Per-variant data is only addresses in `game.toml`. Diff tooling from M2 |
| Legal | the project could be taken down | strict audit: no game bytes, no generated code, no disassembly in the repo, issues or CI artifacts |

## Decision log

| Date       | Decision |
|------------|----------|
| 2026-11-18 | MIT license. Generated code and runtime in C11. The toolchain in Rust (same split as comparable recompilation projects). |
| 2026-11-18 | Targets are the GOG releases (EN, DE, ES as present in `orig/`) plus the German Cultures Gold ISO, because it is the only source of the add-on *Die Rache des Regengottes*. Other builds are added only after hashing them into a manifest. |
| 2026-11-18 | Cultures 1 is the pilot. The C2 engine follows in the order Cultures 2 → Northland → 8th Wonder. |
| 2026-11-18 | `binkw32.dll`, `DPlayDLL.dll` and `gedx8musicdrv.dll` are lifted. Miles (`mss32`) and all system DLLs are HLE. GOG's `winmm.dll` wrapper is not used. |
| 2026-11-18 | SDL3 is the only platform dependency. The project uses host fonts only and bundles no assets. |
| 2026-11-18 | Docs, code and comments are in English. |
| 2026-11-19 | Lifted DLLs are rebased at lift time to a fixed base from the manifest (C1's `binkw32.dll` and `DPlayDLL.dll` collide at `0x10000000`). The executables keep their own base `0x400000`. |
| 2026-11-19 | One crate `rc-media` for all media formats instead of `rc-inno`/`rc-iso`/`rc-iscab`/`rc-zip`. The LZMA1/LZMA2 decoder is our own: `lzma-rs` buffers whole LZMA2 streams in memory (250+ MB here), and a pure-Rust decoder keeps the browser converter possible. |
| 2026-11-19 | All lifted binaries of a game link into one executable `cultures-<id>`. Each binary gets its own symbol prefix, and the shell picks the binary whose hash matches the user's copy. |
| 2026-11-19 | HLE thunks live at `0xF0000000+` (kernel space for a Win32 guest, never touched by the games). Unknown imports resolve to trap thunks, so a game loads with gaps and fails only when it calls one. |
| 2026-11-19 | Store extras (GOG Galaxy DLLs, GOG's Ogg/Vorbis wrapper dependencies, uninstallers) are skipped by name in `recomp identify`, not pinned. |
| 2026-11-18 | *Cultures – Die Saga* (German retail, supplied as a zip of an installed copy) is a separate game `saga` on the C2 engine, because its executables, data archive and content differ from Northland and 8th Wonder. Both single-player builds (`Game.exe` 2003, `GameSaga.exe` 2005) are pinned. |
