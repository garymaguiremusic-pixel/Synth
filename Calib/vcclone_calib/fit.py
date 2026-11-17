"""Fits 128-entry tables from the Osirus side of a sweep run and writes Engine/Tables/Measured.h.

Only quantities that were actually measured get a VC_MEASURED_* define; everything else
keeps its documented guess in Tables.h. Re-run the suite afterwards: the sweeps then
compare the fitted VC-Clone against Osirus.
"""
from __future__ import annotations

import json
import math
import os
import re

import numpy as np

from .sysex import REPO

OUT = os.path.join(REPO, 'Engine', 'Tables', 'Measured.h')


def _series(results, prefix):
    pts = {}
    for r in results:
        if r['id'].startswith(prefix) and r.get('ref') is not None and isinstance(r['ref'], (int, float)):
            v = r['info'].get('value')
            if v is not None and math.isfinite(r['ref']):
                pts[int(v)] = float(r['ref'])
    return pts


def _interp128(pts: dict, log=False, lo=0, hi=127):
    if len(pts) < 2:
        return None
    xs = np.array(sorted(pts))
    ys = np.array([pts[x] for x in xs])
    if log:
        ys = np.log(np.maximum(ys, 1e-9))
    full = np.interp(np.arange(128), xs, ys)
    # linear extrapolation outside the measured range
    if xs[0] > lo:
        slope = (ys[1] - ys[0]) / (xs[1] - xs[0])
        full[:xs[0]] = ys[0] + slope * (np.arange(xs[0]) - xs[0])
    if xs[-1] < hi:
        slope = (ys[-1] - ys[-2]) / (xs[-1] - xs[-2])
        idx = np.arange(xs[-1] + 1, 128)
        full[idx] = ys[-1] + slope * (idx - xs[-1])
    return np.exp(full) if log else full


def _arr(name, values, fmt='{:.6g}'):
    body = ', '.join(fmt.format(float(v)) for v in values)
    return f'inline constexpr double {name}[{len(values)}] = {{ {body} }};'


def fit(results_path: str, out_path: str = OUT, log=print):
    results = json.load(open(results_path))['results']
    tables, defines, notes = [], [], []

    cut = _series(results, 'filter/svf_lp/cutoff/')
    t = _interp128(cut, log=True, lo=8)
    if t is not None:
        tables.append(_arr('kCutoffHz', t)); defines.append('VC_MEASURED_CUTOFF')
        notes.append(f'cutoff: {len(cut)} points')

    att = {k: v / 0.8 for k, v in _series(results, 'env/amp_attack/').items() if v > 0}   # 10-90 % of a linear ramp
    t = _interp128(att, log=True)
    if t is not None:
        t[0] = 0.0
        tables.append(_arr('kAttack', t)); defines.append('VC_MEASURED_ATTACK')
    dec = {k: v for k, v in _series(results, 'env/amp_decay/').items() if 0 < v < 1e4}
    t = _interp128(dec, log=True)
    if t is not None:
        tables.append(_arr('kDecay', t)); defines.append('VC_MEASURED_DECAY')
    rel = {k: v for k, v in _series(results, 'env/amp_release/').items() if 0 < v < 1e4}
    t = _interp128(rel, log=True)
    if t is not None:
        tables.append(_arr('kRelease', t)); defines.append('VC_MEASURED_RELEASE')

    # sustain time: measured, but modelled as a dB/s law in Tables.h (sustainSlopeDbPerSec)

    lfo = _series(results, 'lfo/lfo1_rate/')
    pts = sorted((v, r) for v, r in lfo.items() if 0.5 <= r <= 15.0)
    good, last = [], 0.0
    for v, r in pts:
        if r > last:
            good.append((v, r)); last = r
    if len(good) >= 4:
        xs = np.array([g[0] for g in good], dtype=float)
        ys = np.log(np.array([g[1] for g in good]))
        b, a = np.polyfit(xs, ys, 1)
        t = np.exp(a + b * np.arange(128))
        tables.append(_arr('kLfoRate', t)); defines.append('VC_MEASURED_LFO_RATE')
        notes.append(f'lfo rate: log-linear fit over {len(good)} points ({good[0][0]}..{good[-1][0]}), doubles every {np.log(2) / b:.1f} steps')

    det = _series(results, 'pitch/osc2_detune/')
    if det:
        base = det.get(0)
        if base:
            cents = {k: 1200 * math.log2(v / base) for k, v in det.items() if v > 0}
            # noisy pitch readings: fit cents = c * (v/127)^p (least squares in log space, v >= 16)
            pts = [(k, c) for k, c in cents.items() if k >= 16 and c > 0.05]
            if len(pts) >= 4:
                xs = np.log(np.array([k / 127.0 for k, _ in pts])); ys = np.log(np.array([c for _, c in pts]))
                pexp, logc = np.polyfit(xs, ys, 1)
                t = np.exp(logc) * (np.arange(128) / 127.0) ** pexp
                tables.append(_arr('kDetuneCents', t)); defines.append('VC_MEASURED_DETUNE')
                notes.append(f'detune: cents = {np.exp(logc):.1f} * (v/127)^{pexp:.2f}')

    waves = {}
    for r in results:
        m = re.match(r'osc/wave/(\d+)$', r['id'])
        if m and r.get('harm_ref'):
            waves[int(m.group(1))] = r['harm_ref']
    if len(waves) == 64:
        rows = []
        for w in range(64):
            h = list(waves[w]) + [0.0] * (64 - len(waves[w]))
            rows.append('{ ' + ', '.join(f'{x:.6e}f' for x in h[:64]) + ' }')
        tables.append('inline constexpr float kWaveHarmonics[64][64] = {\n    ' + ',\n    '.join(rows) + ' };')
        defines.append('VC_MEASURED_WAVES')
    elif waves:
        notes.append(f'waves: only {len(waves)}/64 measured (run the sweep with --full to fit them)')

    lines = ['// GENERATED by Calib (python -m vcclone_calib fit) from Osirus measurements. Do not edit.',
             f'// source: {os.path.relpath(results_path, REPO)}', '#pragma once', '#include <algorithm>', '#include <cmath>', '']
    lines += [f'#define {d} 1' for d in defines]
    lines += ['', 'namespace vc::tables::measured', '{',
              'template <size_t N> inline double interp (const double (&t)[N], double v)',
              '{', '    v = std::clamp (v, 0.0, (double) (N - 1));',
              '    const size_t i = std::min ((size_t) v, N - 2);', '    const double f = v - (double) i;',
              '    return t[i] + (t[i + 1] - t[i]) * f;', '}']
    lines += tables
    lines += ['} // namespace vc::tables::measured', '']
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, 'w', newline='\n') as f:
        f.write('\n'.join(lines))
    log(f'wrote {out_path}: {", ".join(defines) or "nothing measured"}')
    for n in notes:
        log('  ' + n)
    return defines
