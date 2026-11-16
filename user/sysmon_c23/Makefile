# Makefile for Project 1 — sysmon_c23 (Host & ARM Cross-Compilation)
# Compatible with GCC 14+ (-std=c23) and automatic fallback to -std=c2x on GCC 12/13

CROSS_COMPILE ?=
CC            := $(CROSS_COMPILE)gcc
STRIP         := $(CROSS_COMPILE)strip

# Automatic feature test for -std=c23 support with fallback to -std=c2x
CSTD_FLAG     := $(shell $(CC) -std=c23 -x c -E /dev/null >/dev/null 2>&1 && echo "-std=c23" || echo "-std=c2x")

CFLAGS        := $(CSTD_FLAG) -O2 -Wall -Wextra -Werror -pedantic \
                 -Wformat=2 -Wconversion -Wsign-conversion \
                 -Wnull-dereference -Wshadow -Wstrict-prototypes \
                 -fstack-protector-strong -D_FORTIFY_SOURCE=2
LDFLAGS       :=

# Build static binary when targeting minimal initramfs rootfs without shared libc.so
ifeq ($(STATIC),1)
    LDFLAGS += -static
endif

TARGET        := sysmon_c23
SRCS          := sysmon_c23.c
OBJS          := $(SRCS:.c=.o)

.PHONY: all clean test sanitize install-rootfs

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c sysmon.h c23_compat.h dbg.h
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	./$(TARGET) 3 200

sanitize: clean
	$(CC) $(CSTD_FLAG) -O1 -g3 -Wall -Wextra -Werror -pedantic \
	      -fsanitize=address,undefined -fno-omit-frame-pointer \
	      $(SRCS) -o $(TARGET)_san
	./$(TARGET)_san 2 100

install-rootfs: clean
	$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) STATIC=1 all
	$(STRIP) $(TARGET)
	install -m 0755 $(TARGET) $(PROJ_ROOT)/rootfs/usr/bin/$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET)_san $(OBJS)
