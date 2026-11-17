# Prompt for Claude Code: build "VC-Clone", a Virus C-style test synth

Before starting, create a new empty project folder and copy these files from your analysis folder into `<project>/reference/`:
- `virusC_single_256byte_map.csv`
- `osirus_valuelists.json.txt`
- `osirus_parameterDescriptions.json`
- `rom_usage_stats_by_offset.csv`

Then paste everything below the line into Claude Code.

---

You are building **VC-Clone**, a clean-room software synthesizer for A/B testing. It imports Access Virus C single presets from `.mid`/`.syx` files and should sound as close as possible to the Osirus emulator running a Virus C OS 6.6 ROM. The synth must NOT need, read, embed or translate any Access ROM or DSP code. Its behaviour is designed from the documented parameter set and then **calibrated by measuring audio** from Osirus, which is used only as a black-box reference.

## Primary goal: 1:1 testing against the original
The user wants **1:1 comparison testing**. Every feature you build must be checked side by side against the original, **Osirus.vst3 running the user's Virus C ROM**. That pairing is the ground truth. "Done" means VC-Clone matches it within the measured tolerances in section 5, not "it sounds roughly similar".

- From the very first stage, build a **1:1 test harness**. It loads the same preset into both plugins, plays identical MIDI (same notes, velocities, timing, tempo, sample rate and block size), renders both outputs and compares them automatically.
- Each comparison produces PASS/FAIL per parameter and per patch against the tolerances. Also report a null test (reference minus ours, residual level in dB). Save the WAVs of both renders and the difference, for listening.
- Before comparing, align the two outputs: measure and compensate the latency between them, and match levels only where the test says to. Find Osirus's internal sample rate by measurement and record it rather than assuming it.
- Never mark a feature complete until its 1:1 tests pass. If a tolerance can't be met, report the remaining gap with numbers. Don't hide it or loosen the threshold silently.
- Free-running randomness can't match sample-for-sample: noise, S&H/S&G LFOs, random mod source, free-running oscillator phase and analog drift. For those, compare statistics (spectrum, level, rate distribution) rather than waveforms, and set `Osc Init Phase` to a fixed value in probe patches wherever possible.
- Sample-identical output is not possible without running Access's DSP code, which this project must not use. Report the 1:1 tests as "matches within X", never as "bit-identical".

## 0. Ground rules
- Stack: C++20, JUCE 8, CMake. Targets: VST3 + Standalone, plus a headless `vc_render` CLI. Windows x64 first.
- Modules:
  - `PresetIO`: pure code, unit-tested.
  - `Engine`: DSP with no JUCE dependency.
  - `Plugin`: JUCE wrapper with a generated UI.
  - `Calib`: Python measurement tooling.
- Engine runs internally at 44.1 kHz with high-quality resampling to the host rate. Oversample oscillators and saturation 2–4×. Filter state in double precision.
- Every 0–127 → physical-value mapping lives in a table under `Engine/Tables/`. Start each with a documented guess; section 5 replaces it with measured data. Track which tables are "measured" and which are "guessed".
- `reference/` data comes from the GPL-3 Gearmulator project. This is a private test build; do not redistribute unless the project is GPL-3.

## 1. Preset import (`PresetIO`): byte-exact
1. **SMF reader**: MIDI file types 0 and 1. Walk all tracks and events. Handle running status, skip meta events, collect SysEx (`F0 <vlq> …`), and join `F7 <vlq>` continuation packets. Also accept raw `.syx` (split on F0…F7).
2. **Detect Access OS-update `.mid` files** (firmware, not presets: lots of non-0x10/0x11 Virus SysEx and no single dumps). Reject them with a clear message.
3. **Virus SysEx** `F0 00 20 33 01 <dev> <cmd> …`:
   - `0x10` Single dump: `<bank> <prog> <256 data> <cs> F7`, where `cs = sum(msg[5..264]) & 0x7F`. On a mismatch, warn and still import. If the dump is 512 bytes (a Virus TI dump), keep the first 256 bytes, store the rest as opaque, and warn.
   - `0x11` Multi dump: parse and keep it (layout in `"multidump"` inside `osirus_parameterDescriptions.json`). v1 plays Singles only.
   - Parameter change `F0 00 20 33 01 dev <page 0x70|0x71> <part> <index> <value> F7`: apply live.
