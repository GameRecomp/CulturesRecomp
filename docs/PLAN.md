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
| M2  | Find the code (C1)                         | ✅ | Coverage report for the three `Cultures.exe` builds: ≥ 99 % of `.text` classified as code, data or padding. Jump tables and vtables resolved. Reached: 100 % for every C1 binary, every leftover settled by analysis or a manifest hint. |
| M3  | Lifter, integer core                       | ✅ | All integer and string instructions used by the C1 executables are lifted. Difftests are green. The generated C compiles warning-free. |
| M4  | x87 and the whole game                     | ✅ | Every function of `Cultures.exe` and `binkw32.dll` is lifted. No trap is left in reachable code. The executable links. |
| M5  | Boot to the main loop                      | ✅ | CRT start-up, `WinMain`, the registry and ini reads and file access through the VFS all work. The game creates its window and asks DirectDraw for a mode. |
| M6  | Picture and input                          | ✅ | Main menu rendered (16-bit DirectDraw → SDL3). Mouse and keyboard work. Cursors are shown. |
| M7  | Sound, music and video                     | 🔶 | Bink intro plays. Miles samples and streams work. CD music plays from `MUSIC/TrackNN.ogg`. |
| M8  | In game                                    | 🔶 | First campaign mission played to the end. Save/load round-trips. A one-hour soak runs without crash on Linux and Windows. |
| M9  | **Cultures 1 release**                     | ⬜ | gog-en, gog-de and gold-de all pass the same checklist (see "Definition of done"), including the add-on campaign on gold-de. Release v0.1. |
| M10 | **Cultures 2** (C2 engine)                 | 🔶 | `Cultures2.exe` discovered, lifted and booted. The `.cms_*` question is answered. DirectMusic music plays through the lifted `gedx8musicdrv.dll`. Campaign mission 1 is played through. Release v0.2. |
| M11 | **Northland, 8th Wonder and Saga**         | 🔶 | Both `Game.exe` builds (+ 8th Wonder ES) and the Saga run. DirectSound 8 works. MPEG-1 movies play. A campaign mission each (Saga: Drachenland and one community campaign) is played through. Release v0.3. |
| M12 | Multiplayer                                | 🔶 | DirectPlay 4 (C1) and DirectPlay 8 (C2 engine) over UDP/LAN between two recompiled builds. `*MP.exe` builds lifted where they are needed. |
| M13 | Editors                                    | 🟨 | Northland and 8th Wonder `Editor.exe` run: GDI, common controls, file dialogs. |
| M14 | Converter                                  | 🔶 | `recomp convert <installer|iso>` produces a runnable build in one step. Later the browser page (the toolchain is built for wasm32). |
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

### M2 – Find the code (C1) ✅

Test material: the three `Cultures.exe` builds plus `binkw32.dll` and `DPlayDLL.dll`, obtained with
`recomp extract` from the C1 GOG installers (EN, DE) and the Gold ISO (see WORKFLOWS.md).

- [x] `rc-discover` crate with the image model (`Image`: sections mapped at their VAs and zero-filled beyond the raw
      data, IAT slot → `dll!name`, exports, entry point, relocations, rebasing) and `iced-x86` as the decoder
- [x] `recomp discover <exe>`: run discovery, print the coverage report, write the code map to
      `generated/<game>/<binary>-<variant>/codemap.json` (git-ignored, input for the lifter)
- [x] Manifest hints per `[[binary]]`: `base` (rebasing) and `[binary.discover]` with `seeds`, `no_return`, `data`
      ranges and `jump_tables` (hex integers), for what analysis cannot find
- [x] Seeds: entry point, exports (DLLs), manifest seeds; imports through `call/jmp [IAT]`
- [x] Recursive descent with call and branch following. No-return functions as a least fixed point over whole
      discovery runs (`ExitProcess` & co., `_CxxThrowException`-style `RaiseException` calls, manifest hints).
      Calls that fall into another function are reported as `no-return?` candidates; the dead stack clean-up after
      no-return calls is classified as dead code
- [x] MSVC 6 specifics: SEH frames (`__except_handler3` scope tables: filters and handlers), C++ EH `FuncInfo`
      (unwind actions, catch blocks), vtables (no RTTI in C1: found through constructor stores), `switch` jump tables
      (`jmp [reg*4+table]`, bounded by `cmp`/`ja` or `and`) with their index byte tables, and the negative-index
      tables of the CRT's `memcpy`/`memmove`. Unbounded tables are resolved last, when the code around them is known
