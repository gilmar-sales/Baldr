#!/usr/bin/env bash
# scripts/install_afl.sh - Install afl++ from a pinned release tarball.
#
# Used by CI (.github/workflows/fuzz.yml) and by developers who want to
# run the fuzz lane locally without going through apt. Pins afl++ to a
# known-good commit so CI runs are reproducible.
#
# Usage:
#   scripts/install_afl.sh [prefix]
#
# The default prefix is "$HOME/.local/aflplusplus". The script puts
# afl-fuzz, afl-g++-fast and the gcc plugin into <prefix>/{bin,lib}.

set -euo pipefail

PREFIX="${1:-$HOME/.local/aflplusplus}"
AFL_VERSION="${AFL_VERSION:-4.21c}"

if [[ -x "$PREFIX/bin/afl-fuzz" ]]; then
    echo "install_afl: afl++ already installed at $PREFIX"
    exit 0
fi

TARBALL="${TMPDIR:-/tmp}/aflplusplus-${AFL_VERSION}.tar.gz"
SRC_DIR="${TMPDIR:-/tmp}/AFLplusplus-${AFL_VERSION}"

echo "install_afl: downloading afl++ ${AFL_VERSION}"
curl -fsSL \
    "https://github.com/AFLplusplus/AFLplusplus/archive/refs/tags/v${AFL_VERSION}.tar.gz" \
    -o "${TARBALL}"

echo "install_afl: extracting to ${SRC_DIR}"
rm -rf "${SRC_DIR}"
mkdir -p "${SRC_DIR}"
tar -xzf "${TARBALL}" -C "${TMPDIR:-/tmp}"

echo "install_afl: building (this takes a few minutes)"
pushd "${SRC_DIR}" >/dev/null
make -j"$(nproc)" >/dev/null
make install PREFIX="${PREFIX}" >/dev/null
popd >/dev/null

echo "install_afl: installed at ${PREFIX}"
echo "Add ${PREFIX}/bin to your PATH or re-run cmake with this prefix on PATH."