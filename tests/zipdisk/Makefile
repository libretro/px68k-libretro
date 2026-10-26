# Regression test for libretro/zipdisk.c. Run by hand:
#   make check                        (Linux, MSYS2 MINGW64)
#   make check SANITIZER=address,undefined
#   make check CC=x86_64-w64-mingw32-gcc EXE=.exe   (cross build)

CORE   := ../..
LRC    := $(CORE)/libretro-common
CC     ?= cc
EXE    ?=
TARGET := zipdisk_test$(EXE)

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

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

check: $(TARGET)
	rm -rf zipdisk_test_tmp && mkdir zipdisk_test_tmp
	./$(TARGET) zipdisk_test_tmp
	rm -rf zipdisk_test_tmp

clean:
	rm -rf $(TARGET) zipdisk_test_tmp

.PHONY: all check clean
