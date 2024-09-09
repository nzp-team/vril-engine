# Nazi Zombies: Portable dQuakePlus

## About
This repository contains the PlayStation Portable and Nintendo 3DS engine for NZ:P, based on dQuakePlus and ctrQuake, containing optimizations from the NZ:P Team, adQuake, and Xash3D-PSP, as well as NZ:P-specific feature implementation. It has also been modified to build on the latest versions of the [PSPSDK](https://github.com/pspdev/pspsdk).

## Building for PlayStation Portable
Building requires a full install of [psptoolchain](https://github.com/pspdev/psptoolchain/). You can either follow the instructions on the GitHub repository or use a Docker container (we recommend [the official one](https://hub.docker.com/r/pspdev/pspdev))!

With the psptoolchain installed, you now need to install `libpspmath`, which we have included in the GitHub repository:
```bash
cd source/libpspmath
make && make install
```
Now you can navigate back to the root of the repository and build an `EBOOT`.

```bash
cd ../../
make -f Makefile.psp install
```

We also provide a prebuilt EBOOT on the [Releases](https://github.com/nzp-team/dquakeplus/releases/tag/bleeding-edge) page.

## Building for Nintendo 3DS
Building requires a full install of [libctru](https://github.com/devkitPro/libctru). You can either follow the instructions on the GitHub repository or use a Docker container (we recommend [the official one](devkitpro/devkitarm))!

With the psptoolchain installed, you now need to install the latest `picaGL`, which needs cloned from the official GitHub repository:
```bash
git clone https://github.com/masterfeizz/picaGL.git -b revamp
cd picaGL
mkdir clean
make install
```
Now you can navigate to the root of the repository and build the `.3dsx`.

```bash
make -f Makefile.ctr
```

We also provide prebuilt .3dsx files on the [Releases](https://github.com/nzp-team/dquakeplus/releases/tag/bleeding-edge) page.