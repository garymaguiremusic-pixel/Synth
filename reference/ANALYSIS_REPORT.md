# Osirus / Access Virus C: Analysis Report

Analysed on 2026-11-17 with REA plus direct byte inspection.

## 1. Files

| File | SHA-256 | What it is |
|---|---|---|
| `Osirus.vst3` (29.6 MB) | 461127ab…8cc3cee | Windows x64 VST3 PE, Osirus v2.2.27 by "The Usual Suspects" (the GPL-3 Gearmulator project). It is the Virus **A/B/C** emulator. Exports: `GetPluginFactory`, `InitDll`, `ExitDll`, `bridgeDevice*`. |
| `moduleinfo.json` | n/a | VST3 manifest naming Osirus 2.2.27. It describes the plugin classes only and does not reference the ROM. |
| `Access Virus C (am29f040b_6v6).BIN` (512 KB) | c2b75cd0…ffcc199 | AM29F040B flash dump: Virus C (KC/Indigo 2) OS 6.6, build "368 14.12.2009". |

## 2. How Osirus works (observed from strings and structure)
Osirus is a **DSP56300 emulator**. It has **no synth algorithm of its own**: the oscillators, filters, envelopes and effects are Access's DSP program, which Osirus loads from the ROM and executes. Relevant plugin strings:
- `Program BootROM offset = 0x%x`, `Program BootROM size`, `Invalid ROM, boot rom of size`, `Invalid ROM, unexpected filesize`, `Could not unpack ROM file`: the ROM parser that finds the DSP boot program and program image inside the flash dump.
- `A Virus A/B/C operating system (.bin or .mid) is required`: Osirus also accepts **Access OS-update `.mid` files** as the ROM. **Those are firmware, not presets.** Make sure your `.mid` files are preset banks and not OS updates.
- Bank labels: `Edit, RAM A, RAM B, ROM C … ROM H`. Six ROM banks match the 768 singles found in the BIN.
- `loadPresetFiles`, `.mid`, `.bin`: preset import, the same SysEx format as below.

## 3. ROM layout (observed)
| Range | Content |
|---|---|
| 0x00000–0x3FFFF | Firmware for the hardware's microcontroller (an 8051-family chip; the image starts with `LJMP` vectors). Contains German debug strings, LCD drivers and UI tables. The **DSP program image is embedded here**: the high-entropy run at 0x18000–0x1FFFF is consistent with that. |
| 0x40000–0x47FFF | Empty. |
| 0x48000–0x4FFFF | 128 Multi presets × 256 bytes. |
| 0x50000–0x7FFFF | **768 Single presets × 256 bytes** (ROM banks C–H). Byte 0 = Version = **7** in every one. Name at bytes 240–249. |

## 4. Preset format: everything needed to replace the BIN for preset import
Single dump (one per SysEx event inside a `.mid` file):
```
F0 00 20 33 01 <dev> 10 <bank> <prog> <256 bytes> <cs> F7        (267 bytes)
cs = sum(msg[5..264]) & 0x7F
bank: 0 = edit buffer, 1.. = banks
```
- Bytes 0–127 are Page A (0x70) and bytes 128–255 are Page B (0x71).
- Multi dump: cmd 0x11. Parameter change: `F0 00 20 33 01 dev <page> <part> <index> <value> F7`.
- Request commands: 0x30 (single), 0x31 (multi), 0x32 (single bank), 0x33 (multi bank), 0x34 (arrangement), 0x35 (global), 0x36 (total), 0x37 (controller dump).

Generated files:
- `virusC_single_256byte_map.csv`: offset, page, index, name, display name, min, max, default, display type, for all 256 bytes. Built from Osirus's own A/B/C parameter JSON, so the ranges are the C ranges.
- `osirus_valuelists.json.txt`: every enum, including filter modes, saturation curves, delay/reverb modes, LFO shapes, Osc3 modes, mod sources (28) and destinations, unison, phaser stages, distortion curves and categories.
- `osirus_parameterDescriptions.json`: the complete original JSON, covering parameter descriptions, regions, MIDI packets and the CC map.
- `rom_usage_stats_by_offset.csv`: for each byte, the distinct count, min, max and most common value across all 768 factory singles. This shows which features Virus C patches actually use. Unison, phaser, distortion, EQ, bass boost, Osc3 waves and Assign 4–6 all vary, so they are live features. Bytes 103, 111 and 250 are constant.

**Data quirk:** a handful of factory patches hold out-of-range values. Examples: Filter1 Mode = 51 and 101 (once each), Saturation = 42/49/50, Osc FM Mode = 28/115/127. How the DSP interprets these is unknown. The importer should clamp them, log them, and record them as calibration test cases.

## 5. What was not done, and why
- **No 1:1 path without the ROM.** Bit-exact output means running Access's DSP code, which is what Osirus does.
- **I did not extract or translate the DSP program.** It is Access Music's copyrighted firmware. Re-implementing it from a disassembly would be copying it, and your synth would still be Access's code. REA also has no DSP56300 decompiler.
- **I did not export the factory presets.** They are Access content; I only used aggregate statistics.

## 6. Route to "as close as possible"
Build a clean-room Virus C engine on the exact preset format. Then **calibrate it against Osirus + your BIN as a black box**: render parameter sweeps, measure the curves, fit lookup tables, and A/B-score real patches. This is what `CLAUDE_CODE_PROMPT.md` sets up.

## 7. Licence
The parameter JSON comes from Gearmulator, which is GPL-3. Using it in a private test build is fine. If you distribute a synth containing it, the synth must be GPL-3 too.

## 8. REA Evidence
- Osirus exports: `ev_faf13e159ac572e86d80405fdec1535722b7335a84b77dde37d8e4df052982f6`
- ROM entropy: `ev_0dd3ec494f6948beffece01fd69d0ec6304b829bf2c781e513ab8fa631602ec7`
