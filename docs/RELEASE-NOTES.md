# Release notes (draft)

Draft for the first release, v0.1 (Cultures 1). Nothing is released yet. The notes say what has been shown to work
and what is known not to, so that players know what to expect.

## What you need

- Your own copy of the game: a GOG installer (English or German) or the German *Cultures Gold* CD image. Nothing of
  the game is included or downloaded.
- For now a build environment: CMake, a C compiler, Rust, and SDL3 with SDL3_ttf and SDL3_mixer (Vorbis for the GOG
  music). `recomp convert <installer>` unpacks, recompiles and builds in one step and leaves `out/cultures1/play.sh`.
  Compiling the recompiled game takes a few minutes and about 0.6 GB of memory per compile job.

## What works (Cultures 1)

Shown in scripted runs on Linux (input from a script, screenshots compared by eye):

- Intro and other Bink movies with sound; menus; the campaign and the scenarios start and run.
- Sound effects (Miles), and music: GOG's OGG files, or for the Gold CD the tracks you rip (`MUSIC/TrackNN.ogg`).
- Saving and loading.
- Network games over LAN or the internet (by IP address) between two copies of the recompiled game, on our own
  network layer under DirectPlay 4.
- An hour of play with random input in a scenario without a fault (216,000 frames on the virtual clock).
- Gold: both campaigns; the add-on's first mission starts.

## Known issues

- No mission has been played to its end by hand yet; problems that only show late in a mission are possible.
- Game speed against the original has not been measured.
- Only Linux has been built and tested. CI is set up for Windows and macOS, but they have never been built.
- The Gold edition's `Intro2` and `Extro2` movies: when the game plays them is not known yet.
- Network games stay in lockstep for an hour (checked with the game running on by itself); a long game with both
  players giving commands has not been tested.
- Network games only between copies of this project, not with the original Windows game (our network replaces
  DirectPlay's own protocol).
- Without a desktop (headless runs) message boxes are answered automatically.

## Beyond v0.1

Cultures 2 needs your serial at its first start and is not playable further yet. Northland, 8th Wonder and the
Saga start their missions, play their movies, sound and music, and their map editors work; they are planned for later
releases (`docs/PLAN.md`, M10 and M11).
