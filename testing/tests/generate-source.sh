#!/bin/bash
#
# Nazi Zombies: Portable
# generate-source
# ----
# Generates source bitmaps for levels validated
# by map-boot. This should never be run automatically,
# and needs to be periodically manually ran to generate
# new masters.
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

#
# run_generation
# ---
# Kicks off generation.
#
function run_generation()
{
    print_info "Beginning content generation.."

    local any_map_failed="0"
    local working_dir="${WORKING_DIR}"
    local content_path="${CONTENT_DIR}/${PLATFORM}${MODE:+-$MODE}"
    
    mkdir -p "${content_path}"

    for bsp in ${working_dir}/nzportable/nzp/maps/*.bsp; do
        local map_failed="0"

        # Get the BSP basename so we can add it to our setup.ini.
        local pretty_bsp=$(basename ${bsp} .bsp)

        # Remove the console log.
        rm -rf ${working_dir}/nzportable/nzp/condebug.log

        # Remove setup.ini and write our new one, this will let us automatically
        # load the .BSP.
        rm -rf ${working_dir}/nzportable/setup.ini
        echo "+developer 1 -cpu333 -user_maps +nosound 1 -condebug +sys_testmode 1 +map ${pretty_bsp}" >> ${working_dir}/nzportable/setup.ini

        # Load emulator and attempt to boot map
        print_info "Loading Nazi Zombies: Portable via [${EMULATOR_BIN}] with map [${pretty_bsp}].."
        local command=$(run_nzportable "1" "${CONTENT_DIR}/blank.png" "${MODE}")
        echo "[${command}]"
        ${command} > /dev/null 2>&1 || true

        # Validate that we were able to enter the server.
        cat ${working_dir}/nzportable/nzp/condebug.log | grep "Server spawned." || map_failed="1"
        
        while read -r host_error; do
            echo "[ERROR]: ${host_error}"
            map_failed="1"
        done < <(grep "Host_Error" ${working_dir}/nzportable/nzp/condebug.log)

        if [[ "${map_failed}" -ne "0" ]]; then
            echo "[ERROR]: FAILED to spawn a server using map [${pretty_bsp}]!"
            echo "         Last 15 lines of console log follows"
            echo "-----"
            cat ${working_dir}/nzportable/nzp/condebug.log | tail -n 15
            echo ""
            echo "-----"
            any_map_failed="1"
        else
            echo "[PASS]: SUCCESSFULLY spawned server using map [${pretty_bsp}]!"
            local move_command="mv $(pwd)/capture.bmp ${content_path}/${pretty_bsp}.bmp"
            ${move_command}
        fi
    done

    if [[ "${any_map_failed}" -ne "0" ]]; then
        exit 1
    else
        exit 0
    fi
}

run_generation;