- [x] Constant scanning: aligned 32-bit values in `.data` and `.rdata` and 32-bit immediates in code that point into
      `.text` (for DLLs: exactly the relocated values). Every candidate is trial-decoded in a transaction and rolled
      back on implausible instructions or overlaps; gaps are then filled with padding, strings and unreferenced code
- [x] Coverage report (code, data, padding, unknown) per executable: **100 %** for all three `Cultures.exe`.
      `recomp codediff` compares two builds function by function (research question 5 answered, see RESEARCH.md)
- [x] Same for `binkw32.dll` and `DPlayDLL.dll` (100 %). `DPlayDLL.dll` is rebased to `0x11000000`, and
      discovery is checked against the relocations (`--no-relocs`: the heuristics alone agree with them)
- [x] Every leftover settled, as in TropicoRecomp's M2: no unknown byte in any C1 binary, every `no-return?`
      candidate checked by hand (`no_return` or the new `returns` hint), the unresolved indirect jumps explained as
      run-time dispatch (RESEARCH.md). Strings that end at padding or code and aligned gap code that falls into a
      second entry are found by discovery; unreferenced x87 stubs in `binkw32.dll` are `seeds`. The ignored test
      `orig_discover` checks exactly this, with the hints applied and with and without the DLLs' relocations
- [x] Later (M10): GOG `Cultures2.exe` 100 % (`.cms_t` answered under M10)
- [x] The C2-engine games: a `switch` bound through a register constant, assertions onto `int3`, and false pointers
      into real code are handled by general rules; every lifted binary is classified to 100 % except dead code (an
      unreferenced CRT helper in Northland, the CD check GOG patched out of the Spanish 8th Wonder)

### M3 – Lifter, integer core ✅

The lifter, its IR, the C emitter and the difftest are TropicoRecomp's (`rc-ir`, `rc-x86`, `rc-emit-c`,
`rc-difftest`, `tools/difftest/`), adapted to this project's code map and runtime (decision log).

- [x] Decoder (iced-x86), IR (registers, flags as values, memory ops, one terminator per instruction) on
      discovery's per-instruction `Flow` (`CodeMap::insns`, `Function::body`, in memory only)
