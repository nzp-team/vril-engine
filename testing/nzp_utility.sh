#!/bin/bash
#
# Nazi Zombies: Portable
# Test suite utility functions.
# ----
# Prepares a testing environment targeting
# PlayStation Portable builds.
#
# This is intended to be used via a Docker 
# container running ubuntu:24.10.
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