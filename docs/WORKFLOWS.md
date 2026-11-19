# Workflows

## Supplying the game files

Put your own copies into `orig/` (git-ignored). Either form works:

- the installers / ISO / zip as they are (`orig/setup_cultures_2.1.0.12.exe`, `orig/Cultures (Gold Edition).iso`,
  `orig/Cultures Saga.zip`, …)
- extracted installs in `orig/<game>/<variant>/` (e.g. `orig/cultures1/gog-en/Cultures.exe`)

Check what you have:

```sh
cargo run --release --manifest-path recomp/Cargo.toml -- identify orig/
```

Every recognised file is printed with its game, variant and role. Unknown executables are reported as
`unknown`. If you have a build that is not in [TARGETS.md](TARGETS.md), open an issue with its **SHA-256 and
size only**. Do not attach the file.

## Extracting

`recomp extract` reads the installers, the Gold ISO and the Saga zip itself (no external tools). It recognises the
medium by its hash, unpacks it to `orig/<game>/<variant>/` and verifies every pinned binary:

```sh
recomp=recomp/target/release/recomp
$recomp extract orig/setup_cultures_2.1.0.12.exe           # → orig/cultures1/gog-en/
$recomp extract "orig/Cultures (Gold Edition).iso"         # → orig/cultures1/gold-de/
$recomp extract "orig/Cultures Saga.zip"                   # → orig/saga/retail-de/
$recomp extract --dest /some/dir orig/setup_northland_2.1.0.9.exe
$recomp list orig/setup_cultures2_2.1.0.10.exe              # just list the files
```

An unknown medium needs `--dest`, and a non-empty destination needs `--force`. A GOG installer takes 10–20 s.

`tools/dev-extract.sh` (innoextract, unshield, unzip) stays as an independent reference for comparisons.

## Test material for code discovery (M2)

Discovery and lifting work on the extracted binaries. For Cultures 1:

```sh
$recomp extract orig/setup_cultures_2.1.0.12.exe           # gog-en: Cultures.exe, binkw32.dll, DPlayDLL.dll
$recomp extract orig/setup_cultures_german_2.1.0.12.exe    # gog-de
$recomp extract "orig/Cultures (Gold Edition).iso"         # gold-de (add-on build)
$recomp info --imports orig/cultures1/gog-en/Cultures.exe
```

Temporary inspection files belong in `build/` (git-ignored), never next to the sources.

## Finding the code

```sh
$recomp discover orig/cultures1/gog-en/Cultures.exe     # report + generated/cultures1/cultures.exe-gog-en/codemap.json
$recomp discover orig/cultures1/*/Cultures.exe          # several at once
$recomp discover -v --no-write orig/cultures1/gog-en/DPlayDLL.dll   # list unknown ranges, warnings, rejects
$recomp discover --no-relocs orig/cultures1/gog-en/binkw32.dll      # check the heuristics against the relocations
$recomp codediff orig/cultures1/gog-en/Cultures.exe orig/cultures1/gold-de/Cultures.exe
cargo test --release -p rc-cli -- --ignored                         # exit criterion: no unknown byte, no candidate left
```

