# Experimental ZIP floppy support

This patch adds read-only access to floppy images stored inside ZIP archives.

- Opening a ZIP from the PX68K internal menu's FDD0 or FDD1 file picker enters a virtual archive browser.
- The browser lists supported floppy images (`.DIM`, `.XDF`, `.HDM`, `.IMG`, `.2HD`, `.DUP`) and a `..` entry to return to the containing filesystem directory.
- Selecting a member mounts it in the selected floppy drive. When a member is selected from the FDD0 picker, the next supported image in that ZIP is also mounted in FDD1 when available. The FDD1 picker can still browse the same or a different ZIP independently.
- ZIP members are read into memory when mounted. ZIP-backed disks are read-only; ejecting or changing them never writes to the archive.
- RetroArch's existing Disk Control list remains supported for ZIPs loaded as content.
- ZIP method 0 (stored) and method 8 (deflate) are supported. Encrypted entries and ZIP64 archives are not supported by this experimental reader.
- This patch requires zlib (`-lz`).
- D88/88D members are intentionally not listed because the current D88 loader has not been adapted to memory-backed archive members.

The internal browser displays member paths as stored in the ZIP. Archive references are kept in a small in-memory table so that FDD0 and FDD1 can use different ZIP files without changing each other's mounted image source.
