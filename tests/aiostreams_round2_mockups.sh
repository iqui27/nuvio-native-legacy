#!/bin/bash
# Focused approval artifact check; no owner-service requests.
set -euo pipefail
cd "$(dirname "$0")/.."
git diff --check -- docs/reviews/aiostreams tests/aiostreams_round2_mockups.sh
node docs/reviews/aiostreams/mockups/render.mjs
