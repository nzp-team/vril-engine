#!/usr/bin/env python3
#
# Copyright (C) 2026 NZ:P Team
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
import urllib.request

parser = argparse.ArgumentParser()
parser.add_argument("action", choices=["prepare", "run"])
parser.add_argument("timeout", type=int)
parser.add_argument("working", type=Path)
parser.add_argument("emulator", type=Path)
args = parser.parse_args()
working = args.working.resolve()
runtime = working / "rpcs3"
env = os.environ.copy()
env["XDG_CONFIG_HOME"] = str(runtime / "config")
env["XDG_CACHE_HOME"] = str(runtime / "cache")
env["XDG_RUNTIME_DIR"] = str(runtime / "run")
env.setdefault("SDL_AUDIODRIVER", "dummy")
env.setdefault("LIBGL_ALWAYS_SOFTWARE", "1")
env.setdefault("LP_NUM_THREADS", "2")
profile = runtime / "config/rpcs3"
flash = Path(env.get("PS3_DEV_FLASH", str(runtime / "dev_flash"))).resolve()
configuration = runtime / "config.yml"


def launch(arguments, name):
    command = [str(args.emulator.resolve()), *arguments]

    if sys.platform.startswith("linux"):
        env.setdefault("QT_QPA_PLATFORM", "xcb")
        icds = sorted(Path("/usr/share/vulkan/icd.d").glob("lvp_icd*.json"))
        if icds:
            env.setdefault("VK_DRIVER_FILES", str(icds[0]))
            env.setdefault("VK_ICD_FILENAMES", str(icds[0]))
        if not env.get("DISPLAY"):
            command = ["xvfb-run", "-a", "-s", "-screen 0 1920x1080x24 -nolisten tcp", *command]
    started = time.time()

    with (working / name).open("wb") as log:
        process = subprocess.Popen(
            command,
            env=env,
            cwd=runtime,
            stdin=subprocess.DEVNULL,
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True
        )

        try:
            deadline = time.monotonic() + args.timeout

            while process.poll() is None:
                if time.monotonic() >= deadline:
                    raise RuntimeError("RPCS3 timed out; see " + str(working / name))
                if name == "rpcs3-launch.log":
                    output = (working / name).read_text(errors="replace")
                    if "RPCS3: Segfault" in output or "Thread terminated due to fatal error" in output:
                        raise RuntimeError("RPCS3 crashed; see " + str(working / name))
                    console = working / "nzportable/nzp/condebug.log"
                    if console.is_file():
                        text = console.read_text(errors="replace")
                        if any(marker in text for marker in ["Host_Error", "Sys_Error", "Test suite failed:"]):
                            raise RuntimeError("The PS3 engine reported a test failure")
                time.sleep(0.2)

            status = process.returncode
        finally:
            if process.poll() is None:
                try:
                    os.killpg(process.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    process.wait()

    if status and name == "rpcs3-firmware.log":
        emulator_log = runtime / "cache/rpcs3/RPCS3.log"
        diagnostic = (working / name).read_text(errors="replace")
        if (status in (134, -signal.SIGABRT) and "manual_typemap" in diagnostic
                and (flash / "sys/external/liblv2.sprx").is_file()
                and emulator_log.is_file() and emulator_log.stat().st_mtime >= started
                and "Successfully installed PS3 firmware version" in emulator_log.read_text(errors="replace")):
            print("RPCS3 completed firmware installation before its headless installer teardown failed.", file=sys.stderr)
            return

    if status:
        raise RuntimeError("RPCS3 exited with status " + str(status) + "; see " + str(working / name))


try:
    runtime.mkdir(parents=True, exist_ok=True)
    Path(env["XDG_RUNTIME_DIR"]).mkdir(parents=True, exist_ok=True, mode=0o700)

    if args.action == "prepare":
        profile.mkdir(parents=True, exist_ok=True)
        for directory in ["dev_hdd0", "dev_hdd1", "dev_flash2", "dev_flash3", "dev_usb000", "dev_bdvd", "games"]:
            (runtime / directory).mkdir(parents=True, exist_ok=True)

        flash.mkdir(parents=True, exist_ok=True)
        vfs = {"$(EmulatorDir)": str(runtime) + "/", "/dev_flash/": str(flash) + "/"}
        (profile / "vfs.yml").write_text(json.dumps(vfs, indent=2) + "\n")

        config = {
            "Core": {
                "PPU Decoder": "Recompiler (LLVM)",
                "Max LLVM Compile Threads": 1,
                "LLVM Precompilation": False,
                "SPU Decoder": "Recompiler (LLVM)"
            },
            "Video": {
                "Renderer": "Vulkan",
                "Resolution": "1920x1080",
                "Aspect ratio": "16:9",
                "Shader Mode": "Legacy Recompiler (single-threaded)",
                "Shader Compiler Threads": 1,
                "Write Color Buffers": True,
                "VSync Mode": "Disabled"
            },
            "Audio": {
                "Renderer": "Null",
                "Music Handler": "Null"
            },
            "Input/Output": {
                "Keyboard": "Null",
                "Mouse": "Null",
                "Camera": "Null"
            },
            "Miscellaneous": {
                "Automatically start games after boot": True,
                "Exit RPCS3 when process finishes": True,
                "Pause emulation on RPCS3 focus loss": False,
                "Play music during boot sequence": False,
                "Show PPU compilation hint": False,
                "Show shader compilation hint": False,
                "Use native user interface": True
            }
        }
        configuration.write_text(json.dumps(config, indent=2) + "\n")

        if not (flash / "sys/external/liblv2.sprx").is_file():
            firmware = Path(env.get("PS3_FIRMWARE", str(working / "PS3UPDAT.PUP"))).resolve()
            if not firmware.is_file():
                if "PS3_FIRMWARE" in env:
                    raise RuntimeError("PS3_FIRMWARE does not exist: " + str(firmware))
                url = "http://dus01.ps3.update.playstation.net/update/ps3/image/us/2026_0318_a2b60b6ac1d2e49e230144345616927c/PS3UPDAT.PUP"
                urllib.request.urlretrieve(url, firmware)
                if hashlib.md5(firmware.read_bytes()).hexdigest() != "a2b60b6ac1d2e49e230144345616927c":
                    firmware.unlink()
                    raise RuntimeError("PS3 firmware checksum mismatch")
            launch(["--headless", "--config", str(configuration), "--installfw", str(firmware)], "rpcs3-firmware.log")

            if not (flash / "sys/external/liblv2.sprx").is_file():
                raise RuntimeError("RPCS3 did not install the firmware")
    else:
        console = working / "nzportable/nzp/condebug.log"
        launch(["--no-gui", "--config", str(configuration),
                str(runtime / "dev_hdd0/game/NZPT00001/USRDIR/EBOOT.BIN")], "rpcs3-launch.log")
        text = console.read_text(errors="replace") if console.is_file() else ""
        if any(marker in text for marker in ["Host_Error", "Sys_Error", "Test suite failed:"]):
            raise RuntimeError("The PS3 engine reported a test failure")
        if "Test suite passed: " not in text:
            raise RuntimeError("RPCS3 exited without a completed engine test suite")
except (OSError, RuntimeError) as error:
    print(str(error), file=sys.stderr)
    sys.exit(1)
