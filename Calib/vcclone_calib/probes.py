"""Probe patches, parameter sweeps and patch A/B jobs.

Every test is a pair of renders (Osirus and VC-Clone) of the same Job, plus a metric that
is measured on both and compared against a tolerance (TOLERANCES below; never loosened
silently: change them here and the report shows the values used).
"""
from __future__ import annotations

from dataclasses import dataclass, field

from .hosts import Job, Note
from .sysex import Single

# ----------------------------------------------------------------------------------------
TOLERANCES = {
    'pitch_cents': 1.0,        # pitch within +-1 cent
    'cutoff_pct': 3.0,         # cutoff within +-3 %
    'env_pct': 5.0,            # envelope times within +-5 %
    'level_db': 0.5,           # levels within +-0.5 dB
    'lfo_pct': 3.0,            # LFO / modulation rates
    'harm_db': 1.0,            # rms dB difference of the strongest harmonics
    'fx_time_pct': 5.0,        # delay times, RT60
    'curve_pct': 10.0,         # saturation fingerprint (THD)
    'null_db': -30.0,          # null residual on deterministic probes
    'ltas_db': 1.0,            # long-term spectrum (free-running chorus/phaser/unison/rate reducer)
}

C3 = 60   # Virus/Osirus "C3" = MIDI note 60


def init_patch() -> Single:
    """Neutral probe base. Mirrors PresetIO::makeInitPatch (verified by tests/test_selfcheck.py)."""
    p = Single()
    raw = p.raw
    for o in range(256):
        raw[o] = 0
    vals = {
        'Version': 7, 'Channel Volume': 127, 'Expression': 127, 'Osc1 Shape': 64, 'Osc2 Shape': 64,
        'Osc1 Semitone': 64, 'Osc2 Semitone': 64, 'Osc3 Semitone': 64, 'Osc1 Keyfollow': 96, 'Osc2 Keyfollow': 96,
        'Osc2 Filt Env Amt': 64, 'FM Filt Env Amt': 64, 'Osc Balance': 0, 'Osc Mainvolume': 64, 'Noise Color': 64,
        'Cutoff': 127, 'Cutoff2': 127, 'Filter1 Keyfollow': 64, 'Filter2 Keyfollow': 64, 'Filter Balance': 0,
        'Filter Keytrack Base': 60, 'Filter Env Sustain': 127, 'Filter Env Sustain Time': 64,
        'Amp Env Sustain': 127, 'Amp Env Sustain Time': 64, 'Patch Volume': 100, 'Panorama': 64,
        'Transpose': 64, 'Lfo1 Rate': 64, 'Lfo2 Rate': 64, 'Lfo3 Rate': 64, 'Lfo1 Symmetry': 64, 'Lfo2 Symmetry': 64,
        'Osc1 Lfo1 Amount': 64, 'Osc2 Lfo1 Amount': 64, 'PW Lfo1 Amount': 64, 'Reso Lfo1 Amount': 64,
        'FiltGain Lfo1 Amount': 64, 'Shape Lfo2 Amount': 64, 'FM Lfo2 Amount': 64, 'Cutoff1 Lfo2 Amount': 64,
        'Cutoff2 Lfo2 Amount': 64, 'Pan Lfo2 Amount': 64, 'Unison Detune': 48, 'Unison Pan Spread': 127,
        'Unison Lfo Phase': 64, 'Chorus Feedback': 64, 'Delay Color': 64, 'Clock Tempo': 57,
        'Bender Range Up': 66, 'Bender Range Down': 62, 'Bender Scale': 1, 'Filter1 Env Polarity': 1,
        'Filter2 Env Polarity': 1, 'Arp Note Length': 64, 'Arp Clock': 4, 'Osc3 Volume': 64,
        'LowEQ Frequency': 43, 'HighEQ Frequency': 85, 'MidEQ Frequency': 71, 'MidEQ Q-Factor': 51,
        'LowEQ Gain': 64, 'MidEQ Gain': 64, 'HighEQ Gain': 64, 'Bass Tune': 32, 'Phaser Feedback': 64,
        'Control Smooth Mode': 1,
        'Osc1 Shape Velocity': 64, 'Osc2 Shape Velocity': 64, 'PulseWidth Velocity': 64, 'Fm Amount Velocity': 64,
        'Flt1 EnvAmt Velocity': 64, 'Flt2 EnvAmt Velocity': 64, 'Resonance1 Velocity': 64, 'Resonance2 Velocity': 64,
        'Amp Velocity': 64, 'Panorama Velocity': 64, 'Assign1 Amount': 64, 'Assign2 Amount1': 64,
        'Assign2 Amount2': 64, 'Assign3 Amount1': 64, 'Assign3 Amount2': 64, 'Assign3 Amount3': 64,
        'LFO1 Assign Amount': 64, 'LFO2 Assign Amount': 64, 'Assign 4 Amount': 64, 'Assign 5 Amount': 64,
        'Assign 6 Amount': 64, 'Arp Note Length': 64, 'Bender Range Up': 66,
        'Osc Init Phase': 1,                 # fixed start phase in every probe (deterministic)
    }
    for k, v in vals.items():
        p.set(k, v)
    p.set_name('Probe')
    return p