- [x] Integer ALU, shifts and rotates, `mul`/`div` with their traps, string ops (`rep movs/stos/cmps/scas`),
      `setcc`/`cmovcc`, `bt*`, `bswap`; `cpuid` (a fixed P6-class CPU without MMX) and `rdtsc` in the runtime.
      Segment pushes and pops follow the flat model (`0x23`); values the instruction drops are not loaded
      (`popad`'s esp slot, `pop es/ds`), and the carry of `cmp x, 0` is the constant 0, so the C has no warnings
- [x] C emitter: deterministic (lifting twice gives identical bytes), `funcs.h`, `code_XXXX.c` per 64 KiB, the
      sorted table `table.c`, `binary.c` and the game's `set.c`; one symbol prefix per binary. Labels the program
      takes as values (SEH handlers, code pointers into a function) get entries of their own; a function entered
      above its lowest instruction starts with a `goto`
- [x] `rc-difftest`: all 1560 modelled instruction forms (175 mnemonics) and the fixtures from random states,
      lifted C against the real CPU in 32-bit mode: 18720/18720 states agree. `--coverage` on every C1 binary: every
      form they use is tested, except `cpuid`/`rdtsc` (answered by the runtime) and `push`/`pop es/ds` (the
      selector differs from the real one by design). Runs in CI
- [x] Traps for unmodelled instructions, each with a name and an address (`rc_trap`); `recomp lift` lists them
- [x] `recomp lift`: all seven C1 binaries in about 5 s; the generated C (about 42 MB per `Cultures.exe`) compiles
      with `-Wall -Wextra -Wpedantic -Werror` on gcc and clang and links into `cultures-cultures1`

### M4 – x87 and the whole game ✅

- [x] x87 on host doubles (stack, control word, compare and flags, `fist`/rounding modes, transcendental
      functions): the fast mode from TropicoRecomp, difftested
- [x] MMX (lifted with M10, see there). Before that: 237 trapping instructions in `Cultures.exe` (MMX, `int 6` of the CRT's FDIV stubs, one `int3`), about
      4160 in each `binkw32.dll`, none in `DPlayDLL.dll`. Both detect MMX with `cpuid` and branch on the result,
      and the runtime's CPU reports none, so the MMX paths are probably dead (RESEARCH question 10). Lift MMX
      only if a trap shows one is reached; no SSE, 3DNow! or `cmov` was found
- [x] DLL rebasing in the runtime: the loader applies a lifted DLL's relocations when it maps it at its manifest
      `base` (C1: `DPlayDLL.dll` at `0x11000000`, whose `DllMain` then runs as lifted code)
- [x] Lift all of `Cultures.exe`, `binkw32.dll` and `DPlayDLL.dll` (all builds), compile and link
- [x] No trap in reachable code so far: start-up, the intro movie and the menus run without one (checked further
      while playing, M8)

### M5 – Boot to the main loop ✅

The runtime, HLE and platform layers are TropicoRecomp's (decision log); Cultures 1 needed the rest.

- [x] TEB/PEB, the `fs` segment, the process heap, `GetModuleHandle`/`GetProcAddress`/`LoadLibrary` (the DirectX
      DLLs the game loads at run time: `DDRAW`, `DINPUT`, `DSOUND`; `D3D8.DLL` is probed and not provided)
- [x] File access through the install (case-insensitive, read-only) and a writable per-user directory
- [x] Registry (`advapi32`): the program's own, kept in `registry.txt` in the user directory; a fresh one has no
      keys, as a clean install, and the game falls back to its defaults
- [x] Every unimplemented import is fatal with its name and caller; `--check` lists them without running the game
      (all 230 imports of `Cultures.exe` are covered: HLE or the lifted DLLs)
- [x] The game's DirectX 7 check passes: it creates the DirectMusic object (a minimal `IDirectMusic`, registered
      with the new COM class registry) and loads `DINPUT.DLL`
- [x] `cultures-cultures1` picks the lifted builds by SHA-256, runs the C runtime start-up of all three binaries,
      the DLLs' `DllMain`, and reaches the game's main loop

### M6 – Picture and input ✅

- [x] `ddraw` (TropicoRecomp's): the game's display mode (640 x 480), surfaces, `Lock`/`Unlock`, `GetDC`,
      palettes, `Flip` → SDL3; the intro movie (lifted Bink) and the main menu render
- [x] `user32` message pump; `GetKeyState`, `GetMessageA`, `wsprintfA`, `SetCursorPos`, `ClipCursor`; cursors from
      the game's `.cur`/`.ani` files (`LoadCursorFromFileA`)
- [x] `dinput`: keyboard and mouse, immediate (`GetDeviceState`) and buffered (`GetDeviceData`), fed from the same
      input user32 gets; `gdi32`: palettes, `SetPixel`/`GetPixel`, `SetBkColor`
- [x] Scripted runs (`RC_INPUT`) click through the menus, start a scenario ("The Last Bastion"), close the briefing
      with the keyboard and open a building's panel; screenshots of each step. The game switches to 1024 x 768 in 256
      colours for the map (DirectDraw palettes)
- [x] All three builds (GOG EN, GOG DE, Gold DE) reach their menus
- [x] The minimap: black is the unexplored map; it shows the view frame and the explored land around the ship
- [ ] By hand on a desktop: window and full-screen modes, scrolling

### M7 – Sound, music and video 🔶

Miles' quick API, samples and streams (TropicoRecomp's `mss32`, plus `AIL_quick_copy`) carry the game's sounds and
Bink's; CD audio (`winmm` MCI) plays `MUSIC/TrackNN.ogg` through SDL3_mixer.

- [x] `mss32`: the digital driver, samples (the game's sound effects), streams, volume and pan, timers
- [x] `dsound` on the platform's PCM streams (Cultures 1 imports only `DirectSoundCreate`, for its device check;
      the C2-engine games play through it, M11)
- [x] `winmm` MCI CD audio → `MUSIC/TrackNN.{ogg,flac,wav}`; Gold ISO: no music unless the user provides the tracks
- [x] Bink intro and outro through the lifted `binkw32.dll` (`BinkOpenMiles` → our `mss32`)
- [ ] By ear on a desktop: volume balance, music against effects, the options' volume sliders

### M8 – In game

- [x] Save and load (scripted): the in-game menu saves to `SAVE/save0000.sav` in the user directory (2.7 MB); after a
      fresh start, Single Player → Load Game lists it with its map preview and loads it. The game keeps its settings
      in the registry (`HKLM\SOFTWARE\Funatics\Cultures`, `registry.txt`)
- [x] A scripted soak in a running scenario (30,000 frames with random clicks): no trap, no crash
- [ ] Play the campaign. Fix traps and HLE gaps as they show up
- [ ] Save/load and the options menu. Timing (`timeGetTime`, frame limiter) must match the original game speed
- [x] One-hour soak test with automated input: 216,000 frames of random clicks in a scenario (virtual clock), no trap
- [x] Record and replay input: `RC_RECORD=path` writes the player's input as an `RC_INPUT` script (exact with
      `RC_CLOCK=virtual`); checked only headless so far, where there is no input to record

