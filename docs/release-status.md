# Public release status

Status recorded on 2026-10-29. This page distinguishes three things: source available here, output reproducible from that source, and an integration exercised on the development handset.

## Rebuilt native packages

The public builders compiled Licar and DoomGeneric from their pinned upstream checkouts using LLVM 22.1.6 and Arm GNU Toolchain 14.2.Rel1. The engine packers checked ARM ELF/PIE structure, resolved symbols, bounded relative relocations and export tables. The PGA packers checked package sizes, entry points, ABI 5, descriptor revision 2, segment requests and checksums.

The following rebuilt files match their qualified development baselines byte for byte:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| DOOM `game.pga` | 19,460 | `50a820bec8aba2eb6ac90934e35eb7b6d582aa373f9265bfc79346305cfaebfe` |
| DOOM `engine.pge` | 399,136 | `280c057a38d74e315408c9ebbee5dfb1bb6bc6717a24d6a8c9b4dabee2e2012a` |
| Racer `game.pga` | 17,768 | `dcc704853baac6cf7926bd85d612c415b7c538fad3446cfca0819e0e0408c9a9` |
| Racer `engine.pge` | 204,124 | `4a224904e8e37eaac116d7e0e27eb646ba6cfe21aa07096cf4aefd96564f89ea` |

Both adapters use a 5 ms service tick. This is a scheduling interval, not a 200 FPS claim. DOOM and Racer were reported playable on the development phone, and repeated DOOM-to-Racer transitions worked after the launcher cleanup repair. No numerical DOOM/Racer FPS benchmark is claimed. Their native audio/save integration remains incomplete.

The new public build was offline and made no phone or firmware changes. Older component reports may contain `phone_ready: false` or `hardware_tested: false`: those describe what that component build itself did, and do not negate the separately reported handset result. Conversely, matching binaries does not qualify a different firmware or handset.

## Small games

Tic Tac Toe, Tetris and Snake have included source and the direct `tools/build_games.py` package recipe. They require ABI 3 or later. The development handset has exercised their menus, controls and persistent records/progress. Current in-game text is German. The SDK package generation does not install firmware.

## Emerald

The development integration includes a dedicated startup path, display/input/audio/storage support, original-game saving and a confirmed exit back to the OS. Reported recent performance is about 20–25 FPS in the field and 30 at the title screen, depending on the scene. These observations come from development builds on one handset, not a new benchmark of the public vanilla configuration.

The public variant removes personal gameplay changes and German localization while retaining platform fixes. The exported source includes rendering/affine work, transitions, sound integration and the battle-party-order pointer repair. The latest BDPCM/reverse-cry decoder and water-reflection correction are included. Their presence in this build is distinct from a fresh handset confirmation.

The public pipeline now completes these stages:

1. Prepare a clean checkout of the exact upstream revision, verify and apply 15 platform edit records, and generate a checked preparation manifest. Personal working-tree changes are not copied.
2. Build the Windows host tools, generate 518 maps and 11,551 asset files, compile 314 C sources and produce an 853-member relocatable ARM archive.
3. Link and pack the native engine, then compile all 32 standalone runtime translation units with regenerated engine/header/CRC/export/allocation bindings. Runtime card values come only from the required local SD profile.
4. Generate the English launcher and fresh/update SD layouts. Fresh output has blank game saves and preferences; updates omit the real save pair and preferences. Both include the required `COLDCHK.BIN`.

| Public Emerald output | Bytes | SHA-256 |
| --- | ---: | --- |
| ARM archive | 25,499,642 | `6d6d5d9f63f5dd9edf4faa927dff93b75ea1941c1120b3ab0478ebcddd344b35` |
| `ADAPT.PGE` engine | 12,830,536 | `8c3eec4174b90b0850333df25d3bbe643745d85945ef92cfef32a98277d415bd` |

The engine has 82,235 relative relocations, including 23,679 unaligned pointer words, and uses 13,205,196 bytes of module memory. Link/package checks found no unresolved symbols. The runtime is 61,440 bytes and passes its linked memory-layout bounds; its checksum and the launcher's checksum depend on the supplied SD profile. The offline runtime/package build used a synthetic card identity, not a development-card default.

The Unicorn 2.1.3 test ran **600 frames** through copyright, intro and title screen: 345 distinct frame hashes, 134,400 audio samples with 107 distinct sample values, no invalid memory access and an intact allocation guard. It also checked erase/program semantics at save sectors 0, 15, 16 and 31, and rejection outside the 128 KiB save mirror. Relocation in this test uses a Python reference loader; it does not exercise the production ARM loader, SD controller or firmware boot gate.

Remaining public Emerald qualification is concrete: run this exact clean engine/runtime on a compatible handset; confirm boot, physical display/input/audio, new-game behavior, actual SD save/readback, pause and return to the OS. The host test is not a full playthrough or an on-card save test. A prior personal build does not establish these results for the newly assembled vanilla package.

## Firmware installation for another owner

The public tree is a developer/source preview, not a complete install distribution for a stock phone. `tools/build_known_firmware.py` now builds the authored User launcher/FAT repair and the OS boot gate from exact matching original images. Original-image size and SHA-256 gates reject other inputs. With the development installation's local inputs, both patched images reproduced its qualified baseline byte for byte. The builder records the SD-profile fingerprint and performs no hardware access or flash operation.

Still missing is a qualified public backup/recovery/download/writer workflow. It must cover suitable locally supplied loaders, device geometry and bad-block mapping, repeated original reads, a no-write rehearsal, tested original restore, bounded writes, full readback comparison and validation of the resulting launcher/boot gate. The historical physical bad-block allowlist and transport identities were specific to the development installation; they are not defaults for another device. A filename or product label cannot replace these inputs. The existing User/OS partition sizes are not a proposal to repartition.

The [guide](guide.md) includes all six local package builds, the offline firmware command and the shared SD-profile requirement. There is no public universal-flasher command to run at this stage.
