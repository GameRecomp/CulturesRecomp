# Architecture

## Overview

```
 your game files            recompiler (Rust, recomp/)                    build (CMake, C11)
 ─────────────────          ──────────────────────────────────            ─────────────────────────────
 GOG installer  ─┐
 Gold ISO        ├─ extract ─► orig/<game>/<variant>/                     generated/<game>/<variant>/*.c
 installed dir  ─┘                   │                                              │
                                     ▼                                              ▼
                    load ─► discover ─► lift ─► emit C  ──────────────►  + runtime/  + hle/  + platform/
                    rc-pe   rc-discover rc-x86  rc-emit-c                 + games/<game>/ (shell, patches)
                            ▲                                                       │
                     games/<game>/game.toml (hashes, seeds, patches)                ▼
                                                                          native executable per game
```

The recompiler runs **ahead of time**. It turns each pinned executable (and each DLL marked
`lift`) into C. The C build links that code against the runtime. The runtime provides the
guest memory, the dispatcher and the HLE implementations of every imported API. At run time the game
reads its own data files from the user's install. We never convert or redistribute assets.

## Pipeline (recomp/)

| Stage       | Crate          | Job |
|-------------|----------------|-----|
| identify    | `rc-manifest`  | Load `games/*/game.toml`, match files by SHA-256 against all variants. |
| load        | `rc-pe`        | Parse the PE: sections, imports (by name and ordinal), exports, relocations, Rich header. Refuse any binary whose hash is not in a manifest. |
| extract     | `rc-media`     | Read GOG Inno Setup 5.5 installers (own LZMA1/LZMA2 decoder), ISO 9660 images, InstallShield 5 cabinets and zip archives of installed copies (the Saga) without third-party tools. Every file is checked (SHA-1, CRC-32 or the manifest hash). |
| discover    | `rc-discover` | Build the code map (`recomp discover`), because **no executable has relocations**. Recursive descent from the entry point, exports and manifest seeds, with `call`/`jmp [IAT]` imports and `switch` jump tables (bounded by `cmp`/`ja` or `and`, with index byte tables). Then candidates, each trial-decoded in a transaction that rolls back on implausible code: immediates and data constants that point into code (for DLLs exactly the relocated values), vtables from constructor stores (C1 has no RTTI), SEH scope tables and C++ EH `FuncInfo`. Then gap filling (padding, strings, unreferenced code). No-return functions are a fixed point over whole runs. Output: functions with byte ranges, labels, jump tables, vtables, data/padding/dead/unknown ranges, imports called, CPU features and the coverage report. DLLs can be rebased first (manifest `base`). `recomp codediff` compares two builds function by function. |
| lift        | `rc-x86` → `rc-ir` | Decode with `iced-x86` and turn each instruction into IR statements plus one terminator, using discovery's per-instruction `Flow` (import slots and jump tables already resolved). Integer, string and x87 (fast mode) instructions are modelled with exact flags; unmodelled ones (MMX so far) become named traps, not build failures. Labels that the program takes as values (SEH handlers, code pointers into a function) get an entry of their own: the owner's body lifted again, entered at the label. Shared with TropicoRecomp. |
| emit        | `rc-emit-c` | Write one C function per guest function (`<binary>_00401000`), grouped as `code_XXXX.c` per 64 KiB of guest address space, with `funcs.h` and the sorted table `table.c`. Registers and flags are C locals. A function entered above its lowest instruction starts with a `goto` to its entry. Output is deterministic. Shared with TropicoRecomp. |
| verify      | `rc-difftest` (dev only) | Every instruction form the lifter models, plus string-instruction and whole-program fixtures, run from random states on the real CPU in 32-bit mode (`tools/difftest/ref32.c`, freestanding, built with `clang -m32`) and as lifted C on the runtime (`tools/difftest/harness.c`). `--coverage EXE` checks that every form a binary uses is tested. Shared with TropicoRecomp (without its native and WebAssembly back ends). |
| convert     | `rc-convert` *(M14)* | End to end: installer → extract → lift → build, the user-facing path. |
| CLI         | `rc-cli` (`recomp`) | `identify`, `info`, `list`, `extract`, `discover`, `codediff`, `lift`; later `convert`. |

## Runtime model (runtime/)

