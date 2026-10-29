# Build and installation guide

This guide covers all six games. There are two separate prerequisites: a compatible firmware launcher on the phone, and the game files on the microSD card. Copying an SD folder does not install the launcher.

## 1. Establish the firmware prerequisite

For an already qualified PEAQ installation, continue to the SD instructions below. Small games need ABI 3 or newer. DOOM and Racer need the native ABI 5 service integration with descriptor revision 2 and the corrected game cleanup behavior. Emerald additionally needs the matching OS boot gate, runtime bindings and SD storage fixes. An ABI number alone is not a complete firmware fingerprint.

For an unmodified phone, the public installation procedure is **not complete**. There is no general-purpose public flasher in this release. The working development installation was established through these concrete stages:

1. Read the original flash independently twice and compare hashes. Keep an untouched backup, including the information needed to interpret NAND geometry, bad blocks and remapping. A USB mass-storage copy is only a file backup.
2. Establish a download connection independent of the installed OS. On the development phone, a battery reset, holding **8** and connecting a working **USB-A-to-C data cable** entered download mode; the key was held for about 20 seconds before release. This describes the tested phone, not a universal boot-key table.
3. Qualify the loaders and bounded partition writer, rehearse without writing, restore the original image, read it back fully and confirm normal boot.
4. Build patches from that phone's compatible original User/OS images, apply only the qualified regions, verify full readback and compare protected regions.
5. Verify normal boot, the Games menu, repeated game transitions and storage before treating that firmware as a suitable SD-package target.

The 16 MiB User and 14 MiB OS regions already exist on the development handset. Their sizes are not instructions to repartition a different phone. A build of a patched image is also separate from qualifying a writer for another device. [Release status](release-status.md) lists the unfinished public work.

## 2. Set up the offline build environment

Use a checkout of this repository, Python 3.10+, Git and LLVM. The verified Windows native build uses **LLVM 22.1.6** and **Arm GNU Toolchain 14.2.Rel1**, including Newlib. `clang`, `ld.lld`, `llvm-nm`, `llvm-objcopy` and `llvm-size` must be available. `PEAQ_LLVM` can point to LLVM's `bin` directory.

```powershell
python -m pip install -r requirements.txt
$env:PEAQ_LLVM = 'C:\Tools\LLVM\bin'
$env:PEAQ_GNU_ARM = 'C:\Tools\arm-gnu'
```

Replace the two example tool locations with your installation paths. `PEAQ_GNU_ARM` names the installation root containing `bin/arm-none-eabi-gcc`, not the `bin` directory itself. The package builders check the qualified engine fingerprints; a toolchain change can require new qualification rather than deletion of those checks.

The commands below build local files. They do not flash firmware or write to a connected phone. `build/`, `deps/`, `local/` and `dist/` are ignored by Git.

### Local SD identity and offline firmware patching

Emerald's boot gate and runtime validate both the SD CID and its sector capacity. Supply your own verified controller-order CID as four unsigned 32-bit words and the capacity in 512-byte sectors. Keep this file at `local/sd-profile.json`. Its only fields are `cid_words` (four integers, or `0x`-prefixed hexadecimal strings) and `capacity_sectors` (a positive integer). No development-card identity or automatically accepted default is included. The current public tools do not yet provide a qualified device-side CID reader; obtaining this profile is one of the remaining first-install prerequisites.

Use the **same profile** for the firmware, runtime and package commands. Their reports record a canonical profile fingerprint; the package builder rejects a different profile from the runtime. Changing the card requires matching regenerated bindings and qualification.

The offline builder accepts only the known original 16 MiB User and 14 MiB OS hashes encoded in `tools/build_known_firmware.py`:

```powershell
python tools/build_known_firmware.py --original-user local/User-original.bin --original-os local/OS-original.bin --sd-profile local/sd-profile.json
```

