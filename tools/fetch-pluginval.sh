#!/usr/bin/env bash
# Idempotent download of the prebuilt pluginval macOS binary (Tracktion/pluginval
# GitHub releases). The binary itself is NOT committed (tools/pluginval.app/ and
# tools/pluginval.zip are gitignored) — only this script is source-controlled.
# pluginval is GPLv3, but it runs out-of-process against the built plugin, so it
# does not taint the product license.
set -euo pipefail

cd "$(dirname "$0")"

BIN="pluginval.app/Contents/MacOS/pluginval"

if [[ -x "$BIN" ]]; then
    echo "pluginval already present"
    exit 0
fi

curl -L "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip" -o pluginval.zip
unzip -o pluginval.zip
rm pluginval.zip

if [[ ! -x "$BIN" ]]; then
    echo "ERROR: pluginval binary missing after unzip (expected at $BIN)" >&2
    exit 1
fi

echo "pluginval fetched successfully"
