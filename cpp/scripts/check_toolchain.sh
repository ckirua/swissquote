#!/usr/bin/env bash
# Verify toolchain meets minimum requirements for the C++ engine.
set -eu

MIN_GCC_MAJOR=13
MIN_CLANG_MAJOR=17

ok=0

if command -v g++ >/dev/null 2>&1; then
  ver="$(g++ -dumpversion | cut -d. -f1)"
  if [[ "${ver}" -ge "${MIN_GCC_MAJOR}" ]]; then
    echo "ok  g++ ${ver} (>= ${MIN_GCC_MAJOR})"
    ok=1
  else
    echo "warn g++ ${ver} < ${MIN_GCC_MAJOR}"
  fi
fi

if command -v clang++ >/dev/null 2>&1; then
  ver="$(clang++ -dumpversion | cut -d. -f1)"
  if [[ "${ver}" -ge "${MIN_CLANG_MAJOR}" ]]; then
    echo "ok  clang++ ${ver} (>= ${MIN_CLANG_MAJOR})"
    ok=1
  else
    echo "warn clang++ ${ver} < ${MIN_CLANG_MAJOR}"
  fi
fi

if [[ "${ok}" -eq 0 ]]; then
  echo "ERROR: need GCC >= ${MIN_GCC_MAJOR} or Clang >= ${MIN_CLANG_MAJOR}" >&2
  exit 1
fi

if ! pkg-config --exists libzmq; then
  echo "ERROR: libzmq not found (install libzmq3-dev)" >&2
  exit 1
fi
echo "ok  libzmq"

if ! command -v cmake >/dev/null 2>&1; then
  echo "ERROR: cmake not found" >&2
  exit 1
fi
echo "ok  cmake $(cmake --version | head -1 | awk '{print $3}')"
