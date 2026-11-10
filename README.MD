**PX68K-libretro**

forked from https://github.com/hissorii/px68k

backported c68k core from https://github.com/kenyahiro/px68k/

(fork of https://github.com/hissorii/px68k using recent c68k yabause core to support X64 build)

All credit goes to hissorii ( and kenyahiro for updating c68k core) .

**Libretro features:**

- **On-screen X68000 keyboard:** Improved software keyboard usable with a gamepad or pointer/touch. It supports key navigation and presses, latched modifiers, and visual feedback. Options include keyboard themes, opacity, layout, cursor colors, and Japanese key labels.
- **Internal MIDI synthesizers:** Adds selectable Roland SC-55, MT-32, CM-32L and CM-64 sound-module emulation, with a shared volume control and automatic ROM discovery. The MT-32 family is powered by Munt. MIDI output options also include LA, GM, GS, and XG modes.
- **More flexible controller input:** Adds Joy2Mouse support, including analog and digital movement speed and analog deadzone settings, plus configurable Joy2Key mappings and Retroarch remapping system.
- **Disk and archive conveniences:** Can remember floppy and hard-drive paths between sessions, swap the drive used for disk changes, and automatically insert floppy images from ZIP archives. Can directly explore a ZIP archive, select and load a disk without the need to extract. An option can also show MAME software-list names in the content browser.
- **Saving options:** Save state support. For content in archives changes are stored separately in RetroArch's save directory. Delta saving stores only modified disk data instead of creating a complete writable copy, reducing disk space usage. Run Ahead support in 2nd instance mode.
- **Performance and display controls:** Adds CPU-emulation and CPU-speed choices, RAM-size configuration, frame skipping, frame-rate adjustment, and options to disable graphics or text layers.
- **Rendering fixes:** Various fixes concerning game emulation.


**How to use:**

First create a 'keropi' folder in retroarch system directory, put the x68000 bios files in.
(iplrom.dat / cgrom.dat / iplrom30.dat)

Then launch emulator with some content (dim|zip|img|d88|88d|hdm|dup|2hd|xdf|hdf)
12 or joypad L2 to enter menu.
Touroku key via ScrollLock key or Joypad R2 to enable MIDI music when preseed in Wolfteam games.


You can launch content with:

- retroarch -L px68k_libretro.so ./content.hdf

- retroarch -L px68k_libretro.so ./content.xdf

- retroarch -L px68k_libretro.so ./content.cmd
 (cmdfile is a text file contening cmd like "px68k /somewhere/software/x68000/content1.dim /somewhere/software/x68000/content2.dim")

- retroarch -L sdlpx68k_libretro.so "px68k /somewhere/software/x68000/content1.dim /somewhere/software/x68000/content2.dim"

- load retroarch , then load core and content from RA menu.

**M3U and paths support ZIP content exploration:**

Example in system/keropi/config:
```
[WinX68k]
FDD0=D:\Games\disk2.zip#disk1.dim
FDD1=D:\Games\disk2.zip#disk2.dim
```
Example in a .m3u:
```
disk1.zip#disk1.dim
disk2.zip#disk2.dim
```

**Internal MIDI emulation guide:**

Set `Internal MIDI Synthesizer` to the desired module. `Disabled` keeps the normal RetroArch MIDI output path. The shared `MIDI Emulation Volume` option controls the internal synthesizer only.

**Roland SC-55:** PX68K uses the existing Nuked-SC55 emulation.

Original SC-55mkII — five ROM files (place together):
```text
rom1.bin
rom2.bin
rom_sm.bin
waverom1.bin
waverom2.bin
```

Original SC-55 MKI — five ROM files (place together):
```text
sc55_rom1.bin
sc55_rom2.bin
sc55_waverom1.bin
sc55_waverom2.bin
sc55_waverom3.bin
```

The SC-55 integration checks the RetroArch System Directory root, `System/prboom/`, recognized subdirectories such as `System/sc55/` and `System/roland/`, and the game/content directory.

**Roland MT-32 / CM-32L / CM-64:** These options use Munt's `mt32emu` library. Put the appropriate ROM files together in the RetroArch System Directory or one of the recognized Munt subdirectories:

| Module | Control ROM | PCM ROM |
|---|---|---|
| MT-32 | `MT32_CONTROL.ROM` | `MT32_PCM.ROM` |
| CM-32L | `CM32L_CONTROL.ROM` | `CM32L_PCM.ROM` |
| CM-64 | `CM32L_CONTROL.ROM` | `CM32L_PCM.ROM` |

Munt identifies ROMs by their contents. The core tries the conventional filenames in the System Directory root, `mt32/`, `cm32l/`, `cm64/`, `roland/`, `munt/`, `prboom/`, and the game/content directory (including `content/mt32/`, `content/cm32l/`, `content/cm64/` and `content/roland/`). ROM search results and initialization status are sent to RetroArch's core log.

List of MT-32-compatible games: https://www.vogonswiki.com/index.php/List_of_MT-32-compatible_computer_games#Sharp_X68000

**CM-64 limitation:** Munt emulates the CM-64's CM-32L LA sound module. It does not emulate the separate CM-32P PCM expander, so this option is not a complete two-module CM-64 emulation.

Munt's `mt32emu` library is vendored under `midi/mt32emu/munt/mt32emu` and compiled directly into the core. No submodule, separately installed library, CMake step, or additional build dependency is required. The library uses its built-in resampler. The bundled source is LGPL-2.1-or-later; retain the upstream `COPYING.txt`, `COPYING.LESSER.txt`, and authorship notices when redistributing the core.

The SC-55 integration uses filename-based discovery and retains the existing Nuked-SC55 ROM naming conventions above.

PX68K:
ひっそりぃ(hissorii)/sakahi
http://hissorii.blog45.fc2.com (ひっそりぃドットコム)
http://emuhani.seesaa.net      (えみゅはに - Emulator Hacking 日記)
GitHub: https://github.com/hissorii/px68k
iOS 版リポジトリ http://hissorii-com.appspot.com
Twitter: @hissorii_com

SC-55 emulation:
Copyright (C) 2021, 2024 nukeykt
Copyright (C) 2024-2026 J.C. Moyer

MT-32 / CM-32L emulation:
Munt project — LGPL-2.1-or-later (https://github.com/munt/munt)
```
