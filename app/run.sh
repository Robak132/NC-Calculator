#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN=""

for candidate in \
  "${SCRIPT_DIR}/fission-app" \
  "${SCRIPT_DIR}/Debug/fission-app" \
  "${SCRIPT_DIR}/Release/fission-app" \
  "${SCRIPT_DIR}/RelWithDebInfo/fission-app" \
  "${SCRIPT_DIR}/MinSizeRel/fission-app"; do
  if [[ -x "${candidate}" ]]; then
    BIN="${candidate}"
    break
  fi
done

if [[ -z "${BIN}" ]]; then
  echo "Missing fission-app executable in ${SCRIPT_DIR}"
  echo "Build first, for example:"
  echo "  cmake -S . -B cmake-build-cmd"
  echo "  cmake --build cmake-build-cmd --target FissionCmd -j"
  exit 1
fi

"${BIN}" "$@"