4. **Layout**: generate `VirusCLayout.h` at build time from `reference/virusC_single_256byte_map.csv`, with offset, name, min, max, default and toText.
   - Page A = bytes 0–127, Page B = 128–255.
   - Name = bytes 240–249 (ASCII; sanitise).
   - Version = byte 0 (Virus C = 7).
   - Category = bytes 251–252.
5. Value lists come from `reference/osirus_valuelists.json.txt`. Implement every `toText` formatter used in the CSV (`signed`, `pan`, `envSustainTime`, `oscShape`, `oscWave`, `musicDivision`, `filterKeytrackBase`, and the rest).
6. **Out-of-range values**: clamp to min/max and log `(patch, param, raw value)`. The factory data contains such values, e.g. Filter1 Mode 51/101, Saturation 42–50, Osc FM Mode 28–127. Keep the raw byte, so export round-trips exactly.
7. Export back to valid 267-byte SysEx and `.mid`. State save/restore = the raw 256 bytes.
8. Catch2 tests:
   - an SMF with 128 dumps imports 128 patches
   - checksum maths
   - names
   - byte-exact round-trip
   - TI dump fallback
   - OS-file rejection
   - fuzzed garbage never crashes

## 2. Engine: Virus C voice architecture
Parameter names below match the CSV. The behaviour notes are the initial hypothesis; calibration (section 5) has the final say.

**Oscillators 1 and 2**
- `Shape` 0–127 sweeps continuously: 0 = the selected **Wave** (`Wave Select`: 0 sine, 1 triangle, 2–63 spectral single-cycle waves); 64 = sawtooth; 127 = pulse (`Pulsewidth`). In between, crossfade/morph.
- Band-limited: PolyBLEP/minBLEP for saw and pulse; mip-mapped tables for the waves. Synthesise the spectral waves additively. Their harmonic content is a calibration target (section 5).
- Pitch:
  - `Semitone` 16–112, centred on 64
  - `Keyfollow` (96 = 100 %)
  - Osc2 `Detune`
  - `Osc2 Filt Env Amt`
  - LFO1 amounts
  - `Transpose`
  - `Bender Range Up/Down` and `Bender Scale` (lin/exp)
  - `Portamento Time`
- `Osc2 Sync` (hard sync to Osc1). FM via `Osc FM Mode` (Pos-Tri, Tri, Wave, Noise, input modes as no-ops) with `Osc2 FM Amount`, `FM Filt Env Amt`, `FM Lfo2 Amount` and `Fm Amount Velocity`.
- `Osc Init Phase`: 0 = free-running.

**Other sources**
- Osc3 (`Osc3 Mode`): Off, Slave (follows Osc2), Saw, Pulse, Sine, Triangle, Wave 3–64. Has Volume, Semitone and Detune.
- Sub oscillator: square or triangle one octave below Osc1.
- Noise with `Noise Color` tilt.
- `Ringmodulator Volume` (Osc1 × Osc2).
- Mix: `Osc Balance`, `Osc Mainvolume`. Above 64 drives the saturation stage.
- `Punch Intensity`: an attack transient on the amp envelope.

**Unison**
- `Unison Mode`: Off, Twin, 3–16 voices.
- `Unison Detune`, `Unison Pan Spread`, `Unison Lfo Phase`.

