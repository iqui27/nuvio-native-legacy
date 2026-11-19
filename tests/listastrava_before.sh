#!/bin/bash
# Expected nonzero: the pre-fix Library exceeds the 16 ms handler budget.
set -euo pipefail
cd "$(dirname "$0")/.."
BIB_REV=b8e9007bc bash tests/listastrava.sh
