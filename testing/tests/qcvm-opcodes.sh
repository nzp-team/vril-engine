#!/bin/bash
#
# Nazi Zombies: Portable
# QCVM tests
# ----
# In-engine QCVM opcode behavior testing.
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

source "setup/${PLATFORM}.sh"

function run_qcvm_opcode_test()
{
	local console_log="${WORKING_DIR}/nzportable/nzp/condebug.log"
	local launch_log="${WORKING_DIR}/qcvm-opcodes.log"
	local command
	local exit_status

	print_info "Beginning platform-native QCVM opcode tests.."
	rm -f "${console_log}" "${launch_log}"
	write_test_setup "ndu" "3"
	command=$(run_nzportable "0" "" "${MODE}")
	echo "[${command}]"

	set +o errexit
	${command} > "${launch_log}" 2>&1
	exit_status=$?
	set -o errexit

	if [[ "${exit_status}" -eq 0 ]] &&
		[[ -f "${console_log}" ]] &&
		grep -q "QCVM opcode tests passed." "${console_log}" &&
		! grep -qE "QCVM opcode test failed|Host_Error|Sys_Error" "${console_log}"; then
		echo "[PASS]: Platform-native QCVM opcode tests passed."
		return
	fi

	echo "[ERROR]: Platform-native QCVM opcode tests failed."
	mkdir -p "${WORKING_DIR}/fail/qcvm-opcodes"
	cp "${launch_log}" "${WORKING_DIR}/fail/qcvm-opcodes/launcher.log" || true
	if [[ -f "${console_log}" ]]; then
		cp "${console_log}" "${WORKING_DIR}/fail/qcvm-opcodes/console.log"
	fi
	{
		echo "## qcvm-opcodes"
		echo ""
		echo "- Platform-native QCVM opcode tests did not complete successfully."
		echo "- Exit status: ${exit_status}"
	} > "${WORKING_DIR}/fail/summary.md"
	return 1
}

run_qcvm_opcode_test