The manifest supplies the per-build hints and, for `DPlayDLL.dll`, the rebased load address (`--base` overrides it).
`--out DIR` writes the code map elsewhere. `--verbose` adds what needs attention: `unknown` ranges, `warning`s
(conflicts on trusted paths), `rejected` candidates with the reason, unresolved indirect jumps, and `no-return?`
candidates. A candidate becomes a `no_return` hint in `[binary.discover]` only after checking the callee by hand; a
callee that does return (MSVC's x87 helpers) becomes a `returns` hint, so the candidate is no longer reported.
Ranges that analysis cannot classify become `data` or `seeds` hints. The code map and the report contain addresses and
counts only; neither the report nor anything derived from the code (disassembly, code bytes) goes into the repository,
issues or commits.

## Lifting

```sh
$recomp lift orig/cultures1/gog-en/Cultures.exe          # C in generated/cultures1/cultures.exe-gog-en/, set.c
$recomp lift orig/cultures1/*/Cultures.exe orig/cultures1/gog-en/*.dll   # several at once
$recomp lift --comments --out build/look orig/cultures1/gog-en/Cultures.exe   # each instruction above its C
cmake --preset ci && cmake --build --preset ci            # compiles everything below generated/<game>/
```

`recomp lift` runs discovery with the manifest hints, lifts every function and writes the C next to the code map.
It prints the trapping instructions by kind (`--verbose` for all). Every lifted binary of a game is listed in
`generated/<game>/set.c` and linked into `cultures-<game>`. Delete a binary's directory and lift another one to
drop it. Like the code map, the C is derived from the game and stays local (`--out` into `build/` for a look).

## Differential tests

```sh
cargo run --release --manifest-path recomp/Cargo.toml -p rc-difftest                # all forms, 12 states each
recomp/target/release/difftest --only "shld" --states 200                           # one kind, more states
recomp/target/release/difftest --states 1 --coverage orig/cultures1/gog-en/Cultures.exe   # every form tested?
```

The difftest needs clang (`-m32 -nostdlib`, no 32-bit libc) and an x86-64 Linux host, and writes to
`build/difftest/`. CI runs it on every push.

## Converting in one step

```sh
CMAKE_PREFIX_PATH=<your SDL3 prefix> recomp/target/release/recomp convert orig/setup_northland_2.1.0.9.exe
recomp/target/release/recomp convert orig/cultures1/gog-en          # an installed (or already unpacked) folder
out/northland/play.sh                                               # starts it with the unpacked game files
```

`convert` identifies the medium by hash (or an installed folder by its executable), unpacks it into
`orig/<game>/<variant>/` unless that is already there (`--force` unpacks again), lifts every binary the manifest
lifts, builds the game's program with the CMake preset `release` (`--preset dev` for a debug build) and puts it
with a launcher into `out/<game>/`. Both stay on your machine: they contain code derived from your copy.

## Running a game

```sh
recomp/target/release/recomp lift orig/cultures1/gog-en/*.exe orig/cultures1/gog-en/binkw32.dll orig/cultures1/gog-en/DPlayDLL.dll
CMAKE_PREFIX_PATH=<your SDL3 prefix> cmake --preset dev && cmake --build --preset dev
build/dev/games/cultures1/cultures-cultures1 --game-dir orig/cultures1/gog-en      # or set RC_GAME_DIR
```

The shell hashes `Cultures.exe` and the DLLs beside it and runs the lifted builds that match; a build that was not
lifted is refused with its hash. Saves, settings and the registry (`registry.txt`) go to the per-user data directory
(`--user-dir` to choose). Without SDL3 (and SDL3_ttf) the null platform is built, which runs without a window.
Music needs an SDL3_mixer with Ogg Vorbis (its default `stb_vorbis` decoder): GOG's CD tracks are `MUSIC/TrackNN.ogg`.
`RC_TRACE=sound` logs what Miles and the CD do.

```sh
build/dev/games/cultures1/cultures-cultures1 --check --game-dir orig/cultures1/gog-en   # imports per DLL
RC_TRACE=check …   # … and the missing ones by name;   RC_TRACE=imports: every API call while running
```

Scripted runs (`hle/frames.c`): `RC_INPUT=script.txt` (lines `FRAME move X Y`, `FRAME down|up BUTTON`,
`FRAME key|keyup VK`, …), `RC_SCREENSHOT=N:path.bmp,…`, `RC_FRAMES=N` to end after frame N, and `RC_CLOCK=virtual`
so a run repeats exactly. `SDL_VIDEO_DRIVER=dummy` runs them without a display. Screenshots are of the game, so they
stay local like everything else derived from it.

Network games: start one program, create a session (Multiplayer → TCP/IP → Create), start the other and join with
the host's address (`127.0.0.1` on the same machine). Sessions on the LAN are found by broadcast as well;
`RC_DPLAY_HOST=name` adds a host to ask. `RC_TRACE=dplay` logs discovery and message counts. Scripted runs of two
programs are lined up with `FRAME wait PATH` (the script pauses until the file exists; the game runs on) and
`FRAME shot PATH` (a screenshot of the next frame), the test script creating the files when both are ready.

## Inspecting a binary

```sh
recomp info orig/cultures1/gog-en/Cultures.exe      # sections, imports, entry, relocations, Rich header
```

## Building

```sh
cmake --preset dev            # Debug, build/dev, tests on
cmake --build --preset dev
ctest --preset dev

cmake --preset release        # RelWithDebInfo, build/release
```

The game shells end up in `build/<preset>/games/<id>/cultures-<id>`.

SDL3 is used when CMake finds it (`-DRC_WITH_SDL3=ON|OFF|AUTO`, default `AUTO`). Without it, the null
platform is built. That is enough for tests.

Once lifting exists (M3+), `cmake/lift.cmake` picks up `generated/<game>/**/*.c` automatically. Without
generated code, each game shell builds and only reports that the code has not been lifted yet.

## Rust toolchain

```sh
cd recomp
cargo fmt --all --check
cargo clippy --all-targets -- -D warnings
cargo test
cargo test -- --ignored        # tests that need your files in ../orig/
```

## Before committing

```sh
git config core.hooksPath tools/hooks     # once per clone: pre-commit (audit) + commit-msg
tools/audit.sh                            # what the pre-commit hook and CI run
```

## Adding a new variant or binary

1. Hash it: `recomp info <file>` prints the SHA-256 and the PE facts.
2. Add `[[variant]]` / `[[binary]]` entries to `games/<id>/game.toml`.
3. Add the rows to `docs/TARGETS.md`.
4. `cargo test` checks the manifest for consistency.