- **Guest memory**: a 4 GiB reservation (on 64-bit hosts), with pages committed when the PE loader or the
  heap maps them. Guest address `a` is host `rc_mem_base + a`. The header `rc/mem.h` provides `rc_ld8/16/32` and
  `rc_st8/16/32` (little-endian, unaligned-safe). Image base `0x400000` is fixed, since there are no relocations,
  so the game's own addresses are used as they are.
- **CPU state**: `rc_cpu` (`rc/cpu.h`, TropicoRecomp's layout) holds the eight GPRs (`r[8]`, also named
  `eax`…`edi` for hand-written code), one byte per arithmetic flag plus `df`, the x87 in the fast mode (`rc/x87.h`:
  host doubles, top, control/status/tag words) and the `fs` base (TEB). Lifted functions take `rc_cpu *` and keep
  registers and flags in C locals inside a function. They write them back at calls, returns and indirect transfers.
  `cpuid` reports a fixed P6-class CPU without MMX or SSE, and `rdtsc` a 1 GHz counter.
- **Stack**: the guest stack lives in guest memory, so `esp` is a real guest pointer. Calling conventions
  (`stdcall`, `cdecl`, `thiscall`) therefore need no special handling.
- **Calls**: direct calls are C calls; transfers to another function's entry are tail calls. Indirect calls and
  jumps (vtables, function pointers, the indirect jumps discovery leaves to run time) go through
  `rc_dispatch(cpu, target)`. It looks the target up in the registry (HLE thunks, a hash table) and then in the
  sorted tables of the lifted binaries (`rc_dispatch_add_table`, one per binary). An unknown target is a fatal error
  that names the address, and gets fixed in the manifest. A trap (`rc_trap`) names the instruction and its address.
- **PE loader** (`rc/pe.h`): maps headers and sections of an exe or DLL at its preferred base and writes
  an HLE thunk address into every IAT slot. The executables have no relocations and always load at `0x400000`.
  The lifted DLLs do have relocations, and two of them collide (C1: `binkw32.dll` and `DPlayDLL.dll` both prefer
  `0x10000000`). Each lifted DLL that collides gets a fixed base in the manifest (`DPlayDLL.dll`: `0x11000000`), and the
  recompiler applies its relocations before discovery and lifting. The loader has to apply the same relocations (M4);
  until then it is fixed-base only.
- **HLE**: every imported function has the same signature as lifted code (`void f(rc_cpu *)`). It reads its
  arguments from the guest stack and pops them (`stdcall`). Imports get reserved guest addresses in a
  thunk region (`0xF0000000+`, 16 bytes apart), so `call [IAT]` lands in `rc_dispatch`, which calls the HLE
  function. An import without an implementation gets a *trap* thunk. The game loads anyway and fails with
  `unimplemented import dll!Name` only when it calls one. A later registration upgrades the trap in place.
- **COM**: DirectX interfaces are COM objects that live in guest memory. Their vtables point at thunk
  addresses, so the game's own vtable calls work unchanged.
- **x87**: a fast mode on host doubles first. An exact 80-bit mode is added only if a game depends on it (both engines
  are MSVC 6 builds that set 53-bit precision in places **(to verify)**).
- **Threads**: the games use few threads (audio, network). They are modelled as cooperative fibers first,
  and switch to real threads only if needed.

## HLE layer (hle/)

There is one directory per Windows DLL. Every module is shared by all games.

