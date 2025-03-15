#!/bin/bash
#
# Nazi Zombies: Portable
# PlayStation Portable test code init
# ----
# Prepares a testing environment targeting
# PlayStation Portable builds.
#
# This is intended to be used via a Docker 
# container running ubuntu:24.10.
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

# tzdata will try to display an interactive install prompt by
# default, so make sure we define our system as non-interactive.
export DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true

function install_dependencies
{
    print_info "Installing dependancies.."
    apt update -y
    apt install build-essential cmake libgl1-mesa-dev \
    libsdl2-dev libsdl2-ttf-dev libfontconfig1-dev \
    libvulkan-dev libglew-dev clang wget zip git -y
}

function clone_ppsspp
{
    print_info "Cloning PPSSPP [${ppsspp_version}] from GitHub.."
    local command="git clone --recurse-submodules https://github.com/hrydgard/ppsspp -b ${ppsspp_version}"
    cd /working
    echo "[${command}]"
    ${command}
}

function build_ppsspp
{
    cd /working/
    print_info "Patching PPSSPP.."
    local command="patch -p0 ppsspp/headless/Headless.cpp ${testing_dir_path}/setup/utils/ppsspp.patch"
    echo "[${command}]"
    $command
    print_info "Building PPSSPP.."
    command="./b.sh --headless"
    echo "[${command}]"
    cd /working/ppsspp
    ${command}
    print_info "Moving build binaries to working directory.."
    mv /working/ppsspp/build/PPSSPP* /working
    cp -R /working/ppsspp/build/assets/* /working/assets
}

function obtain_nzportable
{
    print_info "Obtaining latest Nazi Zombies: Portable PSP release.."
    sleep 0.5
    cd /working
    wget https://github.com/nzp-team/nzportable/releases/download/nightly/nzportable-psp.zip
    unzip nzportable-psp.zip

    print_info "Replacing binary with GitHub Actions Artifact.."
    sleep 0.5
    mv /psp-nzp-eboot/EBOOT.PBP /working/nzportable/
}

function begin_setup()
{
    testing_dir_path="${1}"

    # Create our working directory.
    rm -rf /working
    mkdir -p /working/assets
    cd /working

    install_dependencies;
    clone_ppsspp;
    build_ppsspp;
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
        echo "/working/${EMULATOR_BIN} --system=${system} --screenshot=${comp} --timeout=${TIMEOUT} -r /working/nzportable/ /working/nzportable/EBOOT.PBP"
    else
        echo "/working/${EMULATOR_BIN} --system=${system} --timeout=${TIMEOUT} -r /working/nzportable/ /working/nzportable/EBOOT.PBP"
    fi
}