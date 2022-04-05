# Nazi Zombies: Portable dQuakePlus

## About
This repository contains the PSP engine for NZ:P, based on dQuakePlus and containing optimizations from the NZ:P Team, adQuake, and Xash3D-PSP, as well as NZ:P-specific feature implementation. It has also been modified to build on the latest versions of the [PSPSDK](https://github.com/pspdev/pspsdk).

## Building (Advanced)
Building requires a full install of [psptoolchain](https://github.com/pspdev/psptoolchain/). You can either follow the instructions on the GitHub repository or use a Docker container (we recommend [sharkwouter](https://hub.docker.com/r/sharkwouter/pspdev)'s)!

With the psptoolchain installed, you now need to install `libpspmath`, which we have included in the GitHub repository:
```bash
cd source/libpspmath
make && make install
```
Now you can navigate back to the root of the repository and build an `EBOOT`.

```bash
cd ../../
make -f MakePHAT install # for PSP PHAT/1000
make -f MakeSLIM install # for any other model
```

We also provide prebuilt EBOOTs on the [Releases](https://github.com/nzp-team/dquakeplus/releases/tag/bleeding-edge) page.