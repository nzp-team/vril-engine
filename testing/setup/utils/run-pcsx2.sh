#!/bin/bash
set -o errexit

timeout_seconds="$1"
working_dir="$2"
emulator="$3"
image="${working_dir}/usb.img"
game_directory="${working_dir}/usb/APPS/NZPORTABLE"
console_log="${game_directory}/nzp/condebug.log"
rm -f "${working_dir}/pcsx2.pid" "${game_directory}/crash.log"

mdel -i "${image}" ::/APPS/NZPORTABLE/nzp/condebug.log ::/APPS/NZPORTABLE/capture.bmp ::/APPS/NZPORTABLE/crash.log 2>/dev/null || true
setsid env LD_PRELOAD="$(dirname "${emulator}")/usb-sync.so" XDG_CONFIG_HOME="${working_dir}" SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
	xvfb-run -a \
	bash -c 'echo "$$" > "$1"; shift; exec timeout --foreground --kill-after=5 "$@"' \
	sh "${working_dir}/pcsx2.pid" "${timeout_seconds}" "${emulator}" -nogui -batch -elf "${working_dir}/nzportable/nzportable.elf" &
emulator_pid=$!

function cleanup()
{
	if [[ -f "${working_dir}/pcsx2.pid" ]]; then
		kill -TERM "$(cat "${working_dir}/pcsx2.pid")" 2>/dev/null || true
	fi
	(sleep 5; kill -KILL -- "-${emulator_pid}" 2>/dev/null || true) &
	local killer_pid=$!
	wait "${emulator_pid}" 2>/dev/null || true
	kill "${killer_pid}" 2>/dev/null || true
	wait "${killer_pid}" 2>/dev/null || true
	mcopy -o -i "${image}" ::/APPS/NZPORTABLE/capture.bmp "${game_directory}/capture.bmp" 2>/dev/null || true
	mcopy -o -i "${image}" ::/APPS/NZPORTABLE/nzp/condebug.log "${console_log}" 2>/dev/null || true
	mcopy -o -i "${image}" ::/APPS/NZPORTABLE/crash.log "${game_directory}/crash.log" 2>/dev/null || true
	if [[ -s "${game_directory}/crash.log" ]]; then
		cat "${game_directory}/crash.log" >&2
	fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

while kill -0 "${emulator_pid}" 2>/dev/null; do
	mtype -i "${image}" ::/APPS/NZPORTABLE/nzp/condebug.log > "${console_log}.tmp" 2>/dev/null || true
	if [[ -s "${console_log}.tmp" ]]; then
		cat "${console_log}.tmp" > "${console_log}"
		if grep -qE 'Host_Error|Sys_Error|Test suite failed:|GPU timeout:' "${console_log}"; then
			exit 1
		fi
		if grep -q '^Test suite passed: ' "${console_log}"; then
			exit 0
		fi
	fi
	if mdir -i "${image}" ::/APPS/NZPORTABLE/crash.log > /dev/null 2>&1; then
		exit 1
	fi
	sleep 1
done
wait "${emulator_pid}"
grep -q '^Test suite passed: ' "${console_log}"
