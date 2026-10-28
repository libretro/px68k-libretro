LOCAL_PATH := $(call my-dir)

CORE_DIR := $(LOCAL_PATH)/../..
CPU_RUNTIME := 1
# Runtime mode builds the m68k C68K backend and Musashi together.
# Do not enable the legacy standalone C68K backend.
C68K := 0
USE_LIBRETRO_VFS := 1

include $(CORE_DIR)/Makefile.common

COREFLAGS := -DC68K_NO_JUMP_TABLE -DNO_MERCURY -D__LIBRETRO__ $(INCFLAGS) $(FLAGS)

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
  COREFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

include $(CLEAR_VARS)
LOCAL_MODULE    := retro
LOCAL_SRC_FILES := $(SOURCES_C) $(SOURCES_CXX) $(SOURCES_S)
LOCAL_CFLAGS    := $(COREFLAGS)
LOCAL_CXXFLAGS  := $(COREFLAGS)
LOCAL_LDFLAGS   := -Wl,-version-script=$(CORE_DIR)/link.T
include $(BUILD_SHARED_LIBRARY)
