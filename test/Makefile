# KITE v6 Test Suite
# Copyright (C) 2026 Dere3046
# SPDX-License-Identifier: GPL-2.0

KITE_DIR := ..
CORE_DIR := $(KITE_DIR)/core

CC := gcc
CFLAGS := -Wall -O2 -I$(KITE_DIR)/include -I$(CORE_DIR) -Iinclude

TESTS := l1_unit/test_stubs \
         l1_unit/test_tlsf \
         l1_unit/test_memory

TEST_BINS := $(addprefix build/,$(TESTS))

.PHONY: all clean run

all: $(TEST_BINS)

build/l1_unit/test_stubs: l1_unit/test_stubs.c $(CORE_DIR)/stubs.c | build/l1_unit
	$(CC) $(CFLAGS) $^ -o $@

build/l1_unit/test_tlsf: l1_unit/test_tlsf.c $(CORE_DIR)/memory/tlsf.c | build/l1_unit
	$(CC) $(CFLAGS) $^ -o $@

build/l1_unit/test_memory: l1_unit/test_memory.c $(CORE_DIR)/memory/memory.c $(CORE_DIR)/memory/tlsf.c | build/l1_unit
	$(CC) $(CFLAGS) -D__TEST_HOST__ $^ -o $@

build/l1_unit:
	mkdir -p $@

run: all
	@echo "=== KITE Test Suite ==="
	@for t in $(TEST_BINS); do \
		echo ""; \
		echo "Running $$t..."; \
		$$t || exit 1; \
	done
	@echo ""
	@echo "All tests passed!"

clean:
	rm -rf build
