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

## Checking an install against the runtime

```sh
build/dev/games/cultures1/cultures-cultures1 --check --game-dir orig/cultures1/gog-en
RC_LOG=debug build/dev/games/cultures1/cultures-cultures1 --check --game-dir orig/cultures1/gog-en   # + list
```

`--check` maps the exe with the runtime's PE loader, resolves every import and prints, per DLL, how many imports the
HLE layer implements. It needs no lifted code.

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