@dataclass
class Test:
    id: str
    block: str
    job: Job
    metric: str                       # see suite.METRICS
    deterministic: bool = False       # also run the null test
    companion: Job | None = None      # e.g. the open-filter noise reference for cutoff fits
    info: dict = field(default_factory=dict)


def _job(p: Single, notes, length, **kw) -> Job:
    return Job(patch=p, notes=notes, length=length, **kw)


def _values(full: bool, lo=0, hi=127, step=8):
    return list(range(lo, hi + 1)) if full else sorted(set(list(range(lo, hi + 1, step)) + [hi]))


def build_probes(full: bool = False) -> list[Test]:
    tests: list[Test] = []
    base = init_patch()

    # --- basic probes (deterministic) ----------------------------------------------------
    p = base.copy()
    tests.append(Test('probe/saw_open', 'probe', _job(p, [Note(C3, 0.0, 1.0)], 1.2), 'level', True))
    tests.append(Test('probe/saw_pitch', 'pitch', _job(p, [Note(69, 0.0, 1.0)], 1.0), 'pitch', True, info={'expect_hz': 440.0}))
    p2 = base.copy().set('Osc1 Shape', 127)
    tests.append(Test('probe/pulse', 'probe', _job(p2, [Note(C3, 0.0, 1.0)], 1.2), 'level', True))
    p3 = base.copy().set('Osc1 Shape', 0)
    tests.append(Test('probe/sine', 'probe', _job(p3, [Note(C3, 0.0, 1.0)], 1.2), 'level', True))

    # --- pitch -------------------------------------------------------------------------
    for v in _values(full, 16, 112, 8):
        q = base.copy().set('Osc1 Semitone', v)
        tests.append(Test(f'pitch/osc1_semitone/{v}', 'pitch', _job(q, [Note(72, 0, 0.8)], 0.8), 'pitch', True, info={'param': 'Osc1 Semitone', 'value': v}))
    for v in _values(full):
        q = base.copy().set('Osc Balance', 127).set('Osc2 Detune', v)
        tests.append(Test(f'pitch/osc2_detune/{v}', 'pitch', _job(q, [Note(C3, 0, 0.8)], 0.8), 'pitch', True, info={'param': 'Osc2 Detune', 'value': v}))
    for v in _values(full, 0, 127, 16):
        q = base.copy().set('Osc1 Keyfollow', v)
        tests.append(Test(f'pitch/osc1_keyfollow/{v}', 'pitch', _job(q, [Note(72, 0, 0.8)], 0.8), 'pitch', True, info={'param': 'Osc1 Keyfollow', 'value': v}))

    # --- filters: noise through the filter vs the same noise with the filter open --------------
    # oscillators muted via Osc Volume = 0; noise has its own level (hypothesis, see report)
    noise = base.copy().set('Osc Mainvolume', 0).set('Noise Volume', 127)
    noise_ref = noise.copy()
    ref_job = _job(noise_ref, [Note(C3, 0, 4.0)], 4.0, seed=11)
    for mode, mname, fit in ((0, 'svf_lp', 'lp'), (1, 'svf_hp', 'hp'), (7, 'analog4_lp', 'lp4')):
        for v in _values(full, 24, 120, 8):
            q = noise.copy().set('Filter1 Mode', mode).set('Cutoff', v).set('Filter Balance', 0)
            tests.append(Test(f'filter/{mname}/cutoff/{v}', 'filter', _job(q, [Note(C3, 0, 4.0)], 4.0, seed=11), 'cutoff',
                              companion=ref_job, info={'param': 'Cutoff', 'value': v, 'mode': fit, 'filter_mode': mode}))
    for v in _values(full, 0, 112, 16):
        q = noise.copy().set('Filter1 Mode', 0).set('Cutoff', 64).set('Filter1 Resonance', v)
        tests.append(Test(f'filter/svf_lp/reso/{v}', 'filter', _job(q, [Note(C3, 0, 4.0)], 4.0, seed=11), 'resonance',
                          companion=ref_job, info={'param': 'Filter1 Resonance', 'value': v, 'mode': 'lp'}))

    # --- envelopes --------------------------------------------------------------------------
    for v in _values(full, 0, 127, 8):
        if v == 0:
            continue        # instant segments are covered by the level probes
        q = base.copy().set('Amp Env Attack', v)
        tests.append(Test(f'env/amp_attack/{v}', 'envelope', _job(q, [Note(C3, 0, 12.0)], 12.0 if v > 100 else 4.0), 'attack',
                          True, info={'param': 'Amp Env Attack', 'value': v}))
        q = base.copy().set('Amp Env Decay', v).set('Amp Env Sustain', 0)
        tests.append(Test(f'env/amp_decay/{v}', 'envelope', _job(q, [Note(C3, 0, 12.0)], 12.0 if v > 90 else 4.0), 'decay',
                          True, info={'param': 'Amp Env Decay', 'value': v}))
        q = base.copy().set('Amp Env Release', v)
        tests.append(Test(f'env/amp_release/{v}', 'envelope', _job(q, [Note(C3, 0, 0.5)], 12.0 if v > 90 else 4.0), 'release',
                          True, info={'param': 'Amp Env Release', 'value': v, 'start': 0.5}))
    for v in (0, 16, 32, 48, 63, 64, 65, 80, 96, 112, 127):
        q = base.copy().set('Amp Env Sustain', 64).set('Amp Env Sustain Time', v)
        tests.append(Test(f'env/amp_sustain_time/{v}', 'envelope', _job(q, [Note(C3, 0, 6.0)], 6.0), 'sustain_slope',
                          True, info={'param': 'Amp Env Sustain Time', 'value': v}))

    # --- LFO rate (LFO1 -> filter gain = amplitude modulation) -------------------------------
    for v in _values(full, 0, 127, 8):
        q = base.copy().set('Lfo1 Rate', v).set('FiltGain Lfo1 Amount', 127).set('Lfo1 Shape', 0)
        dur = 30.0 if v < 24 else 8.0
        tests.append(Test(f'lfo/lfo1_rate/{v}', 'lfo', _job(q, [Note(C3, 0, dur)], dur), 'lfo_rate',
                          info={'param': 'Lfo1 Rate', 'value': v}))

    # --- shape morph and waves (harmonic content) ---------------------------------------------
    for v in _values(full, 0, 127, 8):
        q = base.copy().set('Osc1 Shape', v).set('Osc1 Wave Select', 0)
        tests.append(Test(f'osc/shape/{v}', 'osc', _job(q, [Note(48, 0, 1.0)], 1.0), 'harmonics', True,
                          info={'param': 'Osc1 Shape', 'value': v, 'f0': 130.8128}))
    for w in (range(64) if full else range(0, 64, 3)):
        q = base.copy().set('Osc1 Shape', 0).set('Osc1 Wave Select', w)
        tests.append(Test(f'osc/wave/{w}', 'waves', _job(q, [Note(48, 0, 1.0)], 1.0), 'harmonics', True,
                          info={'param': 'Osc1 Wave Select', 'value': w, 'f0': 130.8128}))

    # --- saturation / distortion curves (driven sine) ------------------------------------------
    for c in range(1, 15):
        q = base.copy().set('Osc1 Shape', 0).set('Osc Mainvolume', 127).set('Saturation Curve', c)
        tests.append(Test(f'sat/curve/{c}', 'saturation', _job(q, [Note(48, 0, 1.0)], 1.0), 'curve', c not in (9, 10),
                          info={'param': 'Saturation Curve', 'value': c}))
    for c in range(1, 12):
        q = base.copy().set('Osc1 Shape', 0).set('Distortion Curve', c).set('Distortion Intensity', 100)
        tests.append(Test(f'dist/curve/{c}', 'distortion', _job(q, [Note(48, 0, 1.0)], 1.0), 'curve', c != 9,
                          info={'param': 'Distortion Curve', 'value': c}))

    # --- levels / velocity / mix --------------------------------------------------------------
    for v in (0, 32, 64, 96, 127):
        q = base.copy().set('Osc Mainvolume', v)
        tests.append(Test(f'level/osc_mainvolume/{v}', 'level', _job(q, [Note(C3, 0, 1.0)], 1.0), 'level', True, info={'param': 'Osc Mainvolume', 'value': v}))
        q = base.copy().set('Patch Volume', v)
        tests.append(Test(f'level/patch_volume/{v}', 'level', _job(q, [Note(C3, 0, 1.0)], 1.0), 'level', True, info={'param': 'Patch Volume', 'value': v}))
        q = base.copy().set('Amp Velocity', v)
        tests.append(Test(f'velocity/amp/{v}', 'velocity', _job(q, [Note(C3, 0, 1.0, 40)], 1.0), 'level', True, info={'param': 'Amp Velocity', 'value': v}))
        q = base.copy().set('Osc Balance', v).set('Osc2 Shape', 127)
        tests.append(Test(f'level/osc_balance/{v}', 'level', _job(q, [Note(C3, 0, 1.0)], 1.0), 'level', True, info={'param': 'Osc Balance', 'value': v}))

    # --- unison, portamento --------------------------------------------------------------------
    for v in (0, 32, 64, 96, 127):
        q = base.copy().set('Unison Mode', 1).set('Unison Detune', v)
        tests.append(Test(f'unison/detune/{v}', 'unison', _job(q, [Note(C3, 0, 2.0)], 2.0), 'ltas', info={'param': 'Unison Detune', 'value': v}))
    for v in (16, 48, 80, 112):
        q = base.copy().set('Portamento Time', v).set('Key Mode', 1)
        tests.append(Test(f'porta/time/{v}', 'portamento', _job(q, [Note(48, 0, 0.5), Note(72, 0.5, 2.5)], 3.0), 'glide',
                          True, info={'param': 'Portamento Time', 'value': v}))

    # --- effects timing ------------------------------------------------------------------------
    pluck = base.copy().set('Amp Env Sustain', 0).set('Amp Env Decay', 10)
    for v in (16, 48, 64, 80, 112):
        q = pluck.copy().set('Delay/Reverb Mode', 1).set('Effect Send', 127).set('Delay Time', v).set('Delay Feedback', 0)
        tests.append(Test(f'fx/delay_time/{v}', 'fx', _job(q, [Note(C3, 0, 0.05)], 2.0), 'echo_delay', info={'param': 'Delay Time', 'value': v}))
    for v in (16, 48, 80, 112):
        q = pluck.copy().set('Delay/Reverb Mode', 2).set('Effect Send', 127).set('Dly Rate / Rev Decay', v)
        tests.append(Test(f'fx/reverb_decay/{v}', 'fx', _job(q, [Note(C3, 0, 0.05)], 8.0), 'rt60', info={'param': 'Rev Decay', 'value': v}))
    for v in (16, 64, 112):
        q = base.copy().set('Chorus Mix', 127).set('Chorus Rate', v).set('Chorus Depth', 127)
        tests.append(Test(f'fx/chorus_rate/{v}', 'fx', _job(q, [Note(C3, 0, 6.0)], 6.0), 'ltas', info={'param': 'Chorus Rate', 'value': v}))
        q = base.copy().set('Phaser Mode', 3).set('Phaser Mix', 127).set('Phaser Rate', v).set('Phaser Depth', 127)
        tests.append(Test(f'fx/phaser_rate/{v}', 'fx', _job(q, [Note(C3, 0, 6.0)], 6.0), 'ltas', info={'param': 'Phaser Rate', 'value': v}))
    return tests


