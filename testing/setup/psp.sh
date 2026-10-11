#!/bin/bash
#
# Nazi Zombies: Portable
# PlayStation Portable test code init
# ----
# Prepares a testing environment targeting
# PlayStation Portable builds.
#
# This is intended to be used via a Docker 
# container running ubuntu:24.04.
#
set -o errexit

source "nzp_utility.sh"

# Read by our test scripts.
EMULATOR_BIN="/opt/ppsspp/PPSSPPHeadless"

# How many seconds to wait before time out
TIMEOUT=480

testing_dir_path=""
binary_path=""

function obtain_nzportable
{
    print_info "Obtaining latest Nazi Zombies: Portable PSP release.."
    sleep 0.5
    cd ${working_dir}
    rm -f nzportable-psp.zip
    wget https://github.com/nzp-team/nzportable/releases/download/nightly/nzportable-psp.zip
    unzip -o nzportable-psp.zip

    if [ -n "${binary_path}" ]; then
        print_info "Replacing nightly binary with user-specified: [${binary_path}]"
        sleep 0.5
        cp ${binary_path} ${working_dir}/nzportable/
    fi
}

function begin_setup()
{
    testing_dir_path="${1}"
    binary_path="${2}"
    working_dir="${3}"

    mkdir -p "${working_dir}"
    if [[ ! -x "${EMULATOR_BIN}" ]]; then
        print_error "Toolbox PPSSPPHeadless is unavailable at [${EMULATOR_BIN}]!" "1"
    fi

    obtain_nzportable;
    apply_content_overrides;

    print_info "Done setting up for PlayStation Portable testing!"
}

#
# run_nzportable
# ---
# Returns command used to run game via emulator.
# Modes:
# 0 - Normal (just run)
# 1 - Compare
#
function run_nzportable()
{
    local mode="$1"
    local comp="$2"
    local system="$3"

    if [[ "${system}" == "" ]]; then
        system="slim"
    fi

    if [[ "${mode}" == "1" ]]; then
        echo "${EMULATOR_BIN} --system=${system} --screenshot=${comp} --timeout=${TIMEOUT} -r ${working_dir}/nzportable/ ${working_dir}/nzportable/EBOOT.PBP"
    else
        echo "${EMULATOR_BIN} --system=${system} --timeout=${TIMEOUT} -r ${working_dir}/nzportable/ ${working_dir}/nzportable/EBOOT.PBP"
    fi
}

function write_test_setup()
{
	local map_name="${1}"
	local test_mode="${2:-1}"
	printf '%s\n' "$(map_boot_arguments "${map_name}" "${test_mode}") -cpu333" > "${working_dir}/nzportable/setup.ini"
}

function capture_path()
{
	echo "$(pwd)/capture.bmp"
}