It writes `build/known-firmware/User-Games.bin`, `OS-MenuBoot.bin` and a build report. A User-only build can omit `--original-os` and `--sd-profile`; that does not add Emerald's OS boot gate. Matching development inputs reproduced the qualified patched images byte for byte. These outputs contain the supplied vendor image and remain local. This builder neither reads nor flashes a phone. The historical reader/writer used device-specific bad-block and transport information; a public backup, recovery and bounded flashing workflow is still unfinished.

## 3. Tic Tac Toe, Tetris and Snake

Build the three included games:

```powershell
python tools/build_games.py
```

| Build output | Destination on the card |
| --- | --- |
| `build/Games/TicTacToe/game.pga` | `Games/TicTacToe/game.pga` |
| `build/Games/Tetris/game.pga` | `Games/Tetris/game.pga` |
| `build/Games/Snake/game.pga` | `Games/Snake/game.pga` |

Copy the packages into these game directories. On an update, replace the package file and retain each folder's existing `save0.dat` and `save1.dat`. Safely eject the card, return it to the phone and open the Games category. These games have title screens, tutorials, music/SFX settings and saved progress or records. Their current UI is German; the repository guide is English.

Use the phone upright:

| Game | Main controls |
| --- | --- |
| Tic Tac Toe | D-pad selects a square, OK places a mark; 1–9 directly select the corresponding board square |
| Tetris | Left/right or 4/6 move; Up or 2 rotates; Down or 8 soft-drops; OK or 5 hard-drops |
| Snake | D-pad or 2/8/4/6 steers; OK or 5 pauses |

Right softkey or `#` opens pause during play. In menus, Up/Down or 2/8 selects, OK or 5 confirms, and the right softkey goes back. Side keys adjust volume. The lowest volume is mute. Leave through the game menu so it can finish saving.

## 4. Licar Racer

