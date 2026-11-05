#!/bin/bash
set -euo pipefail

timeout_seconds="$1"
working_dir="$2"
emulator="$3"

# Keep the display server alive until capture and process cleanup have finished.
if [[ "${4:-}" != --display ]]; then
    exec timeout --kill-after=10 "$((timeout_seconds + 90))" \
        xvfb-run -a -e "${working_dir}/xvfb.log" -s '-screen 0 1920x1080x24 -nolisten tcp' \
        bash "$0" "$timeout_seconds" "$working_dir" "$emulator" --display
fi

game_directory="${working_dir}/nzportable"
console_log="${game_directory}/nzp/condebug.log"
rm -f "${game_directory}/capture.request" "${game_directory}/capture.done" "${game_directory}/capture.bmp"
cd "${working_dir}/shadps4"
export SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=x11 LIBGL_ALWAYS_SOFTWARE=1
export XDG_RUNTIME_DIR="${working_dir}/shadps4/runtime"
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"

icd="$(find /usr/share/vulkan/icd.d -maxdepth 1 -name 'lvp_icd*.json' -print -quit)"
[[ -n "$icd" ]] || { echo 'Mesa lavapipe Vulkan ICD is missing.' >&2; exit 1; }
export VK_DRIVER_FILES="$icd" VK_ICD_FILENAMES="$icd"

for attempt in 1 2 3; do
    if timeout --kill-after=5 10 vulkaninfo --summary > "${working_dir}/vulkan.log" 2>&1; then
        break
    fi

    if [[ "$attempt" == 3 ]]; then
        cat "${working_dir}/vulkan.log" >&2
        exit 1
    fi

    sleep 1
done
setsid timeout --kill-after=5 "$timeout_seconds" "$emulator" --fullscreen true \
    "${working_dir}/ps4-package/uroot/eboot.bin" &
emulator_pid=$!

function cleanup()
{
    kill -TERM -- "-${emulator_pid}" 2>/dev/null || true
    (sleep 5; kill -KILL -- "-${emulator_pid}" 2>/dev/null || true) &
    local killer_pid=$!
    wait "$emulator_pid" 2>/dev/null || true
    kill "$killer_pid" 2>/dev/null || true
    wait "$killer_pid" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

startup_started=$SECONDS
while kill -0 "$emulator_pid" 2>/dev/null; do
    if [[ ! -s "$console_log" ]] && (( SECONDS - startup_started >= 120 )); then
        echo 'shadPS4 produced no engine log within 120 seconds.' >&2
        exit 1
    fi
    if [[ -f "${game_directory}/capture.request" && ! -f "${game_directory}/capture.done" ]]; then
        sleep 1
        timeout --kill-after=5 30 ffmpeg -nostdin -y -f x11grab -video_size 1920x1080 -i "${DISPLAY}+0,0" -frames:v 1 -update 1 "${game_directory}/capture.bmp"
        test -s "${game_directory}/capture.bmp"
        touch "${game_directory}/capture.done"
    fi
    if [[ -f "$console_log" ]]; then
        if grep -qE 'Host_Error|Sys_Error|Test suite failed:' "$console_log"; then
            exit 1
        fi
        if grep -q '^Test suite passed: ' "$console_log"; then
            exit 0
        fi
    fi
    sleep 0.2
done
emulator_status=0
wait "$emulator_pid" || emulator_status=$?
echo "shadPS4 exited with status ${emulator_status}." >&2
if [[ ! -s "$console_log" ]]; then
    echo 'shadPS4 exited without creating the engine console log.' >&2
    exit 1
fi
if grep -qE 'Host_Error|Sys_Error|Test suite failed:' "$console_log"; then
    exit 1
fi
grep -q '^Test suite passed: ' "$console_log"
