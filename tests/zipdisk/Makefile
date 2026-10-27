# Regression tests for ZIP content. Run by hand:
#   make check                        (Linux, MSYS2 MINGW64)
#   make check SANITIZER=address,undefined
#   make check CC=x86_64-w64-mingw32-gcc EXE=.exe   (cross build)
#
# sysinfo_test loads the core built at the top of the tree (CORE_LIB), so
# build the core first.

CORE   := ../..
LRC    := $(CORE)/libretro-common
CC     ?= cc
EXE    ?=
TARGET := zipdisk_test$(EXE)
SYSINFO := sysinfo_test$(EXE)

ifeq ($(EXE),.exe)
CORE_LIB ?= $(CORE)/px68k_libretro.dll
DL_LIBS  :=
else
CORE_LIB ?= $(CORE)/px68k_libretro.so
DL_LIBS  := -ldl
endif

CFLAGS ?= -O1 -g
CFLAGS += -std=gnu89 -Wall -Wdeclaration-after-statement \
          -I$(CORE)/libretro -I$(CORE)/x68k -I$(LRC)/include
ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

SOURCES := zipdisk_test.c \
           $(CORE)/libretro/zipdisk.c \
           $(LRC)/compat/compat_strl.c \
           $(LRC)/encodings/encoding_crc32.c \
           $(LRC)/encodings/encoding_deflate.c \
           $(LRC)/features/features_cpu.c \
           $(LRC)/formats/zip/rzip_archive.c \
           $(LRC)/time/rtime.c

all: $(TARGET) $(SYSINFO)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

$(SYSINFO): sysinfo_test.c
	$(CC) $(CFLAGS) sysinfo_test.c -o $@ $(LDFLAGS) $(DL_LIBS)

check: $(TARGET) $(SYSINFO)
	rm -rf zipdisk_test_tmp && mkdir zipdisk_test_tmp
	./$(TARGET) zipdisk_test_tmp
	rm -rf zipdisk_test_tmp
	./$(SYSINFO) $(CORE_LIB)

clean:
	rm -rf $(TARGET) $(SYSINFO) zipdisk_test_tmp

.PHONY: all check clean
