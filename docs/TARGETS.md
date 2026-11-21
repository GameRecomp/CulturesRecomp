# Targets

This file lists every input the project accepts, pinned by SHA-256. It must stay in sync with
`games/<id>/game.toml`, which is the machine-readable source. The test
`rc-manifest::all_manifests_are_consistent` checks the manifests themselves.

The recompiler refuses any binary whose hash is not listed. A new build (another
language, another store, another patch level) is added to the manifest first. Only then is it lifted.

Run `recomp identify <path>` to check your files against this list.

## Installers and media

| Game           | Variant       | File                                   | Size (bytes) | SHA-256 |
|----------------|---------------|----------------------------------------|-------------:|---------|
| `cultures1`    | `gog-en`      | `setup_cultures_2.1.0.12.exe`          | 195201296 | `4800a05b4c32cc8ae6fbce76732f605894aa4536a69b11875df01e9d4c3eb3b2` |
| `cultures1`    | `gog-de`      | `setup_cultures_german_2.1.0.12.exe`   | 215289544 | `e8bf7d293b85d5efd073be131df2121c2ceab63d05a43c89df0500c2b9e31542` |
| `cultures1`    | `gold-de`     | `Cultures (Gold Edition).iso`          | 433559552 | `a7f8631a1faf09de1da0a5cf3866c379cea1e3a51d2bb4584b1682c659586674` |
| `cultures2`    | `gog-en`      | `setup_cultures2_2.1.0.10.exe`         | 297513816 | `cc8f07c30cb1320444022ed163c278db6b392de04b49e00650cc615d2ed48e87` |
| `cultures2`    | `gog-de`      | `setup_cultures2_german_2.1.0.11.exe`  | 332675808 | `fee5ba73a65f53d3a3df5217a8038275067dc64f7bdf287769bb5862fa742632` |
| `northland`    | `gog-en`      | `setup_northland_2.1.0.9.exe`          | 185408048 | `b21024df011a62c218f59907d3fb0a81a6414fe942a1fde8b402041678dff3a0` |
| `northland`    | `gog-de`      | `setup_northland_german_2.1.0.9.exe`   | 188623424 | `26b1b09e1a6912139111697fa3f1cb389aa32a86d59aa252079cf17575514da4` |
| `northland`    | `gog-es`      | `setup_northland_spanish_2.1.0.9.exe`  | 211958320 | `b435eb427081f2a5dc5a8cd79241eee60d2be2ed9fb7c84a15b4198b1f97e5c5` |
| `eighthwonder` | `gog-en`      | `setup_8th_wonder_2.1.0.9.exe`         | 208859192 | `8a0a624791581cfa6ef3efe4b2ab730a4766615b423689331b24959c4946b94d` |
| `eighthwonder` | `gog-de`      | `setup_8th_wonder_german_2.1.0.9.exe`  | 210929320 | `866b833e7d8110861332f439cf331d92e70e44bda81461c299dd1aea1b569139` |
| `eighthwonder` | `gog-es`      | `setup_8th_wonder_spanish_2.1.0.9.exe` | 215327224 | `5888b0e1050ece11536f19677df0f20eda252a8619d2d45b36cc45305c3a50bc` |
| `saga`         | `retail-de`   | `Cultures Saga.zip`                    | 292247022 | `af79b2c20ede90152e40469de7f05d874174218aef6149989deeb1700fa0dba5` |

