#!/bin/bash
#
# Nazi Zombies: Portable
# Test suite utility functions.
# ----
# Prepares a testing environment targeting
# PlayStation Portable builds.
#
# This is intended to be used via a Docker 
# container running ubuntu:24.04.
#

#
# print_info
# ---
# [INFO] status printing with dividers for readability.
#
function print_info
{
    local text="${1}"
    echo ""
    echo "----------------------------"
    echo "[INFO]: ${text}"
    echo "----------------------------"
    echo ""
}

#
# print_error
# ---
# [ERROR] status printing with dividers for readability, 
# if second arg is "1", will bail immediately.
#
function print_error
{
    local text="${1}"
    local should_bail="${2}"

    echo ""
    echo "----------------------------"
    echo "[ERROR]: ${text}"
    echo "----------------------------"
    echo ""

    if [[ "${should_bail}" -ne "0" ]]; then
        exit 1
    fi
}

function map_boot_arguments
{
	local map_name="${1}"
	local test_mode="${2:-1}"
	echo "+developer 1 +nosound 1 -condebug +show_fps 0 +r_retro 1 +host_framerate 0.05 +sys_testmode ${test_mode} +map ${map_name}"
}

function test_game_path()
{
	echo "${working_dir}/nzportable"
}

#
# apply_content_overrides
# ---
# Applies assets or QuakeC overlays if requested in Pull Request.
#
function apply_content_overrides
{
	local game_directory="${working_dir}/nzportable/nzp"

	if [[ -n "${ASSETS_OVERRIDE_DIRECTORY:-}" && -d "${ASSETS_OVERRIDE_DIRECTORY}" ]]; then
		print_info "Applying assets ref: [${ASSETS_OVERRIDE_DIRECTORY}].."
		mkdir -p "${game_directory}"
		find "${game_directory}" -depth -mindepth 1 ! -path "${game_directory}/progs.dat" -delete
		cp -a "${ASSETS_OVERRIDE_DIRECTORY}/." "${game_directory}/"
	fi

	if [[ -n "${QUAKEC_OVERRIDE_FILE:-}" && -f "${QUAKEC_OVERRIDE_FILE}" ]]; then
		print_info "Applying QuakeC ref: [${QUAKEC_OVERRIDE_FILE}].."
		mkdir -p "${game_directory}"
		cp "${QUAKEC_OVERRIDE_FILE}" "${game_directory}/progs.dat"
	fi
}
