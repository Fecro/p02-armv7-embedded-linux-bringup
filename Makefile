# =============================================================================
# Master Orchestration Makefile - Project 01: ARMv7 Embedded Linux Bring-Up
# Usage:
#   make demo    -> Full automated build (Deps, U-Boot, Kernel, BusyBox, C23 App,
#                   MBR SD Card Image) + Launch QEMU (U-Boot -> Linux -> Shell)
#   make direct  -> Build all + Launch QEMU Direct Kernel Boot (4-core SMP)
#   make build   -> Run all build & packaging stages without launching QEMU
#   make clean   -> Remove compiled app binaries and generated images
#   make distclean -> Remove all downloaded upstream sources and build trees
# =============================================================================

SHELL := /usr/bin/env bash

.PHONY: all demo direct build deps uboot kernel busybox sysmon images run run-direct clean distclean

all: build

demo: deps build run

direct: deps build run-direct

build: uboot kernel busybox sysmon images

deps:
	./scripts/01_install_deps.sh

uboot:
	./scripts/02_build_uboot.sh

kernel:
	./scripts/03_build_kernel.sh

busybox:
	./scripts/04_build_busybox.sh

sysmon:
	./scripts/05_build_sysmon.sh

images:
	./scripts/06_package_images.sh

run:
	./scripts/07_run_qemu.sh uboot

run-direct:
	./scripts/07_run_qemu.sh direct

clean:
	$(MAKE) -C user/sysmon_c23 clean
	rm -rf images/

distclean: clean
	rm -rf sources/ Bootloader/ kernel/ rootfs/
