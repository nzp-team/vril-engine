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
EMULATOR_BIN="PPSSPPHeadless"

# How many seconds to wait before time out
TIMEOUT=30

# The PPSSPP branch we checkout and use.
ppsspp_version="v1.17.1"

testing_dir_path=""
binary_path=""

# tzdata will try to display an interactive install prompt by
# default, so make sure we define our system as non-interactive.
export DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true

function install_dependencies
{
    print_info "Installing dependancies.."
    apt update -y
    apt install build-essential cmake libgl1-mesa-dev \
    libsdl2-dev libsdl2-ttf-dev libfontconfig1-dev \
    libvulkan-dev libglew-dev clang wget zip git \
    ffmpeg -y
}

function clone_ppsspp
{
    print_info "Cloning PPSSPP [${ppsspp_version}] from GitHub.."
    local command="git clone --recurse-submodules https://github.com/hrydgard/ppsspp -b ${ppsspp_version}"
    cd ${working_dir}
    echo "[${command}]"
    ${command}
}

function build_ppsspp
{
    cd ${working_dir}/ppsspp
    print_info "Patching PPSSPP.."
    local command="git apply ${testing_dir_path}/setup/utils/ppsspp.patch"
    echo "[${command}]"
    $command
    print_info "Building PPSSPP.."
    command="./b.sh --headless"
    echo "[${command}]"
    ${command}
    print_info "Moving build binaries to working directory.."
    mv ${working_dir}/ppsspp/build/PPSSPP* ${working_dir}
    cp -R ${working_dir}/ppsspp/build/assets/* ${working_dir}/assets
}

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
        mv ${binary_path} ${working_dir}/nzportable/
    fi
}

function begin_setup()
{
    testing_dir_path="${1}"
    binary_path="${2}"
    working_dir="${3}"

    # Create our working directory.
    if [[ ! -d "${working_dir}" ]]; then
        mkdir -p "${working_dir}"
    fi

    mkdir -p ${working_dir}/assets
    cd ${working_dir}

    install_dependencies;
    
    # Check if we have the emulator already
    if [[ -f "${working_dir}/${EMULATOR_BIN}" ]]; then
        print_info "Emulator binary found in [${working_dir}], skipping build.."
    else
        # If we don't have it, we need to nuke the directory to ensure a clean build
        # BUT we want to preserve the directory itself if it was mounted
        find ${working_dir} -mindepth 1 -delete
        mkdir -p ${working_dir}/assets
        
        clone_ppsspp;
        build_ppsspp;
    fi

    obtain_nzportable;

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
        echo "${working_dir}/${EMULATOR_BIN} --system=${system} --screenshot=${comp} --timeout=${TIMEOUT} -r ${working_dir}/nzportable/ ${working_dir}/nzportable/EBOOT.PBP"
    else
        echo "${working_dir}/${EMULATOR_BIN} --system=${system} --timeout=${TIMEOUT} -r ${working_dir}/nzportable/ ${working_dir}/nzportable/EBOOT.PBP"
    fi
}