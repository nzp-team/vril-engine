# Original Xbox

This target uses nxdk, nxdk-gles11, native XID controller reports, and native AC97 audio.
It does not use SDL. It targets the original Xbox's stock CPU and 64 MB of memory.

## Build

Prepare the toolchain and dependencies separately in Toolbox. `Makefile.xbox` only
compiles Vril and links existing archives; it does not fetch or build dependencies.

Set these paths before building:

- `NXDK_DIR`: a prepared nxdk checkout, including its libraries and `cxbe` tool.
- `GLES_DIR`: a prepared nxdk-gles11 checkout. Its `build` directory must contain
  `libGLESv1_CM.lib`, `libswizzle.lib`, and `arena_build/libarena.lib`.
- `MINIMP3_DIR`: the directory containing `minimp3.h` and `minimp3_ex.h`.

Put nxdk's `bin` directory and its LLVM tools on `PATH`, then run:

```sh
make -f Makefile.xbox WERROR=1
```

Toolbox installs these dependencies in its Docker image and exposes the normal build command:

```sh
cd ~/git/toolbox
./nzp build-vril --target xbox -- WERROR=1
```

The output is `build/xbox/nzportable/default.xbe`. The default engine heap is
20 MB; framebuffer, texture, audio, and other platform allocations are additional.
MP3 decoding and image loading use scalar code because the stock CPU has no SSE2.

Dependencies used for initial integration:

