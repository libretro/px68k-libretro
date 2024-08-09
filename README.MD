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
