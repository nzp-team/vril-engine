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
    local console_log="$(test_game_path)/nzp/condebug.log"
    local content_path="${CONTENT_DIR}/${PLATFORM}${MODE:+-$MODE}"
    local captured_image="$(capture_path)"
    local launch_log="${WORKING_DIR}/generate-source.log"
    
    mkdir -p "${content_path}"

    for bsp in ${working_dir}/nzportable/nzp/maps/*.bsp; do
        local map_failed="0"

        # Get the BSP basename so we can add it to our setup.ini.
        local pretty_bsp=$(basename ${bsp} .bsp)

        # Remove output from the previous map.
        rm -f "${console_log}" "${captured_image}"

        # Write the platform launch configuration used to load the BSP.
        write_test_setup "${pretty_bsp}"

        # Load emulator and attempt to boot map
        print_info "Loading Nazi Zombies: Portable via [${EMULATOR_BIN}] with map [${pretty_bsp}].."
        local command=$(run_nzportable "1" "${CONTENT_DIR}/blank.png" "${MODE}")
        echo "[${command}]"
        ${command} > "${launch_log}" 2>&1 || map_failed="1"
        [[ -s "${captured_image}" ]] || map_failed="1"

        # Validate that we were able to enter the server.
        if [[ ! -f "${console_log}" ]]; then
            map_failed="1"
        else
            grep "Server spawned." "${console_log}" || map_failed="1"
            while read -r host_error; do
                echo "[ERROR]: ${host_error}"
                map_failed="1"
            done < <(grep "Host_Error" "${console_log}" || true)
        fi

        if [[ "${map_failed}" -ne "0" ]]; then
            echo "[ERROR]: FAILED to generate a capture for map [${pretty_bsp}]!"
            echo "         Launcher output and last 15 lines of console log follow"
            echo "-----"
            cat "${launch_log}"
            mkdir -p "${WORKING_DIR}/fail/generate-source"
            cp "${launch_log}" "${WORKING_DIR}/fail/generate-source/${pretty_bsp}_launcher.log"
            cp "${console_log}" "${WORKING_DIR}/fail/generate-source/${pretty_bsp}_console.log" 2>/dev/null || true
            tail -n 15 "${console_log}" || true
            echo ""
            echo "-----"
            any_map_failed="1"
        else
            echo "[PASS]: SUCCESSFULLY spawned server using map [${pretty_bsp}]!"
            mv "${captured_image}" "${content_path}/${pretty_bsp}.bmp"
        fi
    done

    if [[ "${any_map_failed}" -ne "0" ]]; then
        exit 1
    else
        exit 0
    fi
}

run_generation;
