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
    local content_path="${CONTENT_DIR}/${PLATFORM}${MODE:+-$MODE}"
    local captured_image="$(capture_path)"
    local launch_log="${WORKING_DIR}/launch.log"
    local failed_maps=()
    local failure_details=()

    for bsp in ${working_dir}/nzportable/nzp/maps/*.bsp; do
        local map_failed="0"
        local emulator_failed="0"
        local ffmpeg_failed="0"

        # Get the BSP basename so we can add it to our setup.ini.
        local pretty_bsp=$(basename ${bsp} .bsp) 

        # Remove the console log.
        rm -rf ${working_dir}/nzportable/nzp/condebug.log

        # Write the platform launch configuration used to load the BSP.
        write_test_setup "${pretty_bsp}"

        # Load emulator and attempt to boot map
        print_info "Loading Nazi Zombies: Portable via [${EMULATOR_BIN}] with map [${pretty_bsp}].."
        local command=$(run_nzportable "1" "${content_path}/${pretty_bsp}.bmp" "${MODE}")
        echo "[${command}]"
        ${command} > "${launch_log}" 2>&1 || emulator_failed="1"

        # Validate that we were able to enter the server.
        local console_log="${working_dir}/nzportable/nzp/condebug.log"
        if [[ ! -f "${console_log}" ]] || ! grep -q "Server spawned." "${console_log}"; then
            map_failed="1"
        fi

        if [[ -f "${console_log}" ]]; then
            while read -r host_error; do
                echo "[ERROR]: ${host_error}"
                map_failed="1"
            done < <(grep "Host_Error" "${console_log}" || true)
        fi

        if [[ "${map_failed}" -ne "0" ]] || [[ "${emulator_failed}" -ne "0" ]]; then
            echo "[ERROR]: FAILED to spawn a server using map [${pretty_bsp}]!"
            echo "         Launcher output follows"
            echo "-----"
            cat "${launch_log}"
            echo ""
            echo "-----"
            if [[ -f "${console_log}" ]]; then
                echo "         Last 15 lines of console log follows"
                echo "-----"
                tail -n 15 "${console_log}"
                echo ""
                echo "-----"
            else
                echo "         The engine exited before creating condebug.log."
            fi
            any_map_failed="1"
            failed_maps+=("${pretty_bsp}")
            failure_details+=("- \`${pretty_bsp}\`: emulator or server startup failed")
            mkdir -p "${WORKING_DIR}/fail/map-boot"
            cp "${launch_log}" "${WORKING_DIR}/fail/map-boot/${pretty_bsp}_launcher.log" || true
            if [[ -f "${console_log}" ]]; then
                cp "${console_log}" "${WORKING_DIR}/fail/map-boot/${pretty_bsp}_console.log"
            fi
            continue
        else
            echo "[PASS]: SUCCESSFULLY spawned server using map [${pretty_bsp}]!"
        fi

        ffmpeg -nostdin -y -i "${content_path}/${pretty_bsp}.bmp" -i "${captured_image}" -filter_complex \
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
            failed_maps+=("${pretty_bsp}")
            failure_details+=("- \`${pretty_bsp}\`: FFmpeg could not compare the capture")
            mkdir -p "${WORKING_DIR}/fail/map-boot"
            cp /tmp/ffmpeg_log.txt "${WORKING_DIR}/fail/map-boot/${pretty_bsp}_ffmpeg.log" || true
            cp "${content_path}/${pretty_bsp}.bmp" "${WORKING_DIR}/fail/map-boot/${pretty_bsp}_source.bmp" || true
            mv "${captured_image}" "${WORKING_DIR}/fail/map-boot/${pretty_bsp}_new.bmp" || true
            continue
        fi

        local map_psnr=$(grep -o 'average:[^ ]*' /tmp/ffmpeg_log.txt | cut -d: -f2 | head -n1)

        if [[ "$map_psnr" == "inf" ]]; then
            echo "[PASS]: Captures were identical!"
        else
            local map_psnr_int=${map_psnr%.*}

            if (( map_psnr_int >= 35 )); then
                echo "[PASS]: Got PSNR value of [${map_psnr}]"
            else
                echo "[ERROR]: PSNR value was less than [35], got [${map_psnr}]!"
                echo "         Writing comparison data to [${WORKING_DIR}/fail/map-boot]"

                mkdir -p "${WORKING_DIR}/fail/map-boot"
                cp "${content_path}/${pretty_bsp}.bmp" "${WORKING_DIR}/fail/map-boot/${pretty_bsp}_source.bmp" || true
                mv "${captured_image}" "${WORKING_DIR}/fail/map-boot/${pretty_bsp}_new.bmp" || true
                mv "$(pwd)/difference.png" "${WORKING_DIR}/fail/map-boot/${pretty_bsp}_diff.png" || true

                any_map_failed="1"
                failed_maps+=("${pretty_bsp}")
                failure_details+=("- \`${pretty_bsp}\`: image comparison was ${map_psnr} dB PSNR (minimum: 35 dB)")
            fi
        fi

        rm -rf "$(pwd)/difference.png"
        rm -rf "${captured_image}"
    done

    if [[ "${any_map_failed}" -ne "0" ]]; then
        echo ""
        echo "========================================"
        echo " MAP BOOT TEST FAILURES"
        echo "========================================"
        printf '%s\n' "${failed_maps[@]}" | sort -u
        echo "========================================"

        mkdir -p "${WORKING_DIR}/fail"
        {
            echo "## map-boot"
            echo ""
            printf '%s\n' "${failure_details[@]}"
        } > "${WORKING_DIR}/fail/summary.md"
        return 1
    else
        return 0
    fi
}

run_mapboot_test;
