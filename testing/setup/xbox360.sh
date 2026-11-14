#!/bin/bash
#
# Nazi Zombies: Portable
# Xbox 360 test code init
# ----
# Prepares a testing environment targeting
# Xbox 360 builds.
#
# This is intended to be used via a Docker 
# container running ubuntu:24.04.
#

set -o errexit
source "nzp_utility.sh"

EMULATOR_BIN="${XENON_BIN:-/opt/xenon/Xenon}"
TIMEOUT=600
export VRIL_PR_ENV_NO_CAPTURE=1

function begin_setup()
{
    testing_dir_path="$1"
    binary_path="$2"
    working_dir="$3"
    [[ -f "${binary_path}" ]] || print_error "Supply an Xbox 360 emulator ELF with --binary." "1"
    [[ -x "${EMULATOR_BIN}" ]] || print_error "Xenon is unavailable at [${EMULATOR_BIN}]." "1"

    xenon_work="${working_dir}/xenon-test"
    mkdir -p "${xenon_work}/home/.local/share/Xenon"
    cp "${binary_path}" "${xenon_work}/xenon.elf"
    local game_path="$(test_game_path)"
    mkdir -p "${game_path}/nzp"
    cd "${working_dir}"
    if [[ ! -f nzportable-3ds.zip ]]; then
        curl --fail --silent --show-error --location --retry 3 https://github.com/nzp-team/nzportable/releases/download/nightly/nzportable-3ds.zip --output nzportable-3ds.zip.tmp
        mv nzportable-3ds.zip.tmp nzportable-3ds.zip
    fi
    python3 -m zipfile -e nzportable-3ds.zip .
    apply_content_overrides

    cd "${xenon_work}"
    truncate -s 0 nand.bin
    truncate -s 16M nand.bin
    printf '\377\117\0\0\0\0\0\0\0\0\200\0\0\0\0\0' | dd of=nand.bin conv=notrunc status=none
    printf '0000000000000000\n%.0s' {1..12} > fuses.txt

    local disk_size=$(( $(du -sm "${game_path}/nzp" | cut -f1) + 64 ))
    (( disk_size >= 256 )) || disk_size=256
    truncate -s 0 game.img
    truncate -s "${disk_size}M" game.img
    mkfs.fat -F 32 game.img >/dev/null
    mmd -i game.img ::/nzportable ::/nzportable/nzp
    local asset
    for asset in "${game_path}"/nzp/*; do
        [[ "${asset##*/}" == "_cache" ]] && continue
        mcopy -s -o -i game.img "${asset}" ::/nzportable/nzp/
    done
    cat > home/.local/share/Xenon/config.toml <<'TOML'
[XCPU]
ElfLoader = true
Simulate1BL = true
RAMSize = "512MiB"
[SMC]
UARTSystem = "print"
[HighlyExperimental]
CPUExecutor = "Interpreted"
[Network]
Enabled = false
[Log]
Level = 4
Type = "sync"
[Paths]
ElfBinary = "xenon.elf"
Nand = "nand.bin"
Fuses = "fuses.txt"
HDDImage = "game.img"
TOML
    cd "${testing_dir_path}"
}

function write_test_setup()
{
    local mode="${2:-1}"
    case "${mode}" in
        1) xenon_test=map-boot ;;
        2) xenon_test=restart-stress ;;
        3) xenon_test=qcvm-opcodes ;;
        *) print_error "Unsupported Xbox 360 test mode [${mode}]." "1" ;;
    esac
    if [[ "${mode}" == "3" ]]; then
        printf '%s\n' "-noudp +sys_testmode 3" > "${xenon_work}/setup.ini"
    else
        printf '%s\n' "-noudp -dedicated 1 +developer 1 +host_framerate 0.05 +sys_testmode ${mode} +map $1" > "${xenon_work}/setup.ini"
    fi
    mcopy -o -i "${xenon_work}/game.img" "${xenon_work}/setup.ini" ::/nzportable/setup.ini
}

function run_nzportable()
{
    echo "bash ${testing_dir_path}/setup/utils/run-xenon.sh ${TIMEOUT} ${xenon_work} ${EMULATOR_BIN} $(test_game_path)/nzp/condebug.log ${xenon_test}"
}