### M9 – Cultures 1 release

- [ ] All three variants: GOG EN and DE and Gold DE start; Gold offers both campaigns and the add-on's first mission
      ("Der Aufbruch") starts. Open: when `Intro2`/`Extro2` play (neither the campaign start nor the Intro menu)
- [ ] Packaging per platform. The user points the build at their install; nothing is bundled
- [ ] Release notes with the known issues: draft in `docs/RELEASE-NOTES.md`, final once the items above are done

### M10 – Cultures 2

- [x] Started on the shared toolchain, runtime and HLE: `Cultures2.exe` (5344 functions), its `binkw32.dll` (the same
      build as German Cultures 1's) and `gedx8musicdrv.dll` lift; the shell picks them by hash and the game boots
      through its initialisation. New for it: a `switch` bounded through a stack slot (`cmp [esp+4], n` before
      `mov eax, [esp+4]`; general in discovery), `emms` (MMX state), Winsock as an offline network (wsock32 and
      ws2_32 by ordinal), `MoveFileA`, `GetTempFileNameA`, `SetThreadPriority`, `ShellExecuteExA`, `CoCreateGuid`
- [x] DirectMusic as silent objects (`hle/dmusic`): performance, loader, segments and their states, audio paths and
      the composer answer as when music plays, without sound. Miles keeps up to 4096 samples (Cultures 2 loads about
      1100 at start; the divide by zero at `0x473199` came from samples that did not load)
- [x] Cultures 2 asks for its serial when `HKCU\Software\Funatics\Cultures2\SerialNumber` is missing. GOG players
      never see that: the GOG installer writes the serial into the registry. `recomp extract` now keeps the
      installer's registry entries (`registry-install.txt` in the unpacked folder), which the runtime loads first, so
      the GOG versions start without the prompt. GOG's CD-check patch (drive `.`) passes unchanged
- [x] MMX in the lifter: eight 64-bit registers in `rc_cpu` (kept apart from the x87 registers, which programs
      separate with `emms`), the operations as C functions in `rc/mmx.h`; the difftest loads all eight registers from
      its window, runs the instruction and stores them back, so every MMX form is checked against the real CPU (1766
      forms, 222 mnemonics, all agree). No MMX trap is left in any lifted binary; only the CRT's `int 6` and `int3`
- [x] Music: `hle/dmusic/player.c` plays the segments (tempo, band, pattern and sequence tracks) through a DLS
      synthesizer (`dls.c`: the games' own .dls files, regions, loops, envelope) into a PCM stream; the loader
      finds segments and the collections bands name in its search directory (RESEARCH "DirectMusic music")
- [x] German GOG version ("Die Tore Asgards"): converts with `recomp convert`
- [x] Past the serial prompt (installer registry, above): main menu, the campaign map, and the first mission
      ("Greenland") starts and runs (scripted, 12,000 frames). Discovery needed the false-pointer rule and a narrower
      `int3` rule (RESEARCH "Code discovery")
- [ ] The campaign's first mission played through
- [x] Research question 1: `.cms_t` does not run up to the serial prompt; `Cultures2MP.exe` (no protection, same
      version, also lifted) is the fallback if it does later
- [x] Discovery and lifting reuse M2–M4 (new manifest seeds only)
- [x] `ole32` COM class registry; `dmusic` with our own DLS synthesizer (see above)
- [x] `dpnet`: DirectPlay 8 on the shared network (M12)
- [x] Winsock by ordinal: offline (the lobby only)

### M11 – Northland, 8th Wonder and Saga

- [x] Northland (GOG EN), 8th Wonder (GOG EN) and the Saga (`GameSaga.exe` and `Game.exe`) lift with no new work in
      discovery or the lifter and reach their main menus. New imports: the clipboard (not shared with the host),
      `mmio*` without `mmioOpen` (never reached: errors), `DirectSoundCreate8` (no device yet)
- [x] Research question 2 answered: the movies go through DirectShow (`CoCreateInstance(CLSID_FilterGraph,
      IID_IGraphBuilder)`); without it the games skip them
- [x] DirectSound 8 (`hle/dsound`): buffers in guest memory fed into the platform's PCM streams from the play
      position, looping and one-shot, volume, frequency, duplicates; the games create a buffer per sample (over 700)
- [x] Movies: the games use DirectShow's multimedia streams (`CLSID_AMMultiMediaStream`, `IDirectDrawMediaStream`,
      `IDirectDrawStreamSample::Update`) and draw the frames themselves. `hle/dshow` decodes the MPEG-1 files with
      pl_mpeg (`third_party/`, MIT) into a DirectDraw surface it makes, frame by frame on the clock, with their sound;
      the filter-graph interfaces are there too. Northland's intro plays
- [x] In game (scripted): Northland's first campaign mission ("The Plague", 8000 frames, a settler's panel),
      8th Wonder's first mission and the Saga's "Nordland" campaign start and run without a trap. DirectMusic needed
      a few more silent methods (audio-path configuration and objects, time conversions, latency)
