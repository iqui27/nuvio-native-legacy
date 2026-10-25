#!/bin/bash
# One offline target gate for J4's four native renderer harnesses.
set -eu
cd "$(dirname "$0")/.."
export J4_PROOF_DIR=/Volumes/ExternalSSD/tmp/rev/diretor21/21-j4
mkdir -p "$J4_PROOF_DIR"
bash tests/conquistas_shot.sh "$J4_PROOF_DIR/achievements"
bash tests/filmografia_shot.sh "$J4_PROOF_DIR/filmography"
bash tests/pessoas_shot.sh "$J4_PROOF_DIR"
bash tests/social_tela_shot.sh "$J4_PROOF_DIR/social"
python3 tests/j4_proof.py
