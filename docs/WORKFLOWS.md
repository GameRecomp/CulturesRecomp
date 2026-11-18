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

## Extracting (until `recomp extract` exists, M1)

`tools/dev-extract.sh` wraps `innoextract` (GOG) and `unshield` + `isoinfo` (Gold ISO):

```sh
tools/dev-extract.sh orig/setup_cultures_2.1.0.12.exe   cultures1 gog-en
tools/dev-extract.sh "orig/Cultures (Gold Edition).iso" cultures1 gold-de
```

The result is `orig/<game>/<variant>/` with the installed layout (for example `Cultures.exe`, `data_l/`, …).

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

SDL3 is used when CMake finds it (`-DRC_WITH_SDL3=ON|OFF|AUTO`, default `AUTO`). Without it, the null
platform is built. That is enough for tests.

Once lifting exists (M3+), `cmake/lift.cmake` picks up `generated/<game>/<variant>/*.c` automatically. Without
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
git config core.hooksPath tools/hooks     # once per clone
tools/audit.sh                            # what the hook and CI run
```

## Adding a new variant or binary

1. Hash it: `recomp info <file>` prints the SHA-256 and the PE facts.
2. Add `[[variant]]` / `[[binary]]` entries to `games/<id>/game.toml`.
3. Add the rows to `docs/TARGETS.md`.
4. `cargo test` checks the manifest for consistency.
