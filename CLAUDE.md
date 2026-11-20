# CulturesRecomp – orientation for a new working session

Static recompilation of the *Cultures* games (Funatics) into portable C on an SDL3 runtime, modelled on
TropicoRecomp. Start here, then read `docs/PLAN.md` (milestones, checklists, decision log).

## State (2026-11-20)

| Milestone | State | What exists |
|-----------|-------|-------------|
| M0 Foundation | done | CMake + presets, C runtime core, SDL3/null platform, HLE module table, game shells, audit + hooks, CI |
| M1 Read the inputs | done | `rc-media` (Inno 5.5.0u, own LZMA1/LZMA2, ISO 9660, InstallShield 5, zip), `recomp list/extract`, runtime PE loader, `--check` |
| M2 Find the code (C1) | done | `rc-discover` (descent, jump tables, SEH/EH, vtables, pointer scans with trial decoding, gaps, no-return fixed point, rebasing, reloc check), `recomp discover/codediff`, per-binary manifest hints (`seeds`, `no_return`, `returns`, `data`, `jump_tables`); 100 % of every C1 binary, no leftover |
| M3 Lifter, integer core | done | TropicoRecomp's `rc-ir`, `rc-x86`, `rc-emit-c`, `rc-difftest` adapted; `recomp lift`; runtime on Tropico's `rc_cpu` layout, `lifted.h`, `x87`; all C1 binaries lift, compile warning-free (gcc, clang) and link; difftest 18720/18720 |
| M4 x87 and the whole game | done | x87 and MMX (both difftested); DLL rebasing in the loader |
| M5 Boot to the main loop | done | runtime/hle/platform from TropicoRecomp + rc_shell (`runtime/src/game.c`); new HLE: advapi32, dinput, dsound (no device), dmusic (DX7 check), shell32, MCI CD audio |
| M6 Picture and input | done | intro, menus and a scenario (1024x768x8) render; mouse and keyboard; all three C1 builds start |
| M7 Sound, music and video | **in progress** | Miles samples and Bink sound work; CD music via MCI → MUSIC/*.ogg (needs SDL3_mixer with Vorbis); DirectSound reports no device |
| M8 In game | **in progress** | an hour of scripted play in a scenario (216,000 frames) without trap; save and load work; open: a whole mission played by hand, timing |
| M11 Northland, 8th Wonder, Saga | **in progress** | all lift, play their movies (dshow + pl_mpeg), sound (DirectSound 8) and music (dmusic player + DLS synth), and start their first missions; open: a mission played through |
| M12 Multiplayer | **in progress** | own network (`hle/dplay/net.c`: UDP discovery, host relay over TCP) under DirectPlay 4 (C1) and 8 (Northland): two programs find each other, join and play a match; open: the other C2-engine games, an hour in lockstep |
| M13 Editors | **in progress** | a Windows desktop for programs without DirectDraw (`user32` windows, menus, dialogs, controls; `gdi32`; `comctl32`; `comdlg32`); the editors of Northland, 8th Wonder and the Saga create, paint, save and open maps, and Northland plays a map made in its editor; open: the other tools by hand |
| M14 Converter | **in progress** | `recomp convert`: installer, CD, zip or folder → unpacked, lifted, built, `out/<game>/play.sh`; open: no-compiler packages, browser |
| M10 Cultures 2 | **in progress** | boots to its serial prompt; needs the player's serial once; MMX and music done |

Check `git log` for the latest work.

## Next steps: M7/M8 (Cultures 1: a whole mission played by hand, then M9 release) and M10 (Cultures 2: past the serial prompt, music)

See `docs/PLAN.md`. Run the game (after `recomp lift` of the C1 binaries and a dev build with SDL3):

```sh
CMAKE_PREFIX_PATH=<your SDL3 prefix> cmake --preset dev && cmake --build --preset dev
build/dev/games/cultures1/cultures-cultures1 --game-dir orig/cultures1/gog-en
RC_TRACE=imports …                     # every API call;  --check: imports the HLE layer lacks
SDL_VIDEO_DRIVER=dummy RC_CLOCK=virtual RC_INPUT=script.txt RC_SCREENSHOT=300:a.bmp RC_FRAMES=310 …   # scripted
```

Facts that matter (details in `docs/RESEARCH.md`, "Code discovery", "Lifting", "Start-up"):

- `runtime/`, `hle/`, `platform/`, the lifter, emitter and difftest are shared with TropicoRecomp: keep changes
  general, so they can go back; Cultures-only behaviour goes into new modules or the manifest.
- No executable has relocations (image base `0x400000`); lifted DLLs are rebased to the manifest `base` by the
  recompiler and by the loader (C1 `DPlayDLL.dll` → `0x11000000`).
- Imports bind to the lifted DLLs' exports or to reserved addresses that `rc_dispatch` sends to HLE functions; an
  unimplemented one stops with its name. COM vtables hold reserved addresses named after the methods.
- Lifted code traps only on `int 6` (CRT FDIV stubs) and `int3`; MMX is lifted (`c->mm`, `rc/mmx.h`). A trap or an unknown dispatch target is fatal and
  names the address.
- Output belongs in `generated/<game>/…` (git-ignored). Function addresses may appear in manifests and docs;
  disassembly, lifted or decompiled code never goes into the repo, issues or commits.

## Layout

```
recomp/crates/rc-pe        PE32 parser (sections, imports, exports, relocs, Rich header)
recomp/crates/rc-manifest  games/*/game.toml loader, validation, hash catalogue
recomp/crates/rc-media     installers / ISO / InstallShield / zip readers, LZMA decoder
recomp/crates/rc-discover  code discovery (M2): image/rebase, engine, code map + report, build diff, tests
recomp/crates/rc-ir        the lifter's IR (shared with TropicoRecomp)
recomp/crates/rc-x86       x86 semantics: instructions → IR, functions and label entries from the code map
recomp/crates/rc-emit-c    IR → C: functions, funcs.h, code_XXXX.c, per-binary table
recomp/crates/rc-difftest  `difftest`: lifted C against the real CPU in 32-bit mode (dev tool, runs in CI)
recomp/crates/rc-cli       `recomp`: identify, info, list, extract, discover, codediff, lift (+ ignored tests on orig/)
runtime/                   TropicoRecomp's runtime: memory/vm/heap, CPU, x87, dispatch (+ one table per lifted binary),
                           imports, process (loader + DLL relocation), threads, clock, host files; rc_shell = game.c
