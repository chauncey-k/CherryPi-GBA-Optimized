ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
SOURCE ?= $(ROOT)/gpsp
HOST := $(ROOT)/build/rootfs/host
CC := $(HOST)/bin/arm-linux-gcc
SYSROOT := $(shell $(CC) --print-sysroot)
export PKG_CONFIG_SYSROOT_DIR := $(SYSROOT)
export PKG_CONFIG_LIBDIR := $(SYSROOT)/usr/lib/pkgconfig
PKG := PKG_CONFIG_SYSROOT_DIR=$(SYSROOT) PKG_CONFIG_LIBDIR=$(SYSROOT)/usr/lib/pkgconfig $(HOST)/bin/pkg-config
VPATH := $(SOURCE):$(SOURCE)/arm:$(SOURCE)/cherrypi
CFLAGS := -O2 -MMD -MP -fcommon -fno-strict-aliasing -fno-pic -march=armv5te -mtune=arm926ej-s -mfloat-abi=soft -DARM_ARCH -DMIYOO -DCHERRYPI -I$(SOURCE) $(shell $(PKG) --cflags sdl libpng zlib)
LDLIBS := $(shell $(PKG) --libs sdl libpng zlib) -lm -Wl,-rpath,'$$ORIGIN/lib'
OBJS := main.o cpu.o memory.o video.o input.o sound.o gui.o cheats.o zip.o arm_stub.o warm.o cpu_threaded.o video_blend.o sha1.o imageio.o platform.o settings.o state_io.o quick_menu.o profile.o
all: gpsp
gpsp: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDLIBS) -no-pie -o $@
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)
