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
$recomp extract --game eighthwonder orig/                 # the game from whichever of its media orig/ holds
$recomp list orig/setup_cultures2_2.1.0.10.exe              # just list the files
```

An unknown medium needs `--dest`. A destination that is already unpacked is left alone (only a missing
`registry-install.txt` is added); `--force` unpacks into it again. A GOG installer takes 10–20 s.

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
$recomp lift orig/cultures1/gog-en                       # a folder: every binary in it the manifest lifts
$recomp lift --comments --out build/look orig/cultures1/gog-en/Cultures.exe   # each instruction above its C
cmake --preset ci && cmake --build --preset ci            # compiles everything below generated/<game>/
```

`recomp lift` runs discovery with the manifest hints, lifts every function and writes the C next to the code map.
It prints the trapping instructions by kind (`--verbose` for all). Every lifted binary of a game is listed in
`generated/<game>/set.c` and linked into `cultures-<game>`. Delete a binary's directory and lift another one to
drop it. Like the code map, the C is derived from the game and stays local (`--out` into `build/` for a look).
Only files whose content changed are written, so a build compiles only those again.

The build lifts by itself (`cmake/lift.cmake`, as in TropicoRecomp): with the game in `orig/` and cargo there,
configuring lifts a game that has no `generated/<game>/set.c` yet, and building `cultures-<game>` (an IDE's debug
action too) builds `recomp` and lifts again whenever `games/<game>/game.toml` or the lifter changed since the last
lift. A game with no folder in `orig/<game>/` yet is first unpacked from its installer, CD image or zip in
`orig/` (`recomp extract --game`, the first variant in the manifest whose medium is there), so putting the media into
`orig/` and building is enough. It lifts `orig/<game>/gog-en`, else the first folder in `orig/<game>/`; `-D<GAME>_GAME_DIRS` (e.g.
`-DCULTURES1_GAME_DIRS="orig/cultures1/gog-en;orig/cultures1/gold-de"`) names the installs instead; each lifted
executable is about 40 MB of C to compile. Every other folder in `orig/<game>/` gets a target of its own,
`cultures-<game>-<folder>` (e.g. `cultures-cultures1-gold-de`, `cultures-cultures1-gog-de`), which lifts and links
only that install and finds it without `--game-dir`: pick it as the debug target to debug another build. These
targets are not part of `all`; each lifted binary is compiled once and shared between the targets that link it.
`-DRC_AUTO_LIFT=OFF` turns it off.

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
with a launcher into `out/<game>/`. Both stay on your machine: they contain code derived from your copy. The build
runs one compile job per CPU, but no more than one per 1.5 GB of available memory (about 0.6 GB per file of
generated code); `CMAKE_BUILD_PARALLEL_LEVEL=N` sets the number instead. Unpacking an Inno installer also writes its registry entries
to `registry-install.txt` in the unpacked folder (for an install unpacked before, `recomp extract` of the same installer adds it and leaves the files alone, and so does `convert`), which the
runtime loads before the player's own `registry.txt`.

## Native code without a C compiler

`recomp lift --native host orig/cultures1/gog-en` compiles an install's lifted binaries with
Cranelift (rc-emit-native, from TropicoRecomp) instead of writing C: one blob per executable, with the
DLLs lifted with it, in `generated/<game>/native/<exe>-<variant>/code-<target>.bin` (targets: `host`,
`x86_64-sysv`, `x86_64-windows`, `arm64-linux`, `arm64-apple`; a few seconds per game). A program runs
it instead of compiled-in C when `RC_NATIVE` names the blob:

```sh
RC_NATIVE=generated/cultures1/native/cultures.exe-gog-en/code-x86_64-sysv.bin \
    build/dev/games/cultures1/cultures-cultures1 --game-dir orig/cultures1/gog-en
```

Packages that run without a compiler, as TropicoRecomp's converter makes them: a shell (the game's
program built without lifted code) is made into a template, and `recomp package` puts the install's
native code (`data/code-<target>-<exe>.bin`) and its files (`data/game/`) beside it:

