px68k-libretro 

forked from https://github.com/hissorii/px68k
backported c68k core from https://github.com/kenyahiro/px68k/ 
(fork of https://github.com/hissorii/px68k using recent c68k yabause core to support X64 build)

All credit goes to hissorii ( and kenyahiro for updating c68k core) .

How to use :

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

ひっそりぃ(hissorii)/sakahi
http://hissorii.blog45.fc2.com (ひっそりぃドットコム)
http://emuhani.seesaa.net      (えみゅはに - Emulator Hacking 日記)
GitHub: https://github.com/hissorii/px68k
iOS 版リポジトリ http://hissorii-com.appspot.com
Twitter: @hissorii_com


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
    └── sc55_waverom3.bin```

Important: The current PX68K version uses filename-based discovery. It doesn't yet have PrBoom's full SHA-256 ROM identification, so the expected filenames matter.
If you already have your ROMs installed for PrBoom, you shouldn't need to duplicate the files—just make sure they're named correctly and stored in a directory the core searches.