Fetch the pinned [Licar source](https://git.coom.tech/drummyfish/Licar):

```powershell
git clone https://git.coom.tech/drummyfish/Licar deps/licar
git -C deps/licar checkout d31205dde1ca79ce9314fe6b18cafeca1fa6f3a7
python tools/build_licar.py --upstream deps/licar --relocatable
python tools/build_licar_package.py --upstream deps/licar
```

The complete local folder is `build/licar-pga/Racer/`:

```text
Racer/
  game.pga
  engine.pge
  data
  music.u8
  LICENSE
```

Copy this folder to `Games/Racer/` on a card used with the qualified native launcher. Preserve any existing user files when updating. Assets are read from the pinned Git blobs; `data` and `LICENSE` use CRLF text endings to reproduce the qualified Windows package, while `music.u8` is copied unchanged. The included music data belongs to upstream Licar; this integration currently has no native audio output, and replay/save appends are not integrated.

Hold the phone **90° counterclockwise, keypad on the right**. The rotated D-pad steers and navigates. `5` accelerates, `4` brakes/reverses and `2` resets the car. Select **play map**, choose a map, then confirm with `0` or the center OK button. Right softkey returns from a race to the race menu; from the menu it exits. After a focus pause, `0` or OK resumes.

## 5. DOOM with Freedoom Phase 1

Fetch the pinned [DoomGeneric source](https://github.com/ozkl/doomgeneric):

```powershell
git clone https://github.com/ozkl/doomgeneric.git deps/doomgeneric
git -C deps/doomgeneric checkout e257a09527062bfb986b6e5e0780839b6cbd5b71
python tools/build_doom_engine.py --upstream deps/doomgeneric --toolchain $env:PEAQ_GNU_ARM --relocatable
python tools/link_doom_engine.py --upstream deps/doomgeneric --toolchain $env:PEAQ_GNU_ARM --relocatable --guarded
python tools/build_doom_package.py --toolchain $env:PEAQ_GNU_ARM
```

The local output is `build/doom-pga/Games/Doom/`. Obtain **Freedoom 0.13.0, Phase 1** from its [official release](https://github.com/freedoom/freedoom/releases/tag/v0.13.0). Extract `freedoom1.wad` and place it beside `game.pga` and `engine.pge`; retain the Freedoom license and credits with the data. The tested package expects this exact filename. Commercial DOOM data is not supplied by this repository.

```text
Games/Doom/
  game.pga
  engine.pge
  LICENSE-DoomGeneric.txt
  freedoom1.wad
```

Copy the complete folder and notices to the card used with the qualified native launcher. Open DOOM from Games. Native sound output and game saves are not integrated in this version.

Hold the phone 90° counterclockwise. The rotated D-pad moves forward/backward and strafes. `5` turns left, `8` turns right, center OK fires and confirms menus, `0` uses/opens, and `*` opens the DOOM menu. Right softkey exits. DOOM has no jump binding. After a focus pause, OK resumes; the resume press is consumed.

## 6. Pokémon Emerald

The public source contains the phone runtime, renderer, launcher glue and minimal platform edit records. Game content is a separately supplied local build input. The [RP2350 port](https://github.com/mattdeeds/pokeemerald-rp2350/tree/3c35e5b957dff6c578878031e0c4a7c8d7691401) is pinned to `3c35e5b957dff6c578878031e0c4a7c8d7691401`; its MIT license covers its platform code, not the surrounding Pokémon decompilation or assets. This repository supplies no ROM, Nintendo assets or personal save.

The complete local pipeline below has built the clean English engine, runtime, launcher and fresh/update folder sets. Its engine passed the documented host smoke test. The new public runtime/package has **not** been tested on a handset. Firmware compatibility, real SD saves, display/audio and exit must be qualified before treating it as an SD update for a phone.

### Build the clean English package

The verified host setup is Windows with Visual Studio 2022 C++ Build Tools, its Windows SDK, CMake, LLVM 22.1.6, Arm GNU 14.2.Rel1 and Git for Windows' Unix tools. The host-tool helper currently uses the Visual Studio 17 2022 CMake generator. Keep the checkout path reasonably short because MSBuild adds nested configuration directories.

Place a locally supplied Emerald upstream checkout at `deps/pokeemerald-rp2350` and check out the pin above. Source preparation makes a separate clean checkout at `build/emerald-source`; it ignores the input checkout's working changes, applies hash-checked platform edits and records their fingerprints. Existing prepared output is verified with `--verify`, not silently replaced.

The additional source dependencies are:

```powershell
git clone https://github.com/madler/zlib.git deps/zlib
git -C deps/zlib checkout 767c4c947852e143f582c85f14cf573411df1b35
git clone https://github.com/pnggroup/libpng.git deps/libpng
git -C deps/libpng checkout 0b0876829d77db0827a8cc2c4326f63be22b2aa1
git clone https://git.savannah.gnu.org/git/make.git deps/gnu-make
git -C deps/gnu-make checkout b3802782de3eff2c0f1eda9e7c0befd8cd142162
```

Build GNU Make using that checkout's `README.W32` instructions (`build_w32.bat --without-guile` in its directory with MSVC available). The checked asset run used GNU Make 4.4.90 from this revision. Set the following tool paths to your actual installations, then run each stage from this repository root:

```powershell
$cmake = 'C:\Tools\CMake\bin\cmake.exe'
$make = 'C:\Tools\GNU-Make\gnumake.exe'
$shellTools = 'C:\Program Files\Git\usr\bin'

python tools/prepare_emerald_source.py --upstream deps/pokeemerald-rp2350
python tools/build_emerald_host_tools.py --upstream build/emerald-source --zlib deps/zlib --libpng deps/libpng --cmake $cmake
python tools/prepare_emerald_assets.py --upstream build/emerald-source --make $make --shell-tools $shellTools --toolchain $env:PEAQ_GNU_ARM
python tools/build_emerald_archive.py --upstream build/emerald-source --toolchain $env:PEAQ_GNU_ARM --relocatable --stage sound --jobs 4
python tools/build_emerald_archive.py --upstream build/emerald-source --toolchain $env:PEAQ_GNU_ARM --relocatable --stage engine --jobs 4
python tools/link_emerald_package_engine.py --toolchain $env:PEAQ_GNU_ARM
python tools/build_emerald_package_runtime.py --sd-profile local/sd-profile.json
python tools/build_emerald_package.py --sd-profile local/sd-profile.json
python tools/check_emerald_public_engine.py --frames 600
```

The asset step generates maps, graphics and audio inputs inside the ignored prepared checkout. The archive builder rechecks the platform manifest and rejects unrelated tracked-source changes. The runtime builder regenerates engine sizes, header, exports, CRC, allocation, symbol offsets and local card bindings. The final builder generates a launcher bound to that runtime, blank dual save records, inactive launch records and the required reference file.

`build/emerald-package/fresh/Games/` contains:

```text
Games/
  COLDCHK.BIN
  Smaragd/
    game.pga
    PLAY.BIN
    ADAPT.PGE
    save0.dat
    save1.dat
    ENGINE0.SAV
    ENGINE1.SAV
    REPORT.BIN
```

The legacy `Smaragd` folder name is required by the existing boot gate even though this public game and launcher are English. The parallel `update/Games/` folder omits `ENGINE0.SAV`, `ENGINE1.SAV` and `REPORT.BIN`, preserving real saves and runtime preferences when copied over a compatible installation. It contains fresh inactive launch records tied to the new runtime. Never copy the fresh folder's blank real save pair over an existing game. These folders are locally built content and are not repository release assets.

After qualification for the target firmware and SD profile, copy the selected `Games/` contents to the card's root, preserving that layout and any existing real saves/settings. Safely eject before returning the card to the phone. See [release status](release-status.md) for what still needs handset verification.

The established integration works as follows: select Emerald in Games and allow the automatic restart into its dedicated runtime. Use the original game's Save command and wait for completion. Right softkey opens the pause/exit interface; confirming Exit returns to the phone OS. The normal phone OS is not running during the dedicated game session.

| Physical key, phone 90° counterclockwise | Emerald action |
| --- | --- |
| Rotated D-pad | Directional input |
| 8 | A |
| 7 | B |
| `#` | Start |
| `*` | Select |
| 5 | L |
| 0 | R |
| Right softkey | Pause/exit interface |
| Side volume keys | Volume; lowest level mutes |
| M1 / M3 | Brighter / dimmer |

The pause settings provide screen mode and optional FPS/battery-voltage overlays. Display refresh and game FPS measure different things.

### Required SD reference and save files

`Games/COLDCHK.BIN` is **required runtime data on the microSD card**, outside the Emerald game folder. It has no visible Games-menu entry. The startup SD-speed check reads this file before loading the engine. Deleting it during diagnostic cleanup caused startup error `02` on the development phone.

It is a deterministic 32,768-byte file. For byte position `p` from 0 through 32767, its value is:

```text
((p * 73 + (p >> 8) * 19 + 0x35) ^ (p >> 3)) & 255
```

Its SHA-256 is `5e32bd3f1e1ff09927b1c94ad3974e03f72dcb0224b81820505b4417a44084b9`. Keep this file when removing diagnostic apps. It is not a personal dump and does not belong inside `Games/Smaragd/`.

The runtime uses `ENGINE0.SAV` and `ENGINE1.SAV` for the real game saves. Its `save0.dat` and `save1.dat` are launch-request records, so they must not be confused with the Pokémon save pair. `REPORT.BIN` also stores runtime preferences. The package builder creates its own blank records; it never imports an owner's save as a default.

## 7. Updating compatible SD packages

An SD-only update replaces the designated game/runtime files after their build has been qualified for the installed launcher. It does not need another firmware flash. Keep a copy of the previous game folder and preserve its saves and settings. Native engine files must match their adapters; mix-and-match `engine.pge` and `game.pga` pairs are not a supported update.

When a game fails to appear, check the exact `Games/<folder>/game.pga` layout and launcher compatibility first. For Emerald startup error `02`, check the required reference file and its hash before assuming that the engine crashed. An old, unchanged runtime report can precede the failed launch and is not evidence of a successful new session.
