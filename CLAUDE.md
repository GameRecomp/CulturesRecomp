# CulturesRecomp – orientation for a new working session

Static recompilation of the *Cultures* games (Funatics) into portable C on an SDL3 runtime, modelled on
TropicoRecomp. Start here, then read `docs/PLAN.md` (milestones, checklists, decision log).

## State (2026-11-19)

| Milestone | State | What exists |
|-----------|-------|-------------|
| M0 Foundation | done | CMake + presets, C runtime core, SDL3/null platform, HLE module table, game shells, audit + hooks, CI |
| M1 Read the inputs | done | `rc-media` (Inno 5.5.0u, own LZMA1/LZMA2, ISO 9660, InstallShield 5, zip), `recomp list/extract`, runtime PE loader, `--check` |
| M2 Find the code (C1) | **in progress** | `rc-discover` crate with `Image` (sections at VAs, IAT map, exports) and `iced-x86`; the discovery passes are the next step |

Last commits: `e15564c` (M2 start), `003c031` (M1), `50881ba` (M0).

## Next step: M2

See `docs/PLAN.md`, section "M2 – Find the code (C1)" for the checklist and exit criterion (≥ 99 % of `.text` of
the three `Cultures.exe` builds classified as code, data or padding; jump tables and vtables resolved).
Facts that matter:

- No executable has relocations; all are MSVC 6.0 builds with image base `0x400000` (`docs/RESEARCH.md`).
- Imports are called as `call [IAT]`; `Image::imports` maps IAT slot VAs to `dll!name`.
- The DLLs to lift (`binkw32.dll`, `DPlayDLL.dll`, later `gedx8musicdrv.dll`) have relocations, and C1's
  `binkw32.dll` and `DPlayDLL.dll` both prefer `0x10000000` → rebasing is planned for M4 (decision log).
- Output belongs in `generated/<game>/<binary>/` (git-ignored). Function addresses may appear in manifests and
  docs; disassembly or decompiled code never goes into the repo, issues or commits.
- Test material: `recomp extract` the C1 media into `orig/<game>/<variant>/` (`docs/WORKFLOWS.md`).

## Layout

```
recomp/crates/rc-pe        PE32 parser (sections, imports, exports, relocs, Rich header)
recomp/crates/rc-manifest  games/*/game.toml loader, validation, hash catalogue
recomp/crates/rc-media     installers / ISO / InstallShield / zip readers, LZMA decoder
recomp/crates/rc-discover  code discovery (M2) – image model so far
recomp/crates/rc-cli       `recomp`: identify, info, list, extract (+ tests/orig_media.rs, ignored)
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
cargo test -p rc-cli -- --ignored          # against the media in orig/
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
