#!/bin/bash
#
# Nazi Zombies: Portable
# stress-restart tests
# ----
# Validates against memory leaks when running server
# restart flow by executing 100 restarts back-to-back.
#
# This is done via an emulator, and intended
# to be used via a Docker container running
# ubuntu:24.04
#

set -o errexit

source "setup/${PLATFORM}.sh"

function run_restart_stress_test()
{
	local console_log="${WORKING_DIR}/nzportable/nzp/condebug.log"
	local launch_log="${WORKING_DIR}/restart-stress.log"
	local command
	local exit_status
	local engine_pid
	local tail_pid

	print_info "Beginning same-map restart stress test.."
	rm -f "${console_log}" "${launch_log}"
	touch "${console_log}"
	write_test_setup "ndu" "2"

	# Allow for 100 five-second intervals plus startup overhead.
	TIMEOUT=600
	command=$(run_nzportable "0" "" "${MODE}")
	echo "[${command}]"

	set +o errexit
	${command} > "${launch_log}" 2>&1 &
	engine_pid=$!
	tail -n 0 --pid="${engine_pid}" -F "${console_log}" &
	tail_pid=$!
	wait "${engine_pid}"
	exit_status=$?
	wait "${tail_pid}" || true
	set -o errexit

	if [[ "${exit_status}" -eq 0 ]] &&
		[[ -f "${console_log}" ]] &&
		grep -q "Restart stress test passed after 100 restarts." "${console_log}" &&
		! grep -qE "Host_Error|failed on allocation|Sys_Error" "${console_log}"; then
		echo "[PASS]: Completed 100 same-map restarts without crashing."
		return
	fi

	echo "[ERROR]: Same-map restart stress test failed."
	mkdir -p "${WORKING_DIR}/fail/restart-stress"
	cp "${launch_log}" "${WORKING_DIR}/fail/restart-stress/launcher.log" || true
	if [[ -f "${console_log}" ]]; then
		cp "${console_log}" "${WORKING_DIR}/fail/restart-stress/console.log"
	fi
	{
		echo "## restart-stress"
		echo ""
		echo "- Engine exited before completing 100 same-map restarts on ndu."
		echo "- Exit status: ${exit_status}"
	} > "${WORKING_DIR}/fail/summary.md"
	return 1
}

run_restart_stress_test