The GOG installers are Inno Setup 5.5 (unicode). The Gold Edition is an ISO 9660 image of
the German CD, with an InstallShield 5/6 setup (`DATA1.CAB`/`DATA1.HDR`). The ISO
holds only the data track. The original CD-audio music tracks are not in it (see
[RESEARCH.md](RESEARCH.md#cd-audio)). The Saga is a zip of an installed copy of the German CD
(original directory layout). Its bonus CD with Cultures 1 is not included.

GOG product ids: Cultures `1207663793`, Cultures 2 `1207663813`, Northland `1207663833`,
8th Wonder `1207663853`.

## Executables (lifted)

All of them are PE32 i386 GUI programs with image base `0x00400000` and stripped relocations, linked by MSVC 6.0.

| Game           | Binary            | Variants              | Size    | Build date (PE)  | Entry      | SHA-256 |
|----------------|-------------------|-----------------------|--------:|------------------|------------|---------|
| `cultures1`    | `Cultures.exe`    | gog-en                | 782336  | 2001-06-21       | `0x49a133` | `65417291a96241d650d5664b6b40ee7a1485f963f5cc959760a951130a1cbfc1` |
| `cultures1`    | `Cultures.exe`    | gog-de                | 782336  | 2003-02-04       | `0x49a133` | `3fc2f057f169013568816ba1fb48e5926536eb9da82bc71a913adeece322099c` |
| `cultures1`    | `Cultures.exe`    | gold-de               | 786432  | 2001-10-11       | `0x49b093` | `ff5b8acd577fa77e81e2f1a89c4da3555a007d7ded783c4f1bf4008d9e6cecf6` |
| `cultures2`    | `Cultures2.exe`   | gog-en, gog-de        | 1393664 | 2017-06-23 ¹     | `0x4d229e` | `e3ba0b2216f75fa2dd53e70974cab8c43dac52212462d89f0e22043bdcab4fde` |
| `cultures2`    | `Cultures2MP.exe` | gog-en, gog-de        | 1110016 | 2002-04-11       | `0x4d152e` | `01beaa3189791df220a30564f4ee261cede3a8935cb028cdbd22ab1eb7d4414a` |
| `northland`    | `Game.exe`        | gog-en, gog-de, gog-es| 1122304 | 2003-09-09       | `0x4e0a7e` | `538fce0d3c60c981f8237c40a9fd8aa0572324416a93076e71ad30541973b56d` |
| `northland`    | `GameMp.exe`      | gog-en, gog-es        | 1175552 | 2003-06-02       | `0x4e039e` | `698539de05c4b79c3731e438fbfffa39cc480e932a95c55656edee548e2c819d` |
| `northland`    | `GameMp.exe`      | gog-de                | 1114112 | 2003-09-04       | `0x4df66e` | `c2657bcf1e9c4707c8d984b52b63a8daae83bbd6f8d4c33c9afc07f9e0f14bdb` |
| `northland`    | `Editor.exe`      | gog-en, gog-es        | 1105920 | 2003-06-04       | `0x4a61e6` | `43e34c821ae456f56f9179cccc8a87f91f4d67efa0caf5f76c9a24fa78c29019` |
| `northland`    | `Editor.exe`      | gog-de                | 1110016 | 2003-09-04       | `0x4a61e6` | `a31a3c4051820897e6e565810bdaffca5e04eb1e011756b4f6761d9b84d3ed43` |
| `eighthwonder` | `Game.exe`        | gog-en, gog-de        | 1146880 | 2003-11-29       | `0x4e709e` | `7ea1ff9d9cac9a2df1155379a0b314586a538cc5433430d48c34aa3b0188edb4` |
| `eighthwonder` | `Game.exe`        | gog-es                | 1138688 | 2003-06-19       | `0x4e4efe` | `5d0c5751ab2a8f18ad2a9c428e08301d8ad965b5217bee854751b3a1fc7f49fc` |
| `eighthwonder` | `GameMp.exe`      | gog-en                | 1134592 | 2003-06-21       | `0x4e3ade` | `bdf07c34afe355b54be9dd512f4a267e79700d177d21d65c22f0ae8d460a9894` |
| `eighthwonder` | `GameMp.exe`      | gog-de                | 1142784 | 2003-11-29       | `0x4e5c4e` | `f3d9715525d77306f3eb81498f098e433a5e605441add9d8fbb291be172fc407` |
| `eighthwonder` | `GameMp.exe`      | gog-es                | 1134592 | 2003-06-19       | `0x4e3ade` | `0488895e8f0dbbddfb1dfac646c2631742bd71c7167c9cbc666058a093f1e815` |
| `eighthwonder` | `Editor.exe`      | gog-en, gog-es        | 1118208 | 2003-06-19       | `0x4a9446` | `30d05acc22d22de33da9c2553d97a5d934fa7698247a404c9507573bef087c7d` |
| `eighthwonder` | `Editor.exe`      | gog-de                | 1126400 | 2003-11-29       | `0x4aa4b6` | `383afe583317a00844392f55bc4605215f267ce564f062e0bd1618db06399eaa` |
| `saga`         | `Game.exe`        | retail-de             | 1159168 | 2003-11-05       | `0x4e91be` | `1ac4f291e80fd29fcf6a9e9222bc15435ddb87344815f0095f3491577e3cbf15` |
| `saga`         | `GameSaga.exe` ²  | retail-de             | 1146880 | 2005-07-18       | `0x4e687e` | `a712d51a339547b17f39c8a0a33cedffa5868a8d0ebb193b8a3093a7174c9801` |
| `saga`         | `GameMp.exe`      | retail-de             | 1150976 | 2003-11-05       | `0x4e6e1e` | `5a3c57c045bdd9168e7e5b85d76656143bdfd85c87afc4edecf8c0dd0879257f` |
| `saga`         | `Editor.exe`      | retail-de             | 1126400 | 2003-11-02       | `0x4aa4b6` | `4cecdd9fb148ec8c5f5b3ca8e30b225398b0ef94cd442bd24a17ead553e85360` |

¹ The GOG `Cultures2.exe` was rewritten in 2017. It has lowercase import names, a rebuilt `.idata`, extra
`.cms_t`/`.cms_d` sections and no Rich header. Its embedded build string still reads `Apr 11 2002`.
See [RESEARCH.md](RESEARCH.md#cultures-2-gog-executable).

² A second single-player build shipped next to `Game.exe`, without its extra checks. It is the Saga's role `main`, the
program the shell runs; `Game.exe` is `alternate` (`--exe Game.exe`). See [RESEARCH.md](RESEARCH.md#die-saga).

Most executables are identical across languages, so the language lives in the data. That gives
**9 distinct single-player executables** to lift: three for C1 and six for the C2 engine (including both Saga builds).

## Middleware DLLs

| Binary               | Used by                 | Treatment | SHA-256 (variants) |
|----------------------|-------------------------|-----------|--------------------|
| `binkw32.dll`        | cultures1, cultures2    | lift      | `70f28f6e469eceaecd5e5795d58b017f096a536227584c2684bc74cd279bcce8` (c1 gog-en, gold-de) · `ee68dcf51e8e3b482f9fb5ef617f44a2d9e970d03f3ed21fe26d40cd63c57a48` (c1 gog-de, c2) |
| `Mss32.dll` (Miles)  | cultures1, cultures2    | HLE       | `fe62f1eb6ba407df77619d16927abbefad3c726014f6bd1f8c37a7c3d6b781cc` (c1 gog-en, gold-de) · `2a515199cc7a2e1ccd8d756efdaab07cedd5c3052309ef1c7499c2560e2097bd` (c1 gog-de, c2) |
| `Mp3dec.asi`         | cultures1, cultures2    | ignore (Miles HLE decodes MP3 itself) | `51765b5a…` (c1) · `0a6db88a…` (c2) |
| `DPlayDLL.dll`       | cultures1               | lift (DirectPlay 4 below it is HLE), rebased to `0x11000000` | `628db393586430dd3d867777669b3db6800b68c78a1eedf53299f85f37f1afd2` (gog-en) · `3b0169c7e0ecb61e10fdc8c1f48e0e3eddddd2c1e31af7381a4d26c837fefb64` (gog-de, gold-de) |
| `gedx8musicdrv.dll`  | cultures2, northland, eighthwonder, saga | lift (DirectMusic below it is HLE) | `547c5571c39dbcb921a2b8fff2d1568f11ceab9b79a7a2d5a2a82fb5601395bf` (all) |
| `winmm.dll` (GOG)    | cultures1 GOG           | ignore (GOG's CD-audio-to-OGG wrapper; our `winmm` HLE does the same) | `3dfadd86…` (en) · `f2006e09…` (de) |
| `libogg-0.dll`, `libvorbis*.dll` | cultures1 GOG | ignore (dependencies of the wrapper above) | — |
| `goggame-*.dll`, `GameuxInstallHelper.dll`, `Lobby/Lobby.exe` | GOG extras | ignore | — |
| `Autorun.exe`, `Register.exe`, `Unregister.exe`, `Uninstall.dll` | Saga retail extras | ignore | see `games/saga/game.toml` |

Unlike the executables, every DLL above has relocations. Their preferred bases (and the `binkw32.dll` /
`DPlayDLL.dll` collision in Cultures 1) are listed in [RESEARCH.md](RESEARCH.md#dll-base-addresses).
