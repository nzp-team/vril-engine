#!/bin/bash
set -euo pipefail

timeout_seconds="$1"
working_dir="$2"
emulator="$3"
log="$4"
test_name="$5"
failed='Test suite failed:|QCVM opcode test failed|Segmentation fault!|Host_Error|Sys_Error|invalid opcode found|PPC Interpreter: Invalid instruction'

cd "$working_dir"
ulimit -f 16384
: > "$log"
setsid env HOME="${working_dir}/home" timeout --kill-after=5 "$timeout_seconds" \
    stdbuf -oL "$emulator" > "$log" 2>&1 &
pid=$!

cleanup()
{
    kill -TERM -- "-${pid}" 2>/dev/null || true
    (sleep 5; kill -KILL -- "-${pid}" 2>/dev/null || true) &
    local killer=$!
    wait "$pid" 2>/dev/null || true
    kill "$killer" 2>/dev/null || true
    wait "$killer" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

started=$SECONDS
reported=$SECONDS
while kill -0 "$pid" 2>/dev/null; do
    if grep -qE "$failed" "$log"; then
        echo 'Xenon reported a test or emulation failure.'
        tail -n 40 "$log"
        exit 1
    fi
    if grep -qF "Test suite passed: ${test_name}" "$log"; then
        exit 0
    fi
    if (( SECONDS - reported >= 30 )); then
        echo "Waiting for Xenon ${test_name}: $((SECONDS - started)) seconds elapsed."
        tail -n 5 "$log"
        reported=$SECONDS
    fi
    sleep 0.2
done

status=0
wait "$pid" || status=$?
if ! grep -qE "$failed" "$log" && grep -qF "Test suite passed: ${test_name}" "$log"; then
    exit 0
fi
echo "Xenon exited with status ${status} before the test completed."
tail -n 40 "$log"
exit 1