```sh
recomp template --game cultures1 --system linux --shell <shell>/cultures-cultures1 --out Cultures-linux-template.zip
recomp package --template Cultures-linux-template.zip orig/cultures1/gog-en   # → out/cultures1/Cultures-gog-en-x86_64-sysv.zip
```

Unpacked, `Cultures/cultures-cultures1` runs the game from `Cultures/data/`. A shell is built with
`-DRC_AUTO_LIFT=OFF` and an empty `RC_GENERATED_DIR` (and SDL3).

`recomp convert --system SYSTEM INSTALLER_OR_ZIP` does what the converter page does, from the installer itself (or
the Saga's zip; manifests built in, no `games/` or `orig/` needed): it takes `<Game>-SYSTEM.zip` from `--templates
DIR` (by default `templates/` beside the program) and writes `<Game>-SYSTEM.zip` (`.apk` for Android) into the
current folder (`-o FILE` elsewhere; `--template FILE` names a template). `tools/package-converter RECOMP TEMPLATES
OUT.zip|OUT.tar.gz` packs the program with every template and a README.txt, as a release offers it; CI makes one
per desktop system (`converter-<system>` artifacts of `converter.yml`), and `release-upload.yml` (run by hand) puts
those of the last successful run on the latest GitHub release.

The difftest runs every instruction form through both back ends. A DLL the manifest rebases is named
`file@base` in the blob (process.c loads it there). This is the base of the converter's packages for
people without a compiler (M14), as in TropicoRecomp.

### Shells, AppImage and Android

`tools/build-shell OUT GAME` builds a game's shell with SDL3, SDL3_ttf and SDL3_mixer (Vorbis) linked
statically, from the releases pinned in `tools/sdl-versions`; `tools/make-appimage SHELL OUT.AppImage
build/icons GAME` wraps it (data/ then lies beside the AppImage). `tools/build-android OUT.apk GAME` builds the
Android shell APK (libmain.so for arm64-v8a and x86_64, SDL's activity, the game's icon); it needs the Android
SDK and NDK, which CI has. `recomp template --system android --shell <shell.apk>` and `recomp package` then make
a signed APK (`recomp verify-apk` checks the signature); the app unpacks its data on the first start.

### In the browser

`recomp lift --native wasm orig/cultures1/gog-en` writes the program's code as a WebAssembly module
(`code-wasm.wasm`) for the web shell. `tools/build-web-shell OUT GAME` builds the shell with Emscripten (emsdk
7.0.0, JSPI) into `OUT/<game>.js` and `.wasm` with the Liberation fonts. `tools/build-web SITE [TEMPLATES
[WEB_SHELLS]]` assembles the converter site: `web/` (the pages), rc-web (the converter, wasm32 through
wasm-bindgen 0.2.130), the templates and the web shells. On the site the GOG installer (for the Saga: a zip of
the folder it was installed to from the CD) is converted in the page, without uploading anything: for "Browser" its files and code go into the browser's private storage and
`play.html` runs them; for a system, the package is saved. Serve it locally with `python3 -m http.server` in
SITE; it needs a browser with WebAssembly stack switching (current Chrome, Edge, Firefox).
`.github/workflows/converter.yml` builds all of it and publishes the site to GitHub Pages (manual runs; the
systems in `platforms.json`).

## Title and icons

The window's title is the game's, then the version, the build's revision and the install's language, as
in TropicoRecomp: "Cultures 2: The Gates of Asgard 0.0.1 (07780ce, Deutsch)". The language comes from
GOG's `goggame-*.info`, else from the variant (`gold-de`: German). The revision is the commit CMake was
configured at.

Each game has an icon drawn for this project in the look of its own (`tools/icons/<game>.svg`: the
round, ringed badges of the GOG icons, with a Viking, a shield, a longship, a lighthouse, a rune stone).
`tools/make-icons` renders them to `build/icons/<game>/png/` and `build/icons/<game>.ico` (rsvg-convert,
else a headless Firefox); a Windows build then puts the `.ico` into the program.

## Running a game

```sh
CMAKE_PREFIX_PATH=<your SDL3 prefix> cmake --preset dev && cmake --build --preset dev   # lifts orig/<game>/gog-en
build/dev/games/cultures1/cultures-cultures1 --game-dir orig/cultures1/gog-en      # or set RC_GAME_DIR
```

Without `--game-dir` or `RC_GAME_DIR`, and without the game in the working directory, the shell looks for
`orig/<game>/` in the working directory, beside the program and in the directories above, and takes the first
install there whose executable was lifted into it, so a debugger can start it without arguments.

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
`FRAME key|keyup VK`, `FRAME focus 0|1` for the window losing or gaining the focus, …), `RC_SCREENSHOT=N:path.bmp,…`, `RC_FRAMES=N` to end after frame N, and `RC_CLOCK=virtual`
so a run repeats exactly. `SDL_VIDEO_DRIVER=dummy` runs them without a display. Screenshots are of the game, so they
stay local like everything else derived from it.

Network games: start one program, create a session (Multiplayer → TCP/IP → Create), start the other and join with
the host's address (`127.0.0.1` on the same machine). Sessions on the LAN are found by broadcast as well;
`RC_DPLAY_HOST=name` adds a host to ask. `RC_TRACE=dplay` logs discovery and message counts. Scripted runs of two
programs are lined up with `FRAME wait PATH` (the script pauses until the file exists; the game runs on) and
`FRAME shot PATH` (a screenshot of the next frame), the test script creating the files when both are ready.

`RC_RECORD=input.txt` writes what the player does as such a script; replayed with `RC_INPUT` and `RC_CLOCK=virtual`
(set both times) it repeats the run. `--exe NAME` runs another program of the install, e.g.
`cultures-northland --exe Editor.exe` (lift it first; `--check --exe Editor.exe` lists what it imports). Such programs (no DirectDraw) get a desktop of
`RC_DESKTOP=WxH` pixels (default 1024x768) in the game window. The map editors of Northland, 8th Wonder and
the Saga run this way (`recomp lift <install>/Editor.exe` first, then build); their file dialogs show the
installation folder, and maps are saved into the user directory. To play one, copy its exported `.c2m` into
`UserMaps/` of the game's user directory: it appears under Single Player → Free Play → User Maps.

## Display settings

Each game writes `settings.ini` into its user directory on the first start (with comments); the environment
overrides any value for a run (`RC_RESOLUTION`, `RC_HARDWARE_CURSOR`, `RC_SCALE`,
`RC_PIXELS`, `RC_FULLSCREEN`, `RC_FPS`).

| Key | Values | Effect |
|-----|--------|--------|
| `resolution` | `window x1` (default), `window x1.5`, `x2`, `x2.5`, `x3`, `screen`, `WxH`, `off` | The resolution the game plays at: the window's size (the screen's in full screen) divided by the factor (larger interface and figures; at least 800 × 600, its width a multiple of 8), the screen's, a fixed one (any shape), or only the game's own modes. Cultures 1's options pages offer the window sizes first and store a choice here; while a mission plays it follows the window when it is resized. The C2-engine games take the size when they start. The patches are in `games/cultures1/patch_resolution.c` and `games/c2engine/`; the menus stay 4:3. In the browser the player page chooses (default `window x1`). An older `game_scale` line still counts. |
| `hardware_cursor` | `yes` (default), `no` | The system's mouse pointer instead of the game's own (Cultures 1's setting is patched). |
| `scale`, `pixels`, `fullscreen`, `fps` | | How the frame is shown: `fit` or whole multiples, sharp or smooth, full screen (Alt+Enter), a frame limit. |

Scripted runs and regression screenshots use `RC_RESOLUTION=off`, so they do not depend on the window.

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

`cmake/lift.cmake` lifts the games found in `orig/` (see "Lifting") and compiles `generated/<game>/**/*.c` into
their shells. Without generated code, each game shell builds and only reports that the code has not been lifted yet.

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
