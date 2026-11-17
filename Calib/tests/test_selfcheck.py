"""Checks the harness itself (no Osirus needed): VC-Clone is compared against itself."""
import os
import subprocess
import tempfile

import numpy as np

from vcclone_calib import analysis as A
from vcclone_calib import probes as P
from vcclone_calib.hosts import Job, Note, VcRenderCli
from vcclone_calib.suite import Cache, run_tests, summarize
from vcclone_calib.sysex import Single, load_singles


def test_python_init_patch_matches_cpp(vc_render):
    with tempfile.TemporaryDirectory() as td:
        out = os.path.join(td, 'init.syx')
        subprocess.run([vc_render, '--syx', out], check=True, capture_output=True)
        cpp = load_singles(out)[0]
    py = P.init_patch()
    diff = [o for o in range(256) if not 240 <= o < 250 and o != 163 and cpp.raw[o] != py.raw[o]]
    assert diff == [], f'offsets differ: {diff}'


def test_dump_checksum_roundtrip():
    s = P.init_patch().set('Cutoff', 77)
    d = s.dump()
    assert len(d) == 267 and d[-1] == 0xF7 and d[-2] == sum(d[5:265]) & 0x7F
    with tempfile.TemporaryDirectory() as td:
        path = os.path.join(td, 'x.syx')
        open(path, 'wb').write(d + d)
        back = load_singles(path)
    assert len(back) == 2 and back[0].raw == s.raw


def test_analysis_basics():
    sr = 44100
    t = np.arange(sr) / sr
    x = 0.5 * np.sin(2 * np.pi * 440 * t)
    assert abs(A.cents(A.yin_f0(x, sr), 440.0)) < 0.5
    assert abs(A.level_db(x) - (20 * np.log10(0.5 / np.sqrt(2)))) < 0.01
    y = np.roll(x, 37)
    assert A.find_lag(x, y) == 37
    rng = np.random.default_rng(1)
    n = rng.standard_normal(sr * 2)
    from scipy import signal
    b, a = signal.butter(2, 1000 / (sr / 2))
    f, h = A.transfer_db(signal.lfilter(b, a, n), n, sr)
    assert abs(A.fit_cutoff(f, h, 'lp')['cutoff_hz'] / 1000 - 1) < 0.05
    assert abs(A.modulation_rate((1 + 0.5 * np.sin(2 * np.pi * 3 * np.arange(sr * 4) / sr)) * rng.standard_normal(sr * 4), sr) - 3) < 0.2


def test_self_comparison_passes(vc_render, tmp_path):
    """VC-Clone vs itself must pass every metric (null = silence)."""
    r = VcRenderCli(vc_render)
    tests = [t for t in P.build_probes() if t.id in ('probe/saw_open', 'probe/saw_pitch', 'pitch/osc1_semitone/72',
                                                       'filter/svf_lp/cutoff/64', 'env/amp_attack/64', 'lfo/lfo1_rate/64',
                                                       'osc/wave/9', 'level/patch_volume/64', 'sat/curve/3')]
    assert len(tests) == 9
    res = run_tests(tests, r, r, str(tmp_path), Cache(str(tmp_path / 'cache')), log=None)
    bad = [(x['id'], x.get('notes')) for x in res if not x['pass']]
    assert bad == []
    blocks = summarize(res)
    assert sum(b['tests'] for b in blocks.values()) == 9


def test_vcclone_pitch_is_exact(vc_render):
    r = VcRenderCli(vc_render)
    x = r.render(Job(P.init_patch(), [Note(69, 0, 1.0)], 1.0))
    assert abs(A.cents(A.yin_f0(x[4410:], 44100), 440.0)) < 1.0
