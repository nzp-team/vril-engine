#!/bin/bash
#
# Nazi Zombies: Portable
# TI-NSPIRE test code init
# ----
# Prepares a testing environment targeting
# TI-NSPIRE builds.
#
# This is intended to be used via a Docker 
# container running ubuntu:24.10.
#
set -o errexit

source "nzp_utility.sh"

# Read by our test scripts.
EMULATOR_BIN="firebird-emu"

# How many seconds to wait before time out
TIMEOUT=30

# The firebird branch we checkout and use.
firebird_version="v1.6"

testing_dir_path=""

workspace_path="/working"

# tzdata will try to display an interactive install prompt by
# default, so make sure we define our system as non-interactive.
export DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true

function install_dependencies
{
    print_info "Installing dependancies.."
    apt update -y
    apt install build-essential wget unzip qtquickcontrols2-5-dev qtmultimedia5-dev qtdeclarative5-dev qtcreator qtchooser qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools qt3d5-dev qt3d5-dev-tools qt3d-defaultgeometryloader-plugin -y
}

function clone_firebird
{
    print_info "Cloning firebird [${firebird_version}] from GitHub.."
    local command="git clone --recurse-submodules https://github.com/nspire-emus/firebird.git -b ${firebird_version}"
    cd ${workspace_path}
    echo "[${command}]"
    ${command}
}

function build_firebird
{
    cd ${workspace_path}
    print_info "Building firebird.."
    command="mkdir -p build && cd build && qmake .. && make"
    echo "[${command}]"
    cd ${workspace_path}/firebird
    eval ${command}
    print_info "Moving build binaries to working directory.."
    mv ${workspace_path}/firebird/build/${EMULATOR_BIN}* ${workspace_path}
}

function obtain_nzportable
{
    print_info "Obtaining latest Nazi Zombies: TI-NSPIRE release.."
    sleep 0.5
    cd ${workspace_path}
    wget https://github.com/nzp-team/nzportable/releases/download/nightly/nzportable-nspire.zip
    unzip nzportable-nspire.zip

    print_info "Replacing binary with GitHub Actions Artifact.."
    sleep 0.5
    #mv /nspire-nzp-tns/nzportable.tns ${workspace_path}/nzportable/
}

function begin_setup()
{
    testing_dir_path="${1}"

    # Create our working directory.
    rm -rf ${workspace_path} || true
    mkdir -p ${workspace_path}
    cd ${workspace_path}

    install_dependencies;
    clone_firebird;
    build_firebird;
    obtain_nzportable;

    print_info "Done setting up for TI-NSPIRE testing!"
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
        echo "${workspace_path}/${EMULATOR_BIN} --system=${system} --screenshot=${comp} --timeout=${TIMEOUT} -r ${workspace_path}/nzportable/ ${workspace_path}/nzportable/EBOOT.PBP"
    else
        echo "${workspace_path}/${EMULATOR_BIN} --system=${system} --timeout=${TIMEOUT} -r ${workspace_path}/nzportable/ ${workspace_path}/nzportable/EBOOT.PBP"
    fi
}