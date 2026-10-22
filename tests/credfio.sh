#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Wall -Wextra -Isrc -Wno-macro-redefined -Wno-deprecated-declarations)
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
cc ${flags[@]+"${flags[@]}"} src/credfio.c tests/credfio.c -lpthread -o /tmp/nuvio-credfio-tests
/tmp/nuvio-credfio-tests
