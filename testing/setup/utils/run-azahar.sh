#!/bin/bash
# Run Azahar until the engine reports a completed test suite.
set -o errexit

timeout_seconds="$1"
console_log="$2"
game_directory="$3"
emulator="$4"
application="$5"

# We need to do a nasty bash hack to hold the PID and kill the emulator after killing the game, because it lingers.
setsid sh -c 'cd "$1" && exec xvfb-run -a timeout --foreground --kill-after=5 "$2" "$3" "$4"' \
	sh "${game_directory}" "${timeout_seconds}" "${emulator}" "${application}" &
emulator_pid=$!

function cleanup()
{
	kill -TERM -- "-${emulator_pid}" 2>/dev/null || true
	wait "${emulator_pid}" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

while kill -0 "${emulator_pid}" 2>/dev/null; do
	if [[ -f "${console_log}" ]]; then
		if grep -qE 'Host_Error|Sys_Error|File a ticket:|Test suite failed:' "${console_log}"; then
			exit 1
		fi
		if grep -q '^Test suite passed: ' "${console_log}"; then
			exit 0
		fi
	fi
	sleep 1
done

# A clean GUI exit alone does not mean the engine completed its test.
wait "${emulator_pid}"
grep -q '^Test suite passed: ' "${console_log}"
