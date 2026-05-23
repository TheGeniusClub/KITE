#!/bin/bash
# KITE v6 Test Suite Runner
# Copyright (C) 2026 Dere3046
# SPDX-License-Identifier: GPL-2.0

set -e

echo "=== KITE v6 Test Suite ==="
cd "$(dirname "$0")"
make clean
make run
echo ""
echo "All tests passed!"
