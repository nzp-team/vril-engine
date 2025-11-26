# Nazi Zombies: Portable Vril Engine


## About
This repository contains the PlayStation Portable, Nintendo 3DS and TI NSPIRE engine for NZ:P, based on dQuakePlus, ctrQuake, and nQuake lovingly titled "Vril". It contains optimizations from the NZ:P Team, adQuake, and Xash3D-PSP, as well as NZ:P-specific feature implementation. It has also been modified to build on the latest versions of the [PSPSDK](https://github.com/pspdev/pspsdk).

## Building for PlayStation Portable
Building requires a full install of [psptoolchain](https://github.com/pspdev/psptoolchain/). You can either follow the instructions on the GitHub repository or use a Docker container (we recommend [the official one](https://hub.docker.com/r/pspdev/pspdev))!

With the psptoolchain installed, you now need to install `libpspmath` via the PSPSDK package manager:
```bash
psp-pacman -Syu libpspmath
```
Now you can navigate to the root of the repository and build an `EBOOT`.

```bash
make -f Makefile.psp
```

We also provide a prebuilt EBOOT on the [Releases](https://github.com/nzp-team/vril-engine/releases/tag/bleeding-edge) page.

## Building for Nintendo 3DS
Building requires a full install of [libctru](https://github.com/devkitPro/libctru). You can either follow the instructions on the GitHub repository or use a Docker container (we recommend [the official one](devkitpro/devkitarm))!

With the psptoolchain installed, you now need to install the latest `picaGL`, which needs cloned from the official GitHub repository:
```bash
git clone https://github.com/masterfeizz/picaGL.git -b revamp
cd picaGL
make clean
make install
```
Now you can navigate to the root of the repository and build the `.3dsx`.

```bash
make -f Makefile.ctr
```

We also provide prebuilt .3dsx files on the [Releases](https://github.com/nzp-team/vril-engine/releases/tag/bleeding-edge) page.

## Building for TI NSPIRE
Building requires a full install of [ndless](https://github.com/ndless-nspire/Ndless), though it can also be built with an unofficial [docker image](https://hub.docker.com/r/bensuperpc/ndless).

With ndless installed, you can begin building the `.tns`.

```bash
make -f Makefile.nspire
```
We also provide a prebuilt .tns on the [Releases](https://github.com/nzp-team/vril-engine/releases/tag/bleeding-edge) page.

# Building for PlayStation VITA
Building requires a full install of [Vita SDK](https://vitasdk.org/). You can either follow the instructions on the GitHub repository or use a Docker container (we recommend [the official one](https://hub.docker.com/r/vitasdk/vitasdk))!

With Vita SDK installed, you can navigate to the root of the repository and build a `VPK`.

```bash
make -f Makefile.psp2
```

We also provide a prebuilt VPK on the [Releases](https://github.com/nzp-team/vril-engine/releases/tag/bleeding-edge) page.