| Dependency | Revision |
| --- | --- |
| [nxdk](https://github.com/XboxDev/nxdk) | `004fefbfe8fd93a5ea8ee9a95ff187b0f58ec8ed` |
| [nxdk-gles11](https://github.com/Ryzee119/nxdk-gles11) | `5a726e61f1af5c3e9682af2c833c9703cab5022e` |
| [minimp3](https://github.com/lieff/minimp3) | `ea99364f61c14656440e8d77e9c233ccf3124633` |

Build nxdk-gles11 without gl4es. Its GLES, swizzle, and arena archives are linked
directly. Its bundled `stb_sprintf.h` supplies floating-point formatting, which
nxdk's PDCLib omits. Dependency licenses must accompany binary distributions.

Toolbox explicitly supplies `-O2 -fno-strict-aliasing` for GLES, swizzle,
arena, PDCLib, and pbkit. With nxdk's Generic CMake toolchain, selecting
`Release` alone leaves the optimization flags empty. The remaining SDK libraries
keep their upstream build settings: globally enabling optimization currently
regresses network startup. Vril supplies integer memory routines that are also
safe for the SDK's interrupt-level callers. Audio is mixed in cached RAM before
a sequential, fenced copy to the write-combined DMA buffer.

The prepared nxdk-gles11 checkout also requires the depth-range correction saved
in Toolbox's `config/nxdk-gles11.patch`: rebuild the viewport
matrix when the depth range changes, include the near-depth offset in its Z
translation, and order the hardware depth bounds for reversed ranges. Rebuild
that dependency separately before linking Vril. Toolbox also applies
`config/nxdk-gles11-state.patch`, which reuses identical texture-environment
register programs across texture changes using an exact per-context state
comparison. Active user clipping planes bypass this reuse so plane equations
continue to upload. Toolbox prepares these patches as part of its image.

## Installation

```text
<drive>:\Games\nzportable\
    default.xbe
    setup.ini              optional startup arguments
    nzp\
        config.cfg
        progs.dat
        maps\
        ...
```

nxdk mounts the directory containing the running XBE as `D:`. Vril uses that
directory as its default base directory, so it can launch from E:, F:, G:, or a
test disc without hardcoding the installation drive. Configuration and game data
remain in `nzp`. Logging uses the existing opt-in `-condebug` argument and
`nzp/condebug.log`; no separate platform log is created. The test capture hook
writes `capture.bmp` alongside the XBE.

## Video and input

The backend requests native 1280x720 at 60 Hz when the console's video settings
and AV connection permit it, falling back to 640x480 otherwise. `-width 640` in
`setup.ini` forces the lower resolution. The default frame cap is 60; saved
configuration can override it. VSync uses the shared `r_vsync` cvar.

The existing **Screen Mode** option controls `vid_widescreen`. At 720p, 16:9 uses
the full frame and 4:3 uses a centered 960x720 viewport. At 640x480, widescreen uses
anamorphic projection and a logical widescreen UI. The default follows the
selected video mode and the console's widescreen setting.

**Video → Screen Inset** (`vid_overscan`) shrinks the complete view and UI to fit
TV overscan. It defaults to 5 percent per edge in 480-line modes and zero at
720p; saved preferences override these defaults. Insets remain proportional in
4:3 and anamorphic 16:9.

Legacy `m_pitch` values such as `0.022` and `-0.022` mean normal and inverted
look. Current 0/1 settings retain their meaning. Shared input and menu code use
the same interpretation, and all stick backends use positive Y up.

Ordinary textures use 16-bit RGB5A1 or RGBA4 storage to conserve shared RAM.
Packed uploads are converted to the NV2A's native channel order. Lightmaps retain
the format selected by the shared texture pipeline.

Native XID reports preserve pressure values for A, B, X, Y, White, Black, and both
triggers. They feed the same `Key_PressureEvent` API used on PS2. The existing
`in_pressure` option enables pressure-sensitive bindings; it defaults off as on
other pressure-capable targets. The D-pad, Back, Start, and stick clicks are digital.
Both sticks report normalized axes with positive Y up.

**Controls → Pressure Buttons** contains the analog-button toggle and
**Firm Press Force**, displayed from 0.00 to 1.00 in 0.05 steps. Xbox defaults to
0.25 and PS2 to 0.50. Existing byte-valued `in_pressure_threshold` configurations
remain compatible. Face buttons activate above 20/255,
while triggers retain their 32/255 activation threshold.

| Xbox control | Vril key |
| --- | --- |
| A / B / X / Y | `BOTTOMFACE` / `RIGHTFACE` / `LEFTFACE` / `TOPFACE` |
| White / Black | `LTRIGGER` / `RTRIGGER` |
| Left / right trigger | `ZLTRIGGER` / `ZRTRIGGER` |
| Back / Start | `SELECT` / `START` |
| Left / right stick click | `LTHUMB` / `RTHUMB` |

Rumble uses both variable-speed motors through `IN_PlatformRumble`, including the
shared `in_rumble` setting and duration expiry. The verified Xbox strength gain is
fixed at 16×, saturating at 65535. Zero remains zero. The common protocol reads
motor strengths and duration as unsigned 16-bit values, preserving high strengths.
The existing
software keyboard is enabled. `PLATFORM_DISALLOW_GLYPH_OVERRIDE` hides the glyph
selector; initialization locks `cl_controllerglyphs` to `xbox`. Include
`common/gfx/controller_glyphs/xbox.tga` from the assets repository at
`nzp/gfx/controller_glyphs/xbox.tga` in the installation. The first connected
controller controls the game; disconnecting it releases its keys and allows a
remaining controller to take over.

## Test references

Run the existing source generator through Toolbox to create the Xbox map-boot
reference captures:

```sh
./nzp test-vril --platform xbox --test generate-source \
  --binary /workspace/repos/vril-engine/build/xbox/nzportable/default.xbe
```

The runner uses the local `config/xemu/` BIOS, MCPX ROM, and blank HDD image from
Toolbox. It installs the supplied game data and XBE onto a disposable HDD, then
collects the engine's native framebuffer capture and console log for each map.
Reference images are written to `testing/validate/xbox/`. The shared test mode
uses simulation time for model interpolation and a seeded random stream to avoid
variation from emulator scheduling. Emulator captures validate rendering and map
loading; they do not measure retail performance.

## Runtime notes

xemu's native AC97 callback limitation prevents validating this audio backend in
the emulator: [xemu issue #1529](https://github.com/xemu-project/xemu/issues/1529).

With VSync enabled, Xbox presentation supplies the frame pacing when the
requested FPS cap meets or exceeds the display refresh rate. Lower caps and
frames that skip presentation retain software deadline limiting.

The Toolbox `nxdk-gles11-performance.patch` caches texture/buffer name lookups
(with collision checks and deletion invalidation) and uploads full 8×8 Morton
tiles in sequential bursts instead of scattered writes into write-combined GPU
memory. Partial edge tiles retain the existing path. Dependency preparation stays
in Toolbox. Emulator profiles are compared separately from retail hardware frame rates.

The Xbox renderer writes static vertices directly into mapped GPU buffers, keeps
model indices in CPU-readable storage, and reuses a bounded transient GPU stream.
Animated alias positions use that stream alongside their existing GPU UV buffers,
avoiding the SDK client-array staging and index-range scan. Exact animation
endpoints use the cached static pose.
Colored vertices remain interleaved; HUD quads use four vertices and shared triangle
indices. Repeated world-array descriptors are cached and invalidated when another
submission path changes them. SSE1/MMX lightmap accumulation preserves scalar
integer results; SSE2 is not required. Lightmap update rectangles expand to
complete 8×8 Morton tiles using the CPU atlas for unchanged neighbors. RGBA
lightmaps are packed directly into the native GPU layout in one pass, avoiding
intermediate region and GL-format buffers while preserving the 16/32-bit setting.
Toolbox prepares the standard OES mapped buffer API through `config/nxdk-gles11-buffers.patch`. No SDK retrieval or library
building occurs in the engine Makefile.

The Toolbox state patch keeps the cached combiner program while reissuing its
texture-stage selection after texture validation. Array batches explicitly
disable unused color and UV streams when switching from colored UI draws to
world geometry and models.

The GLES compatibility patch converts negative OpenGL eye-space Z into positive
planar fog distance. This keeps the linear fog factor from clamping to fully
visible geometry in front of the camera.
