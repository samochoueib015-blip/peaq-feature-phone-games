# PEAQ Feature Phone Games

Native games for the **PEAQ PMP 122 4G**, a UMS9117 feature phone with a physical keypad: **Pokémon Emerald, DOOM, Licar Racer, Tic Tac Toe, Tetris and Snake**.

This is an independent hobby project. It adds a Games launcher to the phone's existing menu. The three small games, DOOM and Racer run inside the phone OS. Emerald starts a dedicated game runtime through an automatic reboot so it can use more RAM, then returns to the normal OS after a confirmed exit.

**Developer/source preview:** all six SD packages can be built locally. The clean public Emerald build has passed host checks, but has not been qualified on a handset. A known-firmware builder also reproduces the launcher and boot patches from exact matching original images. The installation path for an unmodified or different phone is still incomplete. Start with the [build and installation guide](docs/guide.md) and [release status](docs/release-status.md).

## Games

| Game | Public source/build | How it runs | Current limits |
| --- | --- | --- | --- |
| Tic Tac Toe | Included; `tools/build_games.py` | Games menu, portrait | Requires ABI 3 or newer; current menus/tutorials are German |
| Tetris | Included; `tools/build_games.py` | Games menu, portrait | Requires ABI 3 or newer; current menus/tutorials are German |
| Snake | Included; `tools/build_games.py` | Games menu, portrait | Requires ABI 3 or newer; current menus/tutorials are German |
| Licar Racer | Pinned external source; engine and SD package builders included | Games menu, landscape | Requires qualified ABI 5, descriptor revision 2; native audio and save/replay writes are not integrated |
| DOOM | Pinned DoomGeneric source; engine and SD package builders included | Games menu, landscape | Requires qualified ABI 5, descriptor revision 2; bring Freedoom Phase 1 data; native audio and saves are not integrated |
| Pokémon Emerald | Clean source preparation, engine, runtime and SD package builders; game content supplied separately | Dedicated landscape runtime | Original English gameplay; explicit local SD profile; public build awaits handset qualification |

DOOM and Racer's public engine and launcher builds reproduce the qualified development binaries byte for byte. The six integrations have been exercised on one development handset. The separately rebuilt public Emerald engine passed a 600-frame host smoke test through its title screen, with audio output and save-memory boundary checks. This does not qualify its SD runtime or another handset. Exact build fingerprints and remaining work are in [release status](docs/release-status.md).

## Emerald: original gameplay, platform repairs

The public configuration keeps the original starters, encounters, experience rules, evolutions and English game content. It excludes the development phone's personal modifications: 3× EXP, whole-party EXP, custom encounters, trade-evolution replacements, custom Pokédex areas, Charmander as a starter, a Mew egg, German localization and personal saves.

The port retains the platform work needed on this phone: ARM memory/alignment fixes, rendering and affine-transform fixes, audio support, storage, input, the pause/exit menu and startup handling. A platform repair makes the original game work correctly on new hardware; it does not change the game's rules. See [the configuration record](ports/emerald/VANILLA.json) and [license scope](licenses/README.md).

## Hardware and performance

| Item | Development handset |
| --- | --- |
| SoC / CPU | UMS9117 / ARM Cortex-A7; CPU ID `0x410fc075` |
| Advertised memory | 48 MB RAM, 128 MB ROM/flash |
| Ordinary app allocation | Approximately 3.5 MiB in one contiguous allocation; advertised RAM is not the app's heap budget |
| Display | 240 × 320 pixels in portrait; 320 × 240 when held 90° counterclockwise |
| Existing firmware regions | 16 MiB User and 14 MiB OS; this project does not require creating larger partitions |

The owner reports roughly **20–25 game frames/s in Emerald's field and around 30 at the title screen** in recent development builds. These are observations on one phone, not a benchmark for every scene or the clean public configuration. A 60 Hz display refresh rate does not mean 60 newly rendered game frames per second.

## Quick start for an existing compatible launcher

Install Python 3.10+ and LLVM, then run:

```powershell
python -m pip install -r requirements.txt
python tools/build_games.py
```

The three packages appear under `build/Games/`. Copy each `game.pga` into the corresponding folder under `Games/` on the microSD card. Keep existing `save0.dat` and `save1.dat`. The [guide](docs/guide.md) covers all six games, controls, the native engine toolchains and the exact boundary of firmware support.

## Source layout and licensing

| Directory | Purpose |
| --- | --- |
| `games/`, `sdk/` | Three small games and their native game API |
| `firmware/`, `experiments/launcher/` | Authored menu/launcher integration and native service work |
| `ports/`, `experiments/` | Port adapters, runtime source and platform changes |
| `tools/` | Offline builders |
| `docs/` | English build, installation and status documentation |
| `licenses/` | Upstream notices, exact revisions and license scope |

The project's original code is covered by [MIT](LICENSE). Third-party components have their own terms; see [licenses and provenance](licenses/README.md). The RP2350 platform license does not license Pokémon game code or assets. Firmware images, vendor loaders, ROMs, WADs, Nintendo assets, personal saves, device identifiers and device reports are not distributed here.