- [ ] Northland `Game.exe` DE and ES (one build for all three; the German installer here is an incomplete download)
- [x] 8th Wonder German and Spanish (GOG): converted with `recomp convert`; both start the first campaign mission
      (scripted, 4500 frames)
- [ ] New effects (weather, earthquake) are engine code; they should need no HLE work beyond DirectDraw
- [ ] Saga: research questions 8 and 9. Lift `Game.exe` and `GameSaga.exe`. Patch the disc check if there is one. Load the community campaigns from `DataX/UserCampaigns/`

### M12 – Multiplayer

- [x] `dplay` (DirectPlay 4 and its lobby's address functions, below the lifted `DPlayDLL.dll`): TCP/IP provider,
      sessions found by a UDP query on the LAN, on this machine and at the typed address; players connect to the host
      over TCP, which numbers them, keeps their names and data and relays messages. Two Cultures 1 programs on one
      machine: the client finds the host's game, joins its lobby, the map is sent and both play the match
      (RESEARCH "DirectPlay")
- [x] `dpnet` (DirectPlay 8 peer and address objects) on the same network core (`net.c`): Northland's host
      creates a game, a second program finds it on the LAN, joins the lobby, the host puts it into a slot and starts,
      and both play the mission
- [x] 8th Wonder and the Saga: two programs host, join and start a map the same way (same `CDPNet` code as
      Northland; 8th Wonder "Land of the Dragons I", the Saga's co-op map "Schneetreiben" with computer players)
- [x] `GameMp.exe` answered: the game build for the internet lobby (`lobby.exe`, TriNodE's "Flownet" library on
      Winsock, a log-on to the publisher's game server). Neither the lobby nor its `lobby\*.ini` is in any install
      and the server is gone, so it is not needed: LAN and direct-IP games run in `Game.exe` (DirectPlay 8). It
      lifts and every import is covered, should a lobby ever be rebuilt (RESEARCH "GameMp.exe")
- [x] `Cultures2MP.exe` is not needed for multiplayer: it is `Cultures2.exe` without the copy protection (research
      questions 1 and 7); Cultures 2's own lobby (`Lobby.exe`) served a server that is gone
- [x] Cultures 2 multiplayer through its DirectPlay 8: two programs, host creates, client joins, the host starts
      "Economy: Island Rule" and both load the mission (scripted, the same steps as Northland's)
- [x] Determinism check: two recompiled Cultures 1 programs stay in lockstep for an hour on the real clock (65 min,
      about 30,000 messages each way, still in contact at the end, no desync message; the game ran on by itself,
      neither side gave commands). The virtual clock cannot test this (RESEARCH "DirectPlay")
- [ ] The same with both players giving commands, and for the C2-engine games (DirectPlay 8)

### M13 – Editors

- [x] A Windows desktop for programs without DirectDraw: `user32` window tree (frames, client areas, z-order,
      update regions, visible-region clipping), per-thread message queues and cross-thread `SendMessage`, hooks,
      menus (bars and pop-ups), dialogs from templates, the standard controls (button, static, edit, list box,
      combo box, scroll bar), `MessageBox`; `gdi32` with brushes, pens, regions, ROPs, DIBs and text
- [x] `comctl32`: tool bar, status bar, progress bar, track bar, tool tips, `DllGetVersion`
- [x] Northland's `Editor.exe` (MFC 4.2, static) starts with `--exe Editor.exe` and shows its frame window,
      menus, tool bar and status bar
- [x] `comdlg32`: open and save dialogs of our own inside the installation folder, `GetFileTitleA`
- [x] The editors of Northland, 8th Wonder and the Saga: a new map created, players set, terrain painted, saved
      as `.c2e` (with the exported `.c2m`) and opened again (scripted runs)
- [x] A map made in the editor played in the game: Northland, terrain painted and two builders placed, exported
      `.c2m` in `UserMaps`, started from Free Play (scripted)
- [ ] The editors' other tools (buildings, vehicles, scripts, goals, AI) tried by hand
- [ ] Tree and list views, if a dialog of the editors needs them

### M14 – Converter

- [x] `recomp convert`: identify (installer, CD image, zip or installed folder) → extract → lift every binary the
      manifest lifts → CMake build of the game's shell → `out/<game>/` with `play.sh`/`play.bat`
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
| 2026-11-19 | Code maps go to `generated/<game>/<binary>-<variant>/codemap.json` (lower-case file name plus the first variant of the `[[binary]]`), because one game has several builds of the same binary. Addresses in the JSON are hex strings. |
| 2026-11-19 | `DPlayDLL.dll` (both C1 builds) is rebased to `0x11000000` (`base` in the manifest); `binkw32.dll` keeps its preferred base. |
| 2026-11-19 | `runtime/`, `hle/` and `platform/` are TropicoRecomp's, replacing this project's smaller first versions: its kernel32, user32, gdi32, ddraw, Miles and platform layer already run a game of the same era. This project adds what is general (several lifted tables, DLL relocation, a COM class registry, input observers) and the modules Cultures needs (advapi32, dinput, dsound, dmusic, shell32, MCI CD audio). |
| 2026-11-19 | The lifter, IR, C emitter and difftest are TropicoRecomp's crates, copied and adapted (code map, image, runtime API) rather than rewritten; changes that are not project-specific are kept small so they can go back. This project keeps the C back end only. |
| 2026-11-19 | The runtime's `rc_cpu` takes TropicoRecomp's layout (`r[8]`, one byte per flag, x87 fast mode), with the registers also named for hand-written code. `rc_dispatch` looks up the HLE registry, then one sorted table per lifted binary. |
| 2026-11-19 | `cpuid` reports a P6-class CPU without MMX or SSE, as in TropicoRecomp, so the game and Bink take their plain code paths and MMX may never need lifting. |
| 2026-11-19 | Generated C is compiled with the project's full warning set (unlike TropicoRecomp's `-w`); warnings are fixed in the lifter, not silenced. |
| 2026-11-19 | As much code as possible is shared between the games: one toolchain, runtime, HLE and platform layer for all of them. Differences between games and builds go into the manifests or small game modules, not into copies of shared code, and changes to shared rules are checked against every target. |
| 2026-11-19 | M2 closes at TropicoRecomp's bar, not only at ≥ 99 %: no unknown byte in any lifted C1 binary, and every `no-return?` candidate settled by a hint. A function checked to return gets a `returns` hint, so the report only shows unchecked candidates. |
| 2026-11-19 | Discovery accepts a heuristic candidate (pointer constant, vtable slot, gap) only after trial decoding in a transaction that rolls back on implausible instructions or overlaps. Calls that fall into another function are reported as `no-return?` candidates and become `no_return` hints only after a manual check, because MSVC's x87 math routines have second entries right after returning calls. |
| 2026-11-18 | *Cultures – Die Saga* (German retail, supplied as a zip of an installed copy) is a separate game `saga` on the C2 engine, because its executables, data archive and content differ from Northland and 8th Wonder. Both single-player builds (`Game.exe` 2003, `GameSaga.exe` 2005) are pinned. |
