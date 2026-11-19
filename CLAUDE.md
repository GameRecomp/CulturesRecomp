# CulturesRecomp – orientation for a new working session

Static recompilation of the *Cultures* games (Funatics) into portable C on an SDL3 runtime, modelled on
TropicoRecomp. Start here, then read `docs/PLAN.md` (milestones, checklists, decision log).

## State (2026-11-19)

| Milestone | State | What exists |
|-----------|-------|-------------|
| M0 Foundation | done | CMake + presets, C runtime core, SDL3/null platform, HLE module table, game shells, audit + hooks, CI |
| M1 Read the inputs | done | `rc-media` (Inno 5.5.0u, own LZMA1/LZMA2, ISO 9660, InstallShield 5, zip), `recomp list/extract`, runtime PE loader, `--check` |
| M2 Find the code (C1) | done | `rc-discover` (descent, jump tables, SEH/EH, vtables, pointer scans with trial decoding, gaps, no-return fixed point, rebasing, reloc check), `recomp discover/codediff`, per-binary manifest hints (`seeds`, `no_return`, `returns`, `data`, `jump_tables`); 100 % of every C1 binary, no leftover |
| M3 Lifter, integer core | done | TropicoRecomp's `rc-ir`, `rc-x86`, `rc-emit-c`, `rc-difftest` adapted; `recomp lift`; runtime on Tropico's `rc_cpu` layout, `lifted.h`, `x87`; all C1 binaries lift, compile warning-free (gcc, clang) and link; difftest 18720/18720 |
| M4 x87 and the whole game | **in progress** | x87 done; open: MMX (probably dead, `cpuid` says none), DLL rebasing in the runtime loader |
| M5 Boot to the main loop | next | — |

Check `git log` for the latest work.

## Next steps: M4, then M5

See `docs/PLAN.md` ("M4", "M5"). The pipeline is `recomp lift <binary>` → `generated/<game>/<binary>-<variant>/`
(`code_XXXX.c`, `funcs.h`, `table.c`, `binary.c`, `codemap.json`) + `generated/<game>/set.c` → `cmake/lift.cmake`
compiles all of it into `cultures-<game>`. `rc_game_main` (`runtime/src/game.c`) still stops before running lifted
code: M5 maps the PE, registers each `rc_generated_binary`'s table (`rc_dispatch_add_table`), sets up TEB/stack and
calls the entry. Facts that matter (details in `docs/RESEARCH.md`, "Code discovery" and "Lifting"):

- No executable has relocations; all are MSVC 6.0 builds with image base `0x400000`.
- Imports are called as `call [IAT]` (also `mov esi, [IAT]; call esi`); the IAT holds HLE thunk addresses
  (`0xF0000000+`), reached through `rc_dispatch`.
- Lifted DLLs are rebased by the recompiler to the manifest `base` (C1 `DPlayDLL.dll` → `0x11000000`); the runtime
  loader still has to do the same (M4).
- Lifted code traps only on MMX (probably dead: `cpuid` reports no MMX and both the game and Bink check it),
  `int 6` (CRT FDIV stubs) and `int3`. A trap or an unknown dispatch target is fatal and names the address.
- The lifter, emitter, `rc/lifted.h`, `rc/x87.h` and the difftest are shared with TropicoRecomp: keep changes general.
- Output belongs in `generated/<game>/…` (git-ignored). Function addresses may appear in manifests and docs;
  disassembly, lifted or decompiled code never goes into the repo, issues or commits.
- Test material: `recomp extract` the C1 media into `orig/<game>/<variant>/` (`docs/WORKFLOWS.md`).

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
runtime/                   guest memory, CPU, x87, dispatcher + lifted tables, traps, HLE registry, VFS, PE loader,
                           rc_game_main, tests; rc/lifted.h is what generated code includes
hle/                       module table, kernel32 (GetTickCount), winmm (timeGetTime), tests
platform/                  SDL3 and null backends, tests
games/<id>/                game.toml + CMakeLists.txt (rc_add_game → cultures-<id>)
cmake/                     lift.cmake, game_main.c.in
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