| Module      | Backed by                      | Needed by | Notes |
|-------------|--------------------------------|-----------|-------|
| `kernel32`  | libc / POSIX / Win32           | all       | VFS (case-insensitive, `\` → `/`, save redirection), heap, time, threads, TLS, `GetPrivateProfile*` |
| `user32`    | platform (SDL3)                | all       | window, message queue from host input, cursors (`.cur`), `MessageBox` |
| `gdi32`     | platform + SDL3_ttf            | all       | palette functions, `TextOut`/`SetPixel`, enough for the editors later |
| `advapi32`  | per-user registry file         | all       | `Reg*` on a small INI/JSON-backed registry hive |
| `ole32`     | runtime                        | all       | `CoInitialize`/`CoCreateInstance` → our COM classes (DirectMusic, DirectPlay 8, DirectShow) |
| `version`, `shell32` | runtime               | all       | version info of our own modules, `ShellExecute` → host browser or no-op |
| `ddraw`     | platform framebuffer           | all       | DirectDraw 1/2/4/7 COM, 16-bit surfaces, software blits, page flipping → SDL texture |
| `dinput`    | platform input                 | all       | DirectInput (keyboard, mouse) |
| `dsound`    | SDL3 audio                     | c1, northland, eighthwonder, saga | DirectSound / DirectSound8 buffers, our own mixer |
| `mss32`     | dsound HLE / SDL3 audio        | c1, c2    | Miles reimplementation: samples, streams, MP3 (`Mp3dec.asi` is not loaded) |
| `winmm`     | runtime clock + audio          | all       | `timeGetTime`, mixer API stubs, **MCI CD audio → `MUSIC/TrackNN.*`** |
| `dmusic`    | DLS synth + segment player     | c2, northland, eighthwonder, saga | DirectMusic Performance/Loader/Segment as used by `gedx8musicdrv.dll` |
| `dshow` or `mci-mpeg` | MPEG-1 decoder (pl_mpeg, MIT) | northland, eighthwonder, saga | depends on research question 2 |
| `dplay`     | null at first, later UDP       | c1 (DirectPlay 4 under `DPlayDLL.dll`) | no service providers until M12 |
| `dpnet`     | null at first, later UDP       | c2 engine (DirectPlay 8) | same |
| `wsock32`, `ws2_32` | host sockets           | c2 engine | by ordinal |
| `comctl32`, `comdlg32`, `winspool` | platform    | editors only | M13 |

Lifted instead of reimplemented: `binkw32.dll` (Bink 1 video), `DPlayDLL.dll` and `gedx8musicdrv.dll`.
These are game-side code with a narrow API below them, so lifting them is less work and more faithful than
reimplementing them.

## Platform layer (platform/)

This is a thin C API over SDL3: window, framebuffer upload (16-bit → texture, with integer or aspect
scaling), input events, audio device and clock. It also has a `null` backend for tests and CI. Nothing above
`platform/` includes SDL headers.

## Games (games/<id>/)

| File          | Purpose |
|---------------|---------|
| `game.toml`   | Manifest: variants (installers, media), pinned binaries with role, treatment and (for colliding DLLs) `base`; per binary a `[binary.discover]` table with seeds, no-return functions, data ranges and jump tables; later patches. |
| `CMakeLists.txt` | Calls `rc_add_game(<id> TITLE … EXES …)` from `cmake/lift.cmake`. That generates the shell's `main()` from `cmake/game_main.c.in`, and the shell calls `rc_game_main()` in `runtime/src/game.c` (arguments, locating the install, later hash check and boot). The executable is `cultures-<id>`. |
| `patches.c` *(later)* | Hooks at guest addresses, such as skipping the CD check, larger resolutions or fixes. Every patch is opt-in or documented. |

## Generated code (generated/, git-ignored)

The layout is `generated/<game>/<binary>-<variant>/` (for example `generated/cultures1/cultures.exe-gog-en/`):
`codemap.json` from discovery, and from `recomp lift` `code_XXXX.c`, `funcs.h`, `table.c` (`<symbol>_table`) and
`binary.c` (the `rc_generated_binary` record: name, variant, hash, base, entry, table). `recomp lift` also rewrites
`generated/<game>/set.c`, the `rc_generated_set` of every binary lifted for the game so far. All binaries of a game
link into one executable. Each gets its own symbol prefix (from its directory name, e.g. `cultures_exe_gog_en_`),
and the shell picks the ones whose SHA-256 matches the user's copy. `cmake/lift.cmake` compiles everything below
`generated/<game>/` with the project's full warning set and defines `RC_HAVE_GENERATED`. It is
derived from copyrighted code, so it is **never committed**, never uploaded and never put into issues. Builds
regenerate it locally from the user's own files.

## Repository layout

```
.github/workflows/   CI: audit, Rust (fmt/clippy/test), C (Linux/Windows/macOS)
cmake/               lift.cmake: rc_add_game(), linking generated code when present; game_main.c.in
docs/                PLAN, ARCHITECTURE, TARGETS, RESEARCH, WORKFLOWS, notes/
games/               one directory per game: game.toml, CMakeLists.txt (patches.c later)
hle/                 Win32/DirectX/middleware reimplementations, one subdirectory per DLL
platform/            SDL3 backend + null backend
recomp/              Rust workspace: the recompiler and the `recomp` CLI
runtime/             guest memory, CPU state, dispatcher, HLE registry, logging, tests
tools/               audit.sh, git hooks, dev helpers
orig/                YOUR game files (git-ignored)
generated/           lifted C (git-ignored)
build/               CMake build trees (git-ignored)
```
