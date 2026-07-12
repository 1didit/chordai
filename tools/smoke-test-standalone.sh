#!/usr/bin/env bash
# Standalone launch smoke test: proves the app launches from build artefacts,
# stays alive for a few seconds, shuts down cleanly, and leaves no crash report.
# This proves "launches and stays alive, no crash" only; the visual "window
# renders correctly" check happens once in the Plan 03 checkpoint.
# Run from repo root.
set -euo pipefail

APP=$(find build -type d -name 'ChordAI.app' -print -quit)
if [[ -z "$APP" ]]; then
    echo "ERROR: ChordAI.app not found under build/ — run cmake --build build first" >&2
    exit 1
fi

MARKER=$(mktemp)
touch "$MARKER"

BIN="$APP/Contents/MacOS/ChordAI"
"$BIN" &
PID=$!

sleep 5

if ! kill -0 "$PID" 2>/dev/null; then
    echo "FAIL: ChordAI process exited before the 5s liveness check"
    rm -f "$MARKER"
    exit 1
fi

kill "$PID"
wait "$PID" 2>/dev/null || true

CRASH_REPORTS=$(find "$HOME/Library/Logs/DiagnosticReports" -name 'ChordAI*' -newer "$MARKER" 2>/dev/null || true)
rm -f "$MARKER"

if [[ -n "$CRASH_REPORTS" ]]; then
    echo "FAIL: new crash report(s) found:"
    echo "$CRASH_REPORTS"
    exit 1
fi

echo "PASS: ChordAI Standalone launched, stayed alive 5s, shut down cleanly, no crash report"
exit 0
