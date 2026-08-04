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
	echo "+developer 1 +nosound 1 -condebug +show_fps 0 +host_framerate 0.05 +sys_testmode 1 +map ${map_name}"
}
