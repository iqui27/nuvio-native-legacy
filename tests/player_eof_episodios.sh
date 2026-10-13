#!/bin/bash
set -euo pipefail
exec bash "$(dirname "$0")/player_visto_trailer.sh" tests/player_eof_episodios.c
