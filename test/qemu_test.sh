#!/bin/bash
# KITE QEMU Test Script
# Copyright (C) 2026 Dere3046
# SPDX-License-Identifier: GPL-2.0

set -e

KITE_DIR="/home/Dere3046/code/KITE"
TEST_ENV="$KITE_DIR/archive/test_env"
KERNEL="$KITE_DIR/qemu-kernel/arm64/4.14/kernel-qemu2"
ROOTFS="$TEST_ENV/rootfs/debian-arm64-test.qcow2"
UEFI="$TEST_ENV/kernels/QEMU_EFI.fd"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

show_help() {
    cat << EOF
KITE QEMU Test

Usage: $0 {boot|test|shell|stop}

Commands:
    boot  - Boot kernel with QEMU (no KITE)
    test  - Boot kernel with KITE injected
    shell - SSH into running VM
    stop  - Stop QEMU VM
EOF
}

boot_kernel() {
    log_info "Booting kernel without KITE..."
    
    if [ ! -f "$KERNEL" ]; then
        log_error "Kernel not found: $KERNEL"
        exit 1
    fi
    
    if [ ! -f "$ROOTFS" ]; then
        log_warn "Rootfs not found, creating from base image..."
        cp "$TEST_ENV/rootfs/debian-arm64.qcow2" "$ROOTFS"
    fi
    
    QEMU_ARGS=(
        -machine virt,accel=tcg
        -cpu cortex-a72
        -smp 2
        -m 1024
        -kernel "$KERNEL"
        -drive file="$ROOTFS",format=qcow2,if=virtio
        -netdev user,id=net0,hostfwd=tcp::2222-:22
        -device virtio-net-pci,netdev=net0
        -serial stdio
        -display none
        -name "kite-test"
        -append "console=ttyAMA0 root=/dev/vda1 rw"
    )
    
    if [ -f "$UEFI" ]; then
        QEMU_ARGS+=(-bios "$UEFI")
    fi
    
    log_info "Starting QEMU..."
    qemu-system-aarch64 "${QEMU_ARGS[@]}"
}

patch_and_test_kite() {
    log_info "Testing KITE injection..."
    
    if [ ! -f "$KITE_DIR/KITE_DEV/build/bin/kite.bin" ]; then
        log_error "KITE binary not found. Build first with: make -C KITE_DEV"
        exit 1
    fi
    
    PATCHED_KERNEL="$KITE_DIR/KITE_DEV/patcher/patched_kernel_qemu"
    
    log_info "Patching kernel with KITE..."
    "$KITE_DIR/KITE_DEV/patcher/patcher" patch \
        "$KERNEL" \
        "$KITE_DIR/KITE_DEV/build/bin/kite.bin" \
        "$PATCHED_KERNEL" \
        "testkey"
    
    if [ ! -f "$PATCHED_KERNEL" ]; then
        log_error "Patch failed"
        exit 1
    fi
    
    log_info "KITE binary size: $(stat -c%s "$KITE_DIR/KITE_DEV/build/bin/kite.bin") bytes"
    log_info "Starting QEMU with KITE injected kernel..."
    
    if [ ! -f "$ROOTFS" ]; then
        log_warn "Rootfs not found, creating from base image..."
        cp "$TEST_ENV/rootfs/debian-arm64.qcow2" "$ROOTFS"
    fi
    
    QEMU_ARGS=(
        -machine virt,accel=tcg
        -cpu cortex-a72
        -smp 2
        -m 1024
        -kernel "$PATCHED_KERNEL"
        -drive file="$ROOTFS",format=qcow2,if=virtio
        -netdev user,id=net0,hostfwd=tcp::2222-:22
        -device virtio-net-pci,netdev=net0
        -serial stdio
        -display none
        -name "kite-test"
        -append "console=ttyAMA0 root=/dev/vda1 rw"
    )
    
    if [ -f "$UEFI" ]; then
        QEMU_ARGS+=(-bios "$UEFI")
    fi
    
    qemu-system-aarch64 "${QEMU_ARGS[@]}"
}

ssh_vm() {
    log_info "Connecting to VM via SSH..."
    ssh -p 2222 -i "$TEST_ENV/scripts/vm_key" -o StrictHostKeyChecking=no kite@localhost
}

stop_vm() {
    log_info "Stopping QEMU VM..."
    pkill -f "qemu-system-aarch64.*kite-test" || true
}

case "${1:-}" in
    boot)
        boot_kernel
        ;;
    test)
        patch_and_test_kite
        ;;
    shell)
        ssh_vm
        ;;
    stop)
        stop_vm
        ;;
    *)
        show_help
        exit 1
        ;;
esac
