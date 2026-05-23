# KITE v6 Build System
# Copyright (C) 2026 Dere3046
# SPDX-License-Identifier: GPL-2.0

# kernel build config
KERNEL_DIR ?= ../kernel/android-kernel
CROSS_COMPILE ?= aarch64-linux-gnu-

# KITE config
KITE_DIR := /home/Dere3046/code/KITE/KITE_DEV
CORE_DIR := $(KITE_DIR)/core
ARCH_DIR := $(CORE_DIR)/arch/arm64

# output
BUILD_DIR := $(KITE_DIR)/build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin

# compiler
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
AS := $(CROSS_COMPILE)as
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

# flags
CFLAGS := -Wall -O2 -nostdlib -nostartfiles -ffreestanding -fno-builtin-memcpy \
    -I$(KITE_DIR)/include \
    -I$(CORE_DIR) \
    -D__KERNEL__ \
    -DKITE_VERSION=6

ifdef DEBUG
    CFLAGS += -DKITE_DEBUG -DMAP_DEBUG
endif

ASFLAGS := -I$(KITE_DIR)/include -I$(CORE_DIR) -nostdinc

ifdef DEBUG
    ASFLAGS += -DKITE_DEBUG -DMAP_DEBUG
endif

LDFLAGS := -T $(ARCH_DIR)/kite.lds -nostdlib

# source files
C_SRCS := $(wildcard $(CORE_DIR)/*.c)
C_SRCS += $(wildcard $(CORE_DIR)/*/*.c)
C_SRCS += $(wildcard $(ARCH_DIR)/*.c)
S_SRCS := $(wildcard $(ARCH_DIR)/*.S)

OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(C_SRCS)))
OBJS += $(patsubst %.S,$(OBJ_DIR)/%.o,$(notdir $(S_SRCS)))

# targets
.PHONY: all clean qemu test

all: $(BIN_DIR)/kite.bin

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR)/%.o: $(CORE_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(CORE_DIR)/%/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(ARCH_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/map_text.o: $(ARCH_DIR)/map_text.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -mgeneral-regs-only -c $< -o $@

$(OBJ_DIR)/tlsf.o: $(CORE_DIR)/memory/tlsf.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/symbol_engine.o: $(CORE_DIR)/symbol/symbol_engine.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(ARCH_DIR)/%.S | $(OBJ_DIR)
	$(CC) $(ASFLAGS) -x assembler-with-cpp -c $< -o $@

$(BIN_DIR)/kite.elf: $(OBJS) | $(BIN_DIR)
	$(LD) $(LDFLAGS) $^ -o $@

$(BIN_DIR)/kite.bin: $(BIN_DIR)/kite.elf
	$(OBJCOPY) -O binary $< $@

qemu: $(BIN_DIR)/kite.bin
	@echo "Run QEMU test..."
	qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic \
    -kernel $(KERNEL_DIR)/arch/arm64/boot/Image \
    -append "console=ttyAMA0"

test: qemu

clean:
	rm -rf $(BUILD_DIR)

# debug
info:
	@echo "C_SRCS: $(C_SRCS)"
	@echo "S_SRCS: $(S_SRCS)"
	@echo "OBJS: $(OBJS)"