hle/                       TropicoRecomp's kernel32/user32/gdi32/ddraw/mss32/winmm + advapi32, dinput, dsound, dmusic, dplay,
                           shell32, comctl32, comdlg32, winmm/mci.c; user32 also runs a desktop for the editors; frames.c = scripted input and screenshots
platform/                  SDL3 (+ttf, mixer) and null backends
games/<id>/                game.toml + CMakeLists.txt (rc_add_game → cultures-<id>)
cmake/                     lift.cmake, game_main.c.in
third_party/               vendored code under its own licence (pl_mpeg), see its README
tools/                     audit.sh, hooks/ (pre-commit, commit-msg), dev-extract.sh, difftest/ (ref32.c, harness.c)
docs/                      PLAN, ARCHITECTURE, TARGETS, RESEARCH, WORKFLOWS
orig/                      the user's game media (git-ignored): 10 GOG installers, Gold ISO, Saga zip
```

## Commands

```sh
cd recomp && cargo fmt --all && cargo clippy --all-targets -- -D warnings && cargo test
cargo test --release -p rc-cli -- --ignored   # against the media and extracted binaries in orig/
recomp/target/release/recomp discover -v orig/cultures1/gog-en/Cultures.exe
recomp/target/release/recomp lift orig/cultures1/gog-en/Cultures.exe   # then build: links the lifted code
cargo run --release --manifest-path recomp/Cargo.toml -p rc-difftest  # lifted C vs the real CPU
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
cmake --preset ci  && cmake --build --preset ci  && ctest --preset ci     # warnings as errors, no SDL3
tools/audit.sh
```

## Conventions

- **Share code between the games as much as possible.** One recompiler, one runtime, one HLE layer and one
  platform layer serve all five games and both engines. Per-game or per-build differences belong in the manifest
  (`games/<id>/game.toml`: hashes, bases, discovery hints, later patches) or in a small, clearly named game module,
  never in a copy of shared code. Before writing something game-specific, check whether a general rule or an
  existing module covers it, and prefer generalising that. Discovery and lifting rules must hold for every target,
  not just the one at hand (check the other binaries in `orig/` when changing them).
- Docs, code and comments in English (the maintainer writes in German).
- Commits: author **and** committer `TerminShock <357108427+TerminShock@users.noreply.github.com>` (as in the
  initial commit), e.g. `GIT_COMMITTER_NAME=… GIT_COMMITTER_EMAIL=… git commit --author="TerminShock <…>"`.
  Commit messages describe the change only: no AI tools, models, sessions or attribution trailers
  (`tools/hooks/commit-msg` enforces it). Commit only when asked; nothing is pushed so far.
- Hooks are enabled with `git config core.hooksPath tools/hooks`.
- Every source file starts with `SPDX-License-Identifier: MIT`; no absolute home paths; no game files
  (`tools/audit.sh`). See `CONTRIBUTING.md`.
- New builds or media: pin them by hash in `games/<id>/game.toml` and `docs/TARGETS.md` first.
- Keep the docs current: tick checklist items in `docs/PLAN.md`, add findings to `docs/RESEARCH.md`
  (mark unknowns **(open)**), add decisions to the decision log.
- Only Linux has been built and tested locally; Windows/macOS run only in CI (not yet run, nothing pushed).
