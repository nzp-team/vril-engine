#!/bin/bash
set -o errexit
source "nzp_utility.sh"

EMULATOR_BIN="${SHADPS4_BIN:-/opt/shadps4/AppRun}"
TIMEOUT=600

function begin_setup()
{
    testing_dir_path="$1"
    binary_path="$2"
    working_dir="$3"
    local pkgtool="${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core"
    local profile="$(dirname "${EMULATOR_BIN}")/profile"

    [[ "$(uname -m)" == x86_64 ]] || print_error 'shadPS4 tests require native x86_64 Linux.' 1
    [[ -x "${EMULATOR_BIN}" && -f "${profile}/config.json" ]] || print_error 'Use a Toolbox image with shadPS4 test support.' 1
    [[ -x "${pkgtool}" ]] || print_error 'The Toolbox PS4 package tool is unavailable.' 1
    [[ -f "${binary_path}" ]] || print_error 'Supply the PS4 package with --binary.' 1
    mkdir -p "${working_dir}"
    cd "${working_dir}"
    if [[ ! -f nzportable-3ds.zip ]]; then
        curl -fL --retry 3 --connect-timeout 15 --max-time 300 \
            https://github.com/nzp-team/nzportable/releases/download/nightly/nzportable-3ds.zip \
            -o nzportable-3ds.zip
    fi
    unzip -oq nzportable-3ds.zip
    apply_content_overrides
    compgen -G "${working_dir}/nzportable/nzp/maps/*.bsp" > /dev/null
    rm -rf "${working_dir}/ps4-package"
    "${pkgtool}" pkg_extract --passcode 00000000000000000000000000000000 \
        "${binary_path}" "${working_dir}/ps4-package"
    test -s "${working_dir}/ps4-package/uroot/eboot.bin"
    mkdir -p "${working_dir}/shadps4/user"
    cp -a "${profile}/." "${working_dir}/shadps4/user/"
    mkdir -p "${working_dir}/shadps4/user/data"
    ln -sfn "${working_dir}/nzportable" "${working_dir}/shadps4/user/data/nzportable"
    printf '\nshow_fps "0"\nbrightness "0.5"\n' >> nzportable/nzp/config.cfg
    # Explicit opt-in: normal console screenshots never wait for a CI host.
    touch nzportable/capture.enable
    cd "${testing_dir_path}"
}

function write_test_setup()
{
    printf '%s\n' "$(map_boot_arguments "$1" "${2:-1}") -nosound -nocdaudio" > "$(test_game_path)/setup.ini"
}

function run_nzportable()
{
    echo "bash ${testing_dir_path}/setup/utils/run-shadps4.sh ${TIMEOUT} ${working_dir} ${EMULATOR_BIN}"
}

function capture_path()
{
    echo "$(test_game_path)/capture.bmp"
}
