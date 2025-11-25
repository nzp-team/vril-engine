#!/bin/bash
#
# Nazi Zombies: Portable
# map-boot tests
# ----
# Verifies that we can boot into all of our
# built-in maps without visual breakage via MSE
# validation.
#
# This is done via an emulator, and intended
# to be used via a Docker container running
# ubuntu:24.04
#
set -o errexit

PLATFORM="$1"
CONTENT_DIR="$2"
MODE="$3"

source "setup/${PLATFORM}.sh"

#
# run_mapboot_test
# ---
# Kicks off our map-boot test.
#
function run_mapboot_test()
{
    print_info "Beginning map-boot test.."

    local any_map_failed="0"

    for bsp in /working/nzportable/nzp/maps/*.bsp; do
        local map_failed="0"
        local emulator_failed="0"

        # Get the BSP basename so we can add it to our setup.ini.
        local pretty_bsp=$(basename ${bsp} .bsp) 

        # Remove the console log.
        rm -rf /working/nzportable/nzp/condebug.log

        # Remove setup.ini and write our new one, this will let us automatically
        # load the .BSP.
        rm -rf /working/nzportable/setup.ini
        echo "+developer 1 -cpu333 -user_maps +nosound 1 -condebug +sys_testmode 1 +map ${pretty_bsp}" >> /working/nzportable/setup.ini

        # Load emulator and attempt to boot map
        print_info "Loading Nazi Zombies: Portable via [${EMULATOR_BIN}] with map [${pretty_bsp}].."
        local command=$(run_nzportable "1" "${CONTENT_DIR}/${PLATFORM}/${pretty_bsp}.bmp" "${MODE}")
        echo "[${command}]"
        ${command} > /dev/null 2>&1 || emulator_failed="1"

        # Validate that we were able to enter the server.
        cat /working/nzportable/nzp/condebug.log | grep "Server spawned." || map_failed="1"

        if [[ "${map_failed}" -ne "0" ]] || [[ "${emulator_failed}" -ne "0" ]]; then
            echo "[ERROR]: FAILED to spawn a server using map [${pretty_bsp}]!"
            any_map_failed="1"
        else
            echo "[PASS]: SUCCESSFULLY spawned server using map [${pretty_bsp}]!"
        fi

        # Test if __testfailure.bmp exists in pwd, if it does we failed MSE validation
        if [[ -f "$(pwd)/__testfailure.bmp" ]]; then
            echo "[ERROR]: FAILED to validate MSE of [${pretty_bsp}], something is wrong visually!"
            rm -rf "$(pwd)/__testfailure.bmp"
            rm -rf "$(pwd)/__testcompare.png"
            any_map_failed="1"
        fi
    done

    if [[ "${any_map_failed}" -ne "0" ]]; then
        exit 1
    else
        exit 0
    fi
}

run_mapboot_test;