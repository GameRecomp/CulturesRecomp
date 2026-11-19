# CulturesRecomp – orientation for a new working session

Static recompilation of the *Cultures* games (Funatics) into portable C on an SDL3 runtime, modelled on
TropicoRecomp. Start here, then read `docs/PLAN.md` (milestones, checklists, decision log).

## State (2026-11-19)

| Milestone | State | What exists |
|-----------|-------|-------------|
| M0 Foundation | done | CMake + presets, C runtime core, SDL3/null platform, HLE module table, game shells, audit + hooks, CI |
| M1 Read the inputs | done | `rc-media` (Inno 5.5.0u, own LZMA1/LZMA2, ISO 9660, InstallShield 5, zip), `recomp list/extract`, runtime PE loader, `--check` |
| M2 Find the code (C1) | done | `rc-discover` (descent, jump tables, SEH/EH, vtables, pointer scans with trial decoding, gaps, no-return fixed point, rebasing, reloc check), `recomp discover/codediff`, per-binary manifest hints (`seeds`, `no_return`, `returns`, `data`, `jump_tables`); 100 % of every C1 binary, no leftover |
| M3 Lifter, integer core | **next** | — |

M2 is in `fe709eb` and the commit after it (leftovers settled). Check `git log` for later work.

## Next step: M3

See `docs/PLAN.md`, section "M3 – Lifter, integer core". The lifter's input is the code map:
`recomp discover <binary>` writes `generated/<game>/<binary>-<variant>/codemap.json` (`rc_discover::CodeMap`,
serde; addresses as `"0x…"` strings): functions with their byte ranges (`chunks`) and `no_return`, `labels`
(address-taken points inside functions → dispatch entries), `jump_tables` with targets, `vtables`, data/padding/dead
ranges and `imports_called`. Facts that matter (details in `docs/RESEARCH.md`, "Code discovery"):

- No executable has relocations; all are MSVC 6.0 builds with image base `0x400000`.
- Imports are called as `call [IAT]` (also `mov esi, [IAT]; call esi`); `Image::imports` maps IAT slots to `dll!name`.
- Lifted DLLs are rebased by the recompiler to the manifest `base` (C1 `DPlayDLL.dll` → `0x11000000`); the runtime
  loader still has to do the same (M4).
- C1 has no RTTI, almost no C++ EH, some SEH in the CRT, and **MMX** (game and Bink) besides x87; no SSE.
- Tail calls and jumps into other functions exist; function bodies may overlap (a body stops at other entries).
- Output belongs in `generated/<game>/…` (git-ignored). Function addresses may appear in manifests and docs;
  disassembly or decompiled code never goes into the repo, issues or commits.
- Test material: `recomp extract` the C1 media into `orig/<game>/<variant>/` (`docs/WORKFLOWS.md`).

## Layout

```
recomp/crates/rc-pe        PE32 parser (sections, imports, exports, relocs, Rich header)
recomp/crates/rc-manifest  games/*/game.toml loader, validation, hash catalogue
recomp/crates/rc-media     installers / ISO / InstallShield / zip readers, LZMA decoder
recomp/crates/rc-discover  code discovery (M2): image/rebase, engine, code map + report, build diff, tests
recomp/crates/rc-cli       `recomp`: identify, info, list, extract, discover, codediff (+ ignored tests on orig/)
runtime/                   guest memory, CPU, dispatcher, HLE registry, VFS, PE loader, rc_game_main, tests
hle/                       module table, kernel32 (GetTickCount), winmm (timeGetTime), tests
platform/                  SDL3 and null backends, tests
games/<id>/                game.toml + CMakeLists.txt (rc_add_game → cultures-<id>)
cmake/                     lift.cmake, game_main.c.in
tools/                     audit.sh, hooks/ (pre-commit, commit-msg), dev-extract.sh
docs/                      PLAN, ARCHITECTURE, TARGETS, RESEARCH, WORKFLOWS
orig/                      the user's game media (git-ignored): 10 GOG installers, Gold ISO, Saga zip
```

## Commands

```sh
cd recomp && cargo fmt --all && cargo clippy --all-targets -- -D warnings && cargo test
cargo test --release -p rc-cli -- --ignored   # against the media and extracted binaries in orig/
recomp/target/release/recomp discover -v orig/cultures1/gog-en/Cultures.exe
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
cmake --preset ci  && cmake --build --preset ci  && ctest --preset ci     # warnings as errors, no SDL3
tools/audit.sh
```

## Conventions

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
