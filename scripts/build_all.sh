#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 HALA Academy
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail
cd "$(dirname "$0")/.."
make clean
make all
make qualification
make reproducible
