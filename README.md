# Nazi Zombies: Portable Vril Engine

## About
This repository contains the engine for _Nazi Zombies: Portable_, lovingly titled "[Vril](https://callofduty.fandom.com/wiki/Vril_Device)". It is the decendent of many _Quake_ source ports such as dQuakePlus, ctrQuake, and Quakespasm. It has been heavily modified from these bases for multi-platform support, optimization, and NZ:P-specific protocol and gameplay changes.

Development requires [NZ:P Toolbox](https://github.com/nzp-team/toolbox), an all-in-one Docker image and collection of glue scripts used for NZ:P development and testing for all supported targets.

## Building
You can build for a specific platform in Toolbox with `./nzp build-vril --target <target>`, where target can be `linux-x86_64`, `psp`, etc. `--help` will show you the full list of targets.

## Testing
You can run PR testing for a specific platform in Toolbox with the `test-vril`, using `./nzp help` will show you the list of all supported arguments for platform, test specification, etc. Test scripts are found in the `testing/tests` directory.