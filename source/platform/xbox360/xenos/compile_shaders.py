#!/usr/bin/env python3
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

import argparse
from pathlib import Path
import subprocess
import sys

SHADERS = (
    ("hyena.vert", "hyena.vert"),
    ("hyena.frag", "hyena.frag"),
    ("hyena_texture.frag", "hyena_texture.frag"),
    ("hyena_solid.frag", "hyena_solid.frag"),
    ("hyena_alpha.frag", "hyena_texture.frag", "-DALPHA_TEST"),
    ("hyena_fog_alpha.frag", "hyena.frag", "-DALPHA_TEST"),
    ("hyena_solid_fog.frag", "hyena_solid.frag", "-DFOG"),
    ("present.vert", "present.vert"),
    ("present.frag", "present.frag"),
)


def write_shader(header, name, blob, constants):
    symbol = name.replace(".", "_")
    header.write(f"static const uint8_t {symbol}_data[] __attribute__((aligned(256))) = {{\n")
    for offset in range(0, len(blob), 16):
        row = ", ".join(f"0x{byte:02x}" for byte in blob[offset:offset + 16])
        header.write(f"    {row},\n")
    header.write("};\n")
    header.write(f"static const xenr_constant_t {symbol}_constants[] = {{\n")
    for register, values in constants:
        row = ", ".join(f"{float(value).hex()}f" for value in values)
        header.write(f"    {{ {register}, {{ {row} }} }},\n")
    if not constants:
        header.write("    { 0, { 0, 0, 0, 0 } },\n")
    header.write("};\n")
    header.write(f"static const xenr_shader_t {symbol} = {{ {symbol}_data, {symbol}_constants, {len(constants)} }};\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--xost", required=True, type=Path)
    parser.add_argument("--glslang", default="glslangValidator")
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    sys.path.insert(0, str(args.xost.resolve()))
    from xsc.lower import lower_ex
    from xsc.spirv import Module

    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(".tmp")
    try:
        with temporary.open("w") as header:
            header.write("""#pragma once
#include <stdint.h>

typedef struct { unsigned index; float value[4]; } xenr_constant_t;
typedef struct { const uint8_t * data; const xenr_constant_t * constants; unsigned count; } xenr_shader_t;
""")
            for name, source, *flags in SHADERS:
                spirv = args.output.parent / (name + ".spv")
                subprocess.run([args.glslang, "-V", *flags, str(args.source / source), "-o", str(spirv)], check=True)
                blob, constants = lower_ex(Module.parse(spirv.read_bytes()), optimize=True)
                header.write("\n")
                write_shader(header, name, blob, constants)
        temporary.replace(args.output)
    finally:
        temporary.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
