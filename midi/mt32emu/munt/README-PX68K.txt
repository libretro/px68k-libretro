PX68K bundled Munt source
=========================

This directory contains the upstream Munt mt32emu library source, vendored
from https://github.com/munt/munt (repository snapshot dated 2026-11-08).
The mt32emu library version in this snapshot is 2.8.6.

PX68K compiles the required mt32emu C++ sources directly from Makefile.common.
It uses Munt's C-compatible API and built-in internal resampler. No Munt
submodule, separately installed library, CMake generation step, or optional
resampler library is required.

The PX68K integration is in midi/mt32emu/mt32emu_libretro.cpp; the core-facing interface
in midi/mt32emu/mt32emu_libretro.h is C-compatible. The PX68K core's C code remains C89.

Munt is licensed under LGPL-2.1-or-later. Preserve the upstream AUTHORS.txt,
COPYING.txt, COPYING.LESSER.txt, and copyright notices when redistributing.
