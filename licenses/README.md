# Licenses and provenance

The root [MIT license](../LICENSE) covers original project code and documentation. It is not a blanket license for the external game engines, game content or firmware. Keep upstream notices with the components they cover.

## Source inventory

| Component | Revision / origin | Scope and notice |
| --- | --- | --- |
| Small games, SDK, authored launcher/adapter work | This repository | MIT, except where a file derives from a separately licensed upstream component |
| RP2350 Emerald platform | [`3c35e5b957dff6c578878031e0c4a7c8d7691401`](https://github.com/mattdeeds/pokeemerald-rp2350/tree/3c35e5b957dff6c578878031e0c4a7c8d7691401) | Matt Deeds' platform work is MIT; see [the scoped notice](emerald-platform-MIT.txt) |
| DoomGeneric | [`e257a09527062bfb986b6e5e0780839b6cbd5b71`](https://github.com/ozkl/doomgeneric/tree/e257a09527062bfb986b6e5e0780839b6cbd5b71) | GNU GPL version 2; [upstream license copy](DoomGeneric-GPL-2.0.txt) |
| Licar, including its data and music | [`d31205dde1ca79ce9314fe6b18cafeca1fa6f3a7`](https://git.coom.tech/drummyfish/Licar/src/commit/d31205dde1ca79ce9314fe6b18cafeca1fa6f3a7) | CC0 1.0; [upstream license copy](Licar-CC0-1.0.txt); [author's project page](https://drummyfish.itch.io/licar) |
| FPDoom UMS9117 driver work | [`296795c2cfa084b061a7529997a19e75f4f91e36`](https://github.com/ilyakurdyukov/fpdoom/tree/296795c2cfa084b061a7529997a19e75f4f91e36) | Unlicense for the relevant driver work; [license copy](FPDoom-Unlicense.txt) |
| Freedoom Phase 1 data, supplied separately | [0.13.0](https://github.com/freedoom/freedoom/releases/tag/v0.13.0) | BSD 3-Clause; [license copy](Freedoom-BSD-3-Clause.txt); preserve the release's credits with its WAD |

The FPDoom entry concerns the chipset/driver code used as a reference or adapted into the runtime. It does not relicense the separate games included elsewhere in FPDoom's repository. The PEAQ port adds board-specific display, keypad, SD, audio and startup work; upstream UMS9117 support alone does not qualify this phone's complete configuration.

## Emerald's license boundary

The upstream [RP2350 license](https://github.com/mattdeeds/pokeemerald-rp2350/blob/3c35e5b957dff6c578878031e0c4a7c8d7691401/rp2350/LICENSE) explicitly limits its MIT grant to original platform code in `rp2350/` and its host sources. It does **not** grant a license for the surrounding `pret/pokeemerald` decompilation, Nintendo game assets or all of its build tools. Individual upstream build tools retain their own notices.

This public tree provides platform/runtime work and edit records for a separately supplied local input. The edits are not a license grant for the files to which they apply. No Pokémon ROM, graphics, music, map data, personal save or full game binary is included. The public build configuration preserves original gameplay; the excluded personal modifications are listed in [VANILLA.json](../ports/emerald/VANILLA.json).

## External build dependencies

The Windows Emerald helper pins zlib to `767c4c947852e143f582c85f14cf573411df1b35` and libpng to `0b0876829d77db0827a8cc2c4326f63be22b2aa1`. The verified asset pipeline used GNU Make 4.4.90 at `b3802782de3eff2c0f1eda9e7c0befd8cd142162` from `https://git.savannah.gnu.org/git/make.git` (GPL version 3 or later). Their sources and notices belong in the locally supplied dependency checkouts. They are build dependencies, not bundled phone firmware. LLVM, Arm GNU/Newlib, Python and Python packages also retain their respective upstream terms.

The DOOM engine link uses Newlib, libm and compiler support routines supplied by the selected Arm GNU toolchain. If distributing compiled output, include the applicable toolchain-library notices as well as the DoomGeneric terms and the complete corresponding source required for the GPL-covered binary. This source publication does not by itself constitute a complete binary distribution bundle.

## Public/private separation

Publication starts from a curated source tree with fresh history. Full flash images, signed/vendor loaders, modem calibration, device identifiers, USB captures, raw device reports and personal saves are outside the publication scope. Generated engines and external assets stay under ignored local build/dependency directories.

The SD CID/capacity profile is an explicit user-supplied file under ignored `local/`. Runtime source headers are deliberately unbuildable placeholders until the builder injects that local profile into ignored output. CID and capacity checks remain active. Firmware outputs incorporate the user's original vendor images and are not redistribution artifacts.

`Games/COLDCHK.BIN` is a deterministic runtime test pattern, not captured user data. Its size, formula and fingerprint are documented in the [installation guide](../docs/guide.md#required-sd-reference-and-save-files). It is required by the Emerald startup path even though it has no visible game entry.

PEAQ, Pokémon, Nintendo and DOOM names identify the relevant hardware or software. This project is independent and does not claim endorsement by their owners.
