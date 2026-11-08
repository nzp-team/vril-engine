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


"""Launch xemu and retrieve the native test log and framebuffer from its HDD."""
import importlib.util
import json
from pathlib import Path
import socket
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location("disk", Path(__file__).with_name("xemu-hdd.py"))
disk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(disk)


def run(timeout, working, emulator, roms, require_capture):
    config = working / "xemu.toml"
    files = {"bootrom_path": roms / "mcpx_1.0.bin", "flashrom_path": roms / "bios.bin",
             "eeprom_path": working / "eeprom.bin", "hdd_path": working / "xbox_hdd.qcow2",
             "dvd_path": working / "xbox-test.iso"}
    config.write_text('[general]\nshow_welcome = false\n[sys]\navpack = "hdtv"\n'
                      '[display]\nrenderer = "OPENGL"\n'
                      '[display.window]\nstartup_size = "640x480"\nvsync = false\n'
                      '[display.ui]\nshow_menubar = false\nuse_animations = false\n'
                      'aspect_ratio = "auto"\n[sys.files]\n' +
                      "\n".join(key + " = " + json.dumps(str(value)) for key, value in files.items()) + "\n")
    endpoint = working / "xemu-qmp.sock"
    endpoint.unlink(missing_ok=True)
    connection = None
    stream = None

    def command(name, arguments=None):
        request = {"execute": name}
        if arguments is not None:
            request["arguments"] = arguments
        connection.sendall(json.dumps(request).encode() + b"\n")
        while True:
            line = stream.readline()
            if not line:
                raise RuntimeError("xemu closed its monitor connection")
            reply = json.loads(line)
            if "error" in reply:
                raise RuntimeError(reply["error"])
            if "return" in reply:
                return reply["return"]

    def read_output(path):
        image = disk.Qcow(working / "xbox_hdd.qcow2")
        try:
            return disk.Fatx(image).read("Games/nzportable/" + path)
        except StopIteration:
            return b""
        finally:
            image.file.close()

    previous_log = read_output("nzp/condebug.log")
    process = subprocess.Popen([emulator, "-config_path", str(config), "-qmp",
                                "unix:" + str(endpoint) + ",server,nowait"])
    try:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError("xemu exited with status " + str(process.returncode))
            if connection is None:
                if not endpoint.exists():
                    time.sleep(0.2)
                    continue
                connection = socket.socket(socket.AF_UNIX)
                connection.settimeout(15)
                connection.connect(str(endpoint))
                stream = connection.makefile("rb")
                stream.readline()
                command("qmp_capabilities")
            blocks = command("query-block")
            drive = next(block["device"] for block in blocks if not block["removable"])
            command("human-monitor-command", {"command-line": 'qemu-io ' + drive + ' "flush"'})
            log = read_output("nzp/condebug.log")
            if log == previous_log:
                time.sleep(1)
                continue
            if log:
                (working / "nzportable/nzp/condebug.log").write_bytes(log)
            if b"Host_Error" in log or b"Sys_Error" in log or b"Test suite failed:" in log:
                raise RuntimeError(log[-4000:].decode(errors="replace"))
            if b"Test suite passed:" in log:
                capture = read_output("capture.bmp") if require_capture else b""
                if capture or not require_capture:
                    if capture:
                        (working / "nzportable/capture.bmp").write_bytes(capture)
                    print(log[-4000:].decode(errors="replace"))
                    return
            time.sleep(1)
        raise TimeoutError("Timed out waiting for the Xbox map-boot capture")
    finally:
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        if stream is not None:
            stream.close()
        if connection is not None:
            connection.close()
        endpoint.unlink(missing_ok=True)


if __name__ == "__main__":
    run(int(sys.argv[1]), Path(sys.argv[2]).resolve(), sys.argv[3], Path(sys.argv[4]).resolve(), sys.argv[5] != "0")
