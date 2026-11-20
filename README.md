# CulturesRecomp

A **static recompilation** of the *Cultures* series by Funatics. It translates
the original 32-bit x86 Windows executables ahead of time into portable C. That C
runs on a small runtime that reimplements the Win32, DirectX and middleware APIs
the games call, on top of SDL3.

The result has no emulator, no interpreter and no Wine: it is a native program
built from the game's own code. The game still needs its original data files,
which you supply from your own copy.

> Unofficial fan project. Not affiliated with Funatics, THQ Nordic, JoWooD or GOG.
> This repository contains **no game code and no game data**, and it never will.

## Supported games

| Id             | Game (EN / DE)                                                | Engine | Sources supported                         |
|----------------|---------------------------------------------------------------|--------|-------------------------------------------|
| `cultures1`    | Cultures: Discovery of Vinland / Die Entdeckung Vinlands      | C1     | GOG (EN, DE), German *Gold Edition* CD (incl. add-on *Die Rache des Regengottes*) |
| `cultures2`    | Cultures 2: The Gates of Asgard / Die Tore Asgards            | C2     | GOG (EN, DE)                              |
| `northland`    | Cultures: Northland / Reise nach Nordland                     | C2     | GOG (EN, DE, ES)                          |
| `eighthwonder` | Cultures: 8th Wonder of the World / Das Achte Weltwunder      | C2     | GOG (EN, DE, ES)                          |
| `saga`         | Cultures: The Saga / Die Saga                                 | C2     | German retail CD (installed copy as zip)  |

There are two engines. *C1* is the engine of the first game. *C2* is the Cultures 2 engine,
which Northland, 8th Wonder and the Saga reuse. Every executable we support is pinned by
SHA-256 in [`docs/TARGETS.md`](docs/TARGETS.md) and `games/<id>/game.toml`. Any other
build is refused.

## Editions at a glance

| Edition | Year | What it contains | Exclusive content | Here |
|---------|------|------------------|-------------------|------|
| Cultures – Die Entdeckung Vinlands (retail CD) | 2000 | base game | — | via GOG / Gold |
| Die Rache des Regengottes (add-on) | 2001 | needs the base game | new campaign (7 missions, branching), 16 multiplayer maps, mercenaries from allied tribes | via Gold |
| **Cultures Gold** (DE) | 2002 | base + add-on | a few extra maps (a shop listing says 4 single-player and 3 multiplayer, including a Christmas map) | ✅ `cultures1/gold-de` |
| **Cultures** (GOG) | — | base game only (no add-on) | music as OGG instead of CD audio | ✅ `cultures1/gog-en`, `gog-de` |
| **Cultures 2 – Die Tore Asgards** (retail, GOG) | 2002 | base game | — (no add-on was released) | ✅ `cultures2/gog-*` |
| **Reise nach Nordland / Northland** (retail, GOG, Steam) | 2002 | game + map editor | — | ✅ `northland/gog-*` |
| **Das Achte Weltwunder / 8th Wonder** (retail, GOG, Steam) | 2003 | game + map editor | Egyptians, earthquakes, revised editor | ✅ `eighthwonder/gog-*` |
| Die Abenteuerbox | 2003 | Northland + 8th Wonder | — | covered by its parts |
| **Cultures – Die Saga** (DE) | 2003 | Northland + 8th Wonder campaigns in one menu, editor; Vinland on a bonus CD | **Drachenland** campaign (5 missions), 28 community missions in two campaigns (22 + 6); a 2005 build `GameSaga.exe`; multiplayer only with other Saga copies | ✅ `saga/retail-de` |
| Steam / GOG bundle "Northland + 8th Wonder" | 2013/2015 | both games | — | GOG ✅, Steam builds not yet hashed |
| Mac/Linux ports (Runesoft), iOS Vinland (2014), Cultures Online (2010, Flash) | — | ports / other games | — | out of scope |

