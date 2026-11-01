#!/bin/bash

set -o errexit

source "nzp_utility.sh"

EMULATOR_BIN="${PCSX2_BIN:-/opt/pcsx2/AppRun}"
TIMEOUT=600

function begin_setup()
{
	testing_dir_path="$1"
	binary_path="$2"
	working_dir="$3"
	if [[ ! -x "${EMULATOR_BIN}" || ! -f "$(dirname "${EMULATOR_BIN}")/usb-sync.so" ]]; then
		print_error "PCSX2 is unavailable at [${EMULATOR_BIN}]. Run setup/utils/install-pcsx2.sh first." "1"
	fi
	if [[ ! -f "${PS2_BIOS:-}" ]]; then
		print_error "Set PS2_BIOS to a PS2 BIOS file." "1"
	fi
	if [[ ! -f "${binary_path}" ]]; then
		print_error "A PS2 ELF must be supplied with --binary." "1"
	fi
	mkdir -p "${working_dir}"
	cd "${working_dir}"
	if [[ ! -f nzportable-3ds.zip ]]; then
		wget -q https://github.com/nzp-team/nzportable/releases/download/nightly/nzportable-3ds.zip
	fi
	unzip -oq nzportable-3ds.zip
	cp "${binary_path}" nzportable/nzportable.elf
	apply_content_overrides
	printf '\nshow_fps "0"\nvid_widescreen "0"\n' >> nzportable/nzp/config.cfg
	mkdir -p "$(test_game_path)/nzp" "${working_dir}/PCSX2/inis" "${working_dir}/PCSX2/bios"
	cp "${PS2_BIOS}" "${working_dir}/PCSX2/bios/ps2.bin"
	print_info "Preparing PS2 USB image.."
	truncate -s 512M "${working_dir}/usb.img"
	mkfs.fat -F 32 "${working_dir}/usb.img"
	mmd -i "${working_dir}/usb.img" ::/APPS ::/APPS/NZPORTABLE
	mcopy -s -o -i "${working_dir}/usb.img" nzportable/nzp ::/APPS/NZPORTABLE/
	cat > "${working_dir}/PCSX2/inis/PCSX2.ini" <<CONFIG
[UI]
SettingsVersion = 1
SetupWizardIncomplete = false
[Folders]
Bios = ${working_dir}/PCSX2/bios
[EmuCore]
EnableFastBoot = true
EnableCheats = false
EnablePatches = false
[EmuCore/GS]
Renderer = 13
VsyncEnable = 0
[EmuCore/Speedhacks]
EECycleRate = 0
EECycleSkip = 0
vuThread = false
vu1Instant = false
vuFlagHack = false
[SPU2/Output]
OutputModule = nullout
[BIOS]
Filename = ps2.bin
[USB1]
Type = Msd
Msd_subtype = 0
Msd_ImagePathMsd = ${working_dir}/usb.img
[USB2]
Type = None
CONFIG
	cd "${testing_dir_path}"
}

function test_game_path()
{
	echo "${working_dir}/usb/APPS/NZPORTABLE"
}

function write_test_setup()
{
	printf '%s\n' "$(map_boot_arguments "$1" "${2:-1}") -nosound -nocdaudio +vid_widescreen 0" > "$(test_game_path)/setup.ini"
	mcopy -o -i "${working_dir}/usb.img" "$(test_game_path)/setup.ini" ::/APPS/NZPORTABLE/setup.ini
}

function run_nzportable()
{
	echo "bash ${testing_dir_path}/setup/utils/run-pcsx2.sh ${TIMEOUT} ${working_dir} ${EMULATOR_BIN}"
}

function capture_path()
{
	echo "$(test_game_path)/capture.bmp"
}
