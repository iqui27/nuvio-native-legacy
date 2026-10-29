#!/usr/bin/env bash
set -euo pipefail
node tests/avroi-path.cjs
node tests/tizen-avplay-contract.cjs
