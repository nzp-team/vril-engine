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
WORKING_DIR="$4"

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
    local working_dir="${WORKING_DIR}"

    for bsp in ${working_dir}/nzportable/nzp/maps/*.bsp; do
        local map_failed="0"
        local emulator_failed="0"

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

        local command=$(run_nzportable "1" "${CONTENT_DIR}/${PLATFORM}/${pretty_bsp}.bmp" "${MODE}")
        echo "[${command}]"
        ${command} > /dev/null 2>&1 || emulator_failed="1"

        # Validate that we were able to enter the server.
        cat ${working_dir}/nzportable/nzp/condebug.log | grep "Server spawned." || map_failed="1"

        while read -r host_error; do
            echo "[ERROR]: ${host_error}"
            map_failed="1"
        done < <(grep "Host_Error" ${working_dir}/nzportable/nzp/condebug.log)

        if [[ "${map_failed}" -ne "0" ]] || [[ "${emulator_failed}" -ne "0" ]]; then
            echo "[ERROR]: FAILED to spawn a server using map [${pretty_bsp}]!"
            echo "         Last 15 lines of console log follows"
            echo "-----"
            cat ${working_dir}/nzportable/nzp/condebug.log | tail -n 15
            echo ""
            echo "-----"
            any_map_failed="1"
        else
            echo "[PASS]: SUCCESSFULLY spawned server using map [${pretty_bsp}]!"
        fi

        ffmpeg -nostdin -y -i "${CONTENT_DIR}/${PLATFORM}/${pretty_bsp}.bmp" -i "$(pwd)/__testfailure.bmp" -filter_complex \
        "[0:v][1:v]psnr=stats_file=psnr_stats.log[psnr_out]; \
        [0:v][1:v]blend=all_mode='difference'[diff_out]" \
        -map "[psnr_out]" -f null - \
        -map "[diff_out]" "$(pwd)/difference.png" &> /tmp/ffmpeg_log.txt || ffmpeg_failed="1"

        if [[ "${ffmpeg_failed}" -ne "0" ]]; then
            echo "[ERROR]: ffmpeg failed to calculate PSNR of content."
            echo "         logs follow:"
            echo "----------------------------------------------------"
            cat /tmp/ffmpeg_log.txt
            echo "----------------------------------------------------"
            any_map_failed="1"
        fi

        local map_psnr=$(cat /tmp/ffmpeg_log.txt | sed -n 's/.*average:\([0-9.]*\|inf\).*/\1/p')
        map_psnr=${map_psnr%.*}

        if [[ "${map_psnr}" == "inf" ]] || [[ "${map_psnr}" -gt "35" ]]; then
            echo "[PASS]: Got PSNR value of [${map_psnr}]"
        else
            echo "[ERROR]: PSNR value was less than [35], got [${map_psnr}]!"
            echo "         Writing comparison data to [${WORKING_DIR}/fail/map-boot]"

            mkdir -p "${WORKING_DIR}/fail/map-boot"
            cp "${CONTENT_DIR}/${PLATFORM}/${pretty_bsp}.bmp" "${WORKING_DIR}/fail/map-boot/${pretty_bsp}_source.bmp" || true
            mv "$(pwd)/__testfailure.bmp" "${WORKING_DIR}/fail/map-boot/${pretty_bsp}_new.bmp" || true
            mv "$(pwd)/difference.png" "${WORKING_DIR}/fail/map-boot/${pretty_bsp}_diff.png" || true
            any_map_failed="1"
        fi

        rm -rf "$(pwd)/difference.png"
        rm -rf "$(pwd)/__testfailure.bmp"
        rm -rf "$(pwd)/__testcompare.png"
    done

    if [[ "${any_map_failed}" -ne "0" ]]; then
        exit 1
    else
        exit 0
    fi
}

run_mapboot_test;