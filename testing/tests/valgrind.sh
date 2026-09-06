#!/bin/bash
#
# Nazi Zombies: Portable
# Valgrind test
# ----
# Runs Valgrind on Linux binaries.
#
# This is done via an emulator, and intended
# to be used via a Docker container running
# ubuntu:24.04
#

set -o errexit

PLATFORM="$1"
CONTENT_DIR="$2"
MODE="$3"
WORKING_DIR="$4"
VALGRIND_DURATION_MIN="2"

source "setup/${PLATFORM}.sh"

function run_valgrind_test()
{
	if [[ "${PLATFORM}" != "linux" ]]; then
		return
	fi

    if [[ "$(uname -m)" != "x86_64" ]]; then
        return
    fi

	local console_log="${WORKING_DIR}/nzportable/nzp/condebug.log"
	local launch_log="${WORKING_DIR}/launcher_output.log"
	local valgrind_log="${WORKING_DIR}/valgrind_report.log"
	local command
	local pid

	print_info "Beginning Valgrind memory leak test ([${VALGRIND_DURATION_MIN}] minutes)..."
	rm -f "${console_log}" "${launch_log}" "${valgrind_log}"
	write_test_setup "nzp_xmas2" "0"
	
	command=$(run_nzportable "0" "" "${MODE}" "1")
	echo "[Running: ${command}]"

    # Run NZ:P with Valgrind in background
    set -m
    eval "${command}" > "${launch_log}" 2>&1 &
    pid=$!
    set +m

    local duration_seconds=$((VALGRIND_DURATION_MIN * 60))
    echo "Running NZ:P with Valgrind for [${duration_seconds}] seconds..."
    sleep "${duration_seconds}"

    if kill -0 "${pid}" 2>/dev/null; then
        echo "Sending SIGINT to Valgrind process group -${pid}..."
        kill -INT "-${pid}" 2>/dev/null || kill -INT "${pid}"

        # Wait for graceful exit
        local timeout=15
        while kill -0 "${pid}" 2>/dev/null && [ ${timeout} -gt 0 ]; do
            sleep 1
            ((timeout--))
        done

        # Kill..
        if kill -0 "${pid}" 2>/dev/null; then
            echo "Valgrind did not exit gracefully; sending SIGKILL..."
            kill -KILL "-${pid}" 2>/dev/null || kill -9 "${pid}"
        fi

        wait "${pid}" 2>/dev/null || true
    fi

	if [[ -f "${valgrind_log}" ]] && grep -q "ERROR SUMMARY: 0 errors" "${valgrind_log}" && ! grep -q "definitely lost:" "${valgrind_log}"; then
		echo "[PASS]: Valgrind memory leak test passed successfully."
		return 0
	fi

	echo "[ERROR]: Valgrind memory leak test failed or detected leaks."
	mkdir -p "${WORKING_DIR}/fail/valgrind"
	cp "${launch_log}" "${WORKING_DIR}/fail/valgrind/launcher.log" || true
	
	if [[ -f "${valgrind_log}" ]]; then
		cp "${valgrind_log}" "${WORKING_DIR}/fail/valgrind/valgrind_report.log"
	fi
	if [[ -f "${console_log}" ]]; then
		cp "${console_log}" "${WORKING_DIR}/fail/valgrind/console.log"
	fi
	
	{
		echo "## valgrind"
		echo ""
		echo "- Valgrind detected memory leaks or errors. See valgrind_report.log for details."
	} > "${WORKING_DIR}/fail/summary.md"
	
	return 1
}

run_valgrind_test