**Filters**
- Filter 1 modes: LP, HP, BP, BS (2-pole SVF), plus **Analog 1/2/3/4-pole** (ZDF ladder with self-oscillation).
- Filter 2 modes: LP, HP, BP, BS (SVF).
- Controls:
  - `Cutoff`, `Cutoff2`, and `Filter2 Cutoff Link`
  - Resonance 1 and 2
  - Env amounts with `Filter1/2 Env Polarity`
  - Keyfollow around `Filter Keytrack Base`
  - `Filter Balance`
  - Velocity → env amount and resonance
  - LFO1 → resonance and filter gain; LFO2 → cutoff 1 and 2
- `Filter Routing`:
  - Serial4: F1 → saturation → F2
  - Serial6: same, with F1 at 4 poles
  - Parallel4
  - Split: Osc1 → F1, Osc2 → F2
- **Saturation** (`Saturation Curve`, 15 curves): Off, Light, Soft, Middle, Hard, Digital, Shaper, Rectifier, BitReducer, RateReducer, Rate+Follow, LowPass, Low+Follow, HighPass, High+Follow. Oversample this stage.

**Envelopes** (filter and amp)
- ADSR plus **Sustain Time** slope: 0 = infinite; 1–63 = decay towards zero after the decay phase; 64 = "Fall"; 65–127 = rise.
- Exponential segments, with times from calibrated tables.
- `Amp Velocity` and `Panorama Velocity`.

**LFOs**
- LFO1 and LFO2: Rate, Shape (incl. waves), Env Mode, Mode (poly/mono), Symmetry, Keyfollow, Keytrigger, and Clock divisions synced to `Clock Tempo`.
- LFO1 fixed destinations: Osc1, Osc2, PW, Reso, FiltGain, plus Assign Dest/Amount.
- LFO2 fixed destinations: Shape, FM, Cutoff1, Cutoff2, Pan, plus Assign Dest/Amount.
- LFO3: Rate, Shape, Mode, Keyfollow, Fade-In, Clock, and Destination (Osc1, Osc1+2, Osc2, PW1, PW1+2, PW2) with Amount.

**Mod matrix**
- Assign1 → 1 destination, Assign2 → 2, Assign3 → 3, plus Assign 4, 5 and 6 (1 destination each).
- 28 sources (`modmatrixSource`). Destinations come from `modmatrixDest`.
- Amounts are bipolar, centred on 64.

**Voice and part**
- `Key Mode`: Poly, Mono 1–4, Hold, each with its own legato/retrigger behaviour.
- `Patch Volume`, `Panorama`.
- Up to 32 voices with voice stealing.
- Arpeggiator: Mode, Pattern, Octave Range, Note Length, Swing, Hold, Clock.

**FX chain** (order to verify by calibration)
1. Distortion: `Distortion Curve` with 12 curves, plus Intensity.
2. Bass boost: Intensity and Tune.
3. 3-band EQ: Low/High frequency and gain; Mid gain, frequency and Q.
4. Chorus: Mix, Rate, Depth, Delay, Feedback, LFO shape.
5. Phaser: 1–6 stages, Mix, Rate, Depth, Frequency, Feedback, Spread.
6. Delay/Reverb: `Delay/Reverb Mode`, with 27 modes (delay, reverb, reverb+feedback, ping-pong ratios, patterns). Controls: Effect Send, Time or Clock, Feedback, Rate/Decay, Depth, Damping, Color.

Ignore Input Mode/Select, the vocoder and Second Output in v1, but list them in an "ignored parameters" panel per patch.

Smoothing: about 1 ms ramps on continuous parameters.

## 3. Plugin UI
- Generated controls grouped by the `regions` section of the JSON.
- Preset browser: open or drag-drop `.mid`/`.syx` files, then list patches by name and category.
- Warnings panel: checksum, clamped values, ignored parameters.
- MIDI CC map taken from `controllerMap`.

## 4. `vc_render` CLI
`vc_render --preset bank.mid --index 5 --notes "60:0.0:2.0:100" --len 4 --sr 44100 --out x.wav [--set "Cutoff=80"]`
- Deterministic: fixed RNG seed.
- Can also dump the parsed patch to JSON.

## 5. Calibration against Osirus (how "close to 1:1" is reached)
Build `Calib/` in Python 3.11 with `dawdreamer` or `pedalboard`. Host **Osirus.vst3 (the user loads their own Virus C ROM in it)** next to VC-Clone. The tooling only renders audio and measures it. It never opens the ROM.

1. **Probe patches.** Generate init SysEx single dumps that isolate one block each, for example:
   - one saw, filter open, FX off
   - amp envelope A0 D0 S127 R0

   Send each to the edit buffer (bank 0) before rendering.
2. **Sweeps** over each value 0..127:
   - Cutoff/resonance: frequency response measured with noise or a log sweep through the filter (oscillators off, noise on). Fit cutoff Hz and resonance peak/Q per filter mode. Find the self-oscillation threshold.
   - Envelope A/D/R and Sustain Time: time constants and curve shape.
   - LFO rates per shape, plus clocked rates.
   - Pitch, detune and keyfollow in cents (YIN tracker).
   - Shape morph and the 64 waves: single-cycle FFT harmonic magnitudes → additive tables.
   - Saturation and distortion curves: static transfer function (sine-amplitude sweep → LUT).
   - Velocity and mod-matrix scaling.
   - Unison detune and spread.
   - Portamento.
   - Chorus, phaser and delay timings; reverb decay (RT60).
3. Code-generate fitted tables into `Engine/Tables/`. Re-run the sweeps on VC-Clone and assert:
   - pitch within ±1 cent
   - cutoff within ±3 %
   - envelope times within ±5 %
   - levels within ±0.5 dB
4. **Patch A/B.** Render the user's imported `.mid` patches through both plugins: held C3, a chord and a short sequence. Report:
   - multi-resolution STFT distance
   - LUFS delta
   - spectral-centroid trajectory
   - envelope correlation

   Produce an HTML report ranked worst-first, and iterate on the worst blocks until the median score plateaus. Also test the out-of-range value cases from section 1.6.
5. **1:1 regression suite.** Keep every probe patch, every sweep and the user's patch set as automated tests, run with `ctest` or `pytest`. Each test renders Osirus and VC-Clone and checks the tolerances. Cache the Osirus renders with a hash of preset + MIDI + settings, so reruns are fast; re-render when the Osirus version changes. Each run prints a summary table: block | tests | pass | fail | worst error. CI fails on any regression against the last passing results.
6. **Per-patch 1:1 score.** For each user patch, report:
   - pitch error (cents)
   - level error (dB)
   - STFT distance
   - envelope correlation
   - null residual (dB)

   Each patch gets PASS/FAIL against agreed thresholds. Start with: null residual ≤ −30 dB on deterministic probe patches, and STFT distance below a threshold set from Osirus-vs-Osirus repeat renders, which give the noise floor. Track the pass rate over time in the HTML report.
7. **Listening A/B.** Add a "blind A/B" page to the HTML report. It shows both renders of each patch in random order and records which one the listener thinks is the original, for the user's own ear test.

## 6. Order of work
1. PresetIO + tests + `vc_render` JSON dump.
2. Osc, filters, envelopes, VCA.
3. LFOs, mod matrix, key modes, unison, arpeggiator.
4. Saturation, then the FX chain.
5. Calibration loop + A/B report.
6. VST3/Standalone packaging.

The 1:1 harness (section 5, steps 1, 5 and 6) is built **in stage 1**, alongside PresetIO. Every later stage adds its own 1:1 tests before it counts as done.

After each stage:
1. Build, and run the unit tests and the full 1:1 suite against Osirus.
2. Report the pass/fail table and the worst-matching patches, with numbers.
3. State which behaviours are measured and still need work, and which are still guesses.