def build_patch_tests(singles: list, source: str) -> list[Test]:
    """Held C3, a chord and a short sequence for every user patch."""
    tests = []
    seq = [Note(n, i * 0.25, 0.2, 100) for i, n in enumerate([48, 55, 60, 63, 67, 70, 72, 60])]
    for i, s in enumerate(singles):
        name = f'{source}#{i:03d} {s.name}'
        tests.append(Test(f'patch/{name}/held', 'patch', _job(s, [Note(C3, 0, 2.0)], 3.0), 'patch', info={'patch': name, 'index': i}))
        tests.append(Test(f'patch/{name}/chord', 'patch', _job(s, [Note(n, 0, 2.0) for n in (60, 64, 67, 71)], 3.0), 'patch', info={'patch': name, 'index': i}))
        tests.append(Test(f'patch/{name}/sequence', 'patch', _job(s, seq, 3.0), 'patch', info={'patch': name, 'index': i}))
    return tests


def build_out_of_range_tests() -> list[Test]:
    """The factory oddities from section 1.6, as standalone probes."""
    base = init_patch()
    cases = [('Filter1 Mode', 51), ('Filter1 Mode', 101), ('Saturation Curve', 42), ('Saturation Curve', 49),
             ('Saturation Curve', 50), ('Osc FM Mode', 28), ('Osc FM Mode', 115), ('Osc FM Mode', 127)]
    tests = []
    for name, v in cases:
        q = base.copy().set(name, v).set('Osc2 FM Amount', 60 if 'FM' in name else 0).set('Cutoff', 80).set('Osc Mainvolume', 100)
        tests.append(Test(f'oor/{name.replace(" ", "_")}/{v}', 'out_of_range', _job(q, [Note(C3, 0, 1.0)], 1.5), 'patch',
                          info={'param': name, 'value': v}))
    return tests
