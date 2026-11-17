# osirusRender (offline Osirus renderer for calibration)

A small command-line program built on Gearmulator's Virus A/B/C device library (GPL-3, like Gearmulator).
It boots the Virus firmware from **your own ROM** once, then renders job after job for the calibration
harness. Each patch arrives as a complete single dump by SysEx, so every byte is applied, including the ones
Osirus does not expose as host parameters (delay/reverb settings, Clock Tempo).

The ROM is only read by the emulator at runtime. Never commit it.

## Build (Linux)

```bash
git clone --depth 1 https://github.com/dsp56300/gearmulator
```

Then, inside the checkout: fetch the submodules recursively (`source/cpu/dsp56300`, `source/3rdparty/*`, `source/cpu/mc68k`),
copy this folder to `source/axel/osirusRender/`, add `add_subdirectory(osirusRender)` to `source/axel/CMakeLists.txt`, and run:

```bash
cmake . -B temp/b -DCMAKE_BUILD_TYPE=Release -Dgearmulator_SYNTH_OSIRUS=ON -Dgearmulator_SYNTH_OSTIRUS=OFF -Dgearmulator_SYNTH_VAVRA=OFF -Dgearmulator_SYNTH_XENIA=OFF -Dgearmulator_SYNTH_NODALRED2X=OFF -Dgearmulator_SYNTH_JE8086=OFF -Dgearmulator_SYNTH_88EMU=OFF -Dgearmulator_COMPONENT_DSPBRIDGE=OFF -Dgearmulator_BUILD_INSTALLER=OFF
```

```bash
cmake --build temp/b --target osirusRender -j4
```

## Use

Put the ROM alone in a folder and set in `Calib/config.json`:

```json
"osirus_render": "/path/to/osirusRender",
"osirus_rom_dir": "/path/to/rom_folder/"
```

With `osirus_render` set, the harness uses it instead of hosting the Osirus VST3.

Note: if you instead host the Osirus VST3 in pedalboard on Linux, rebuild it with the shared C++ runtime
(comment out `add_link_options(-static-libgcc -static-libstdc++)` in `source/cmake/base.cmake`). The statically linked
runtime crashes inside the Python host.
