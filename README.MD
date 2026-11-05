**PX68K-libretro**

forked from https://github.com/hissorii/px68k

backported c68k core from https://github.com/kenyahiro/px68k/  
  
(fork of https://github.com/hissorii/px68k using recent c68k yabause core to support X64 build)

All credit goes to hissorii ( and kenyahiro for updating c68k core) .

**Libretro features:**

- **On-screen X68000 keyboard:** Improved software keyboard usable with a gamepad or pointer/touch. It supports key navigation and presses, latched modifiers, and visual feedback. Options include keyboard themes, opacity, layout, cursor colors, and Japanese key labels.
- **Internal Roland SC-55 emulation:** Adds an internal SC-55 synthesizer option, with volume control and support for locating its ROM files in recognized system or content directories. MIDI output options also include LA, GM, GS, and XG modes.
- **More flexible controller input:** Adds Joy2Mouse support, including analog and digital movement speed and analog deadzone settings, plus configurable Joy2Key mappings and Retroarch remapping system.
- **Disk and archive conveniences:** Can remember floppy and hard-drive paths between sessions, swap the drive used for disk changes, and automatically insert floppy images from ZIP archives. Can directly explore a ZIP archive, select and load a disk without the need to extract. An option can also show MAME software-list names in the content browser.
- **Saving options:** Save state support. For content in archives changes are stored separately in RetroArch's save directory. For HDD images, delta saving stores only modified disk data instead of creating a complete writable copy, reducing disk space usage. Run Ahead support in 2nd instance mode.
- **Performance and display controls:** Adds CPU-emulation and CPU-speed choices, RAM-size configuration, frame skipping, frame-rate adjustment, and options to disable graphics or text layers.
- **Rendering fixes:** Various fixes concerning game emulation.


**How to use:**

First create a 'keropi' folder in retroarch system directory.
put the x68000 bios files in.

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

**Internal Roland SC-55 emulation guide:**  

PX68K can emulate the Roland SC-55 GS MIDI sound module.  

List of compatible games:  
https://www.vogonswiki.com/index.php/List_of_MT-32-compatible_computer_games#Sharp_X68000

```
Place these five files together in the same directory:
rom1.bin
rom2.bin
rom_sm.bin
waverom1.bin
waverom2.bin

Original SC-55 MKI — five ROM files
sc55_rom1.bin
sc55_rom2.bin
sc55_waverom1.bin
sc55_waverom2.bin
sc55_waverom3.bin

These filenames follow the conventions used by the Nuked-SC55 emulator.

The integration checks these locations:

*     RetroArch System Directory root
*     System/prboom/
*     Recognized subdirectories for SC-55 ROMs, such as System/sc55/ or System/roland/
*     The game/content directory

    For example, your System Directory could look like this:
    RetroArch/system/
    └── sc55/
    ├── sc55_rom1.bin
    ├── sc55_rom2.bin
    ├── sc55_waverom1.bin
    ├── sc55_waverom2.bin
    └── sc55_waverom3.bin
```

Important: The current PX68K version uses filename-based discovery. It doesn't yet have PrBoom's full SHA-256 ROM identification, so the expected filenames matter.
If you already have your ROMs installed for PrBoom, you shouldn't need to duplicate the files—just make sure they're named correctly and stored in a directory the core searches.
```
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
```