Sources and details are in [`docs/RESEARCH.md`](docs/RESEARCH.md#editions).

## Status

**M0, Foundation: done.** The docs, target manifests, Rust toolchain (`rc-pe`, `rc-manifest`, `recomp`),
C runtime core (guest memory, dispatcher, HLE registry, VFS lookup), the SDL3/null platform layer, game shells,
audit hooks and CI are in place. The `recomp` CLI already identifies every supported installer, ISO, zip and executable.
**M1, Read the inputs: done.** `recomp extract` unpacks every supported installer, the Gold ISO and the Saga zip
without third-party tools (verified byte-identical against innoextract/unshield/unzip), and the runtime's PE loader
maps every target executable.
**M2, Find the code: done.** `recomp discover` separates code from data in the executables (which have no
relocations) and DLLs: every byte of every Cultures 1 binary is classified, jump tables and vtables are resolved, and
the code map for the lifter goes to `generated/`. `recomp codediff` compares builds.
**M3, Lifter (integer core): done.** `recomp lift` turns every Cultures 1 binary into C (shared lifter with
TropicoRecomp; every instruction form checked against the real CPU). The C compiles without warnings and links into
the game shell.
**M4/M5: done.** The runtime, HLE and platform layers come from TropicoRecomp, with the modules Cultures adds.

What runs now (Linux; shown in scripted runs, not yet played through by hand):

| Game | State |
|------|-------|
| Cultures 1 (GOG EN/DE, Gold DE) | intro, menus, campaigns and scenarios; sound, music, save and load; LAN games between two programs; an hour of scripted play without a fault |
| Cultures 2 (GOG EN/DE) | menus, music, the campaign map; the first mission starts and runs |
| Northland, 8th Wonder, Saga | movies, sound, music; first campaign missions start; LAN games in all of them; the map editors make, save and open maps, and a map made in Northland's editor plays in the game |

`recomp convert` turns any of the supported installers, the Gold ISO or the Saga zip into a program in one step.
Open: a mission played through by hand, game speed, Windows and macOS builds, packages that need no compiler.
See [`docs/PLAN.md`](docs/PLAN.md) for the roadmap and [`docs/RELEASE-NOTES.md`](docs/RELEASE-NOTES.md) for the known
issues.

## Quick start (developers)

Requirements: CMake ≥ 3.25, a C11 compiler, Rust (stable), and optionally SDL3.

```sh
# Recompiler toolchain (Rust)
cargo build --release --manifest-path recomp/Cargo.toml
cargo test --manifest-path recomp/Cargo.toml

# Identify your game files (installers, ISO, or an installed folder)
recomp/target/release/recomp identify orig/

# Unpack one (→ orig/<game>/<variant>/, binaries verified)
recomp/target/release/recomp extract orig/setup_cultures_2.1.0.12.exe

# Or everything in one step: unpack, lift, build → out/cultures1/play.sh
recomp/target/release/recomp convert orig/setup_cultures_2.1.0.12.exe

# Lift it (→ generated/cultures1/…, never committed)
recomp/target/release/recomp lift orig/cultures1/gog-en/Cultures.exe

# Runtime, HLE and game shells (C)
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# The game (needs SDL3, SDL3_ttf and SDL3_mixer for a window and sound)
build/dev/games/cultures1/cultures-cultures1 --game-dir orig/cultures1/gog-en
build/dev/games/cultures1/cultures-cultures1 --check --game-dir orig/cultures1/gog-en
```

Put your own game files in `orig/` (git-ignored). Read [`docs/WORKFLOWS.md`](docs/WORKFLOWS.md) for the details.

## Documentation

| File                                           | Contents                                                |
|------------------------------------------------|---------------------------------------------------------|
| [`docs/PLAN.md`](docs/PLAN.md)                 | Goals, milestones, order of work, risks, decision log   |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Pipeline, runtime model, HLE layer, repository layout   |
| [`docs/TARGETS.md`](docs/TARGETS.md)           | Every supported installer and binary, with hashes       |
| [`docs/RESEARCH.md`](docs/RESEARCH.md)         | Series history and findings from analysing the binaries |
| [`docs/WORKFLOWS.md`](docs/WORKFLOWS.md)       | Day-to-day commands: extract, identify, lift, build, debug |
| [`docs/RELEASE-NOTES.md`](docs/RELEASE-NOTES.md) | What works, what does not yet (draft for the first release) |
| [`CONTRIBUTING.md`](CONTRIBUTING.md)           | Rules, especially regarding copyrighted material        |
| [`CLAUDE.md`](CLAUDE.md)                       | Short orientation for a new working session: state, conventions, next steps |

## License

[MIT](LICENSE) applies to the code in this repository. The games themselves are the property of their
rights holders. Buy them (for example on GOG) to use this project.
