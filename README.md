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

**M0, Foundation: in progress.** Docs, target manifests and the Rust toolchain (`rc-pe`, `rc-manifest`, `recomp`) are in place. The C side (CMake, runtime, HLE, platform), CI and the audit hook are still to do.
The `recomp` CLI already identifies every supported installer, ISO and executable.
Nothing is playable yet. See [`docs/PLAN.md`](docs/PLAN.md) for the full roadmap.

## Quick start (developers)

Requirements: CMake ≥ 3.25, a C11 compiler, Rust (stable), and optionally SDL3.

```sh
# Recompiler toolchain (Rust)
cargo build --release --manifest-path recomp/Cargo.toml
cargo test --manifest-path recomp/Cargo.toml

# Identify your game files (installers, ISO, or an installed folder)
recomp/target/release/recomp identify orig/

# Runtime, HLE and game shells (C)
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
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
| [`CONTRIBUTING.md`](CONTRIBUTING.md)           | Rules, especially regarding copyrighted material        |

## License

[MIT](LICENSE) applies to the code in this repository. The games themselves are the property of their
rights holders. Buy them (for example on GOG) to use this project.
