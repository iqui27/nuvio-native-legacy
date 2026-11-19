#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -I. -Isrc -DNV_CEL_TEST_LOOPBACK -D_GNU_SOURCE -pthread tests/celular_lan_fixo.c -o /tmp/celular_lan_fixo
/tmp/celular_lan_fixo
