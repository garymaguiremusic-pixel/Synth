"""Runs the 1:1 suite: renders Osirus (cached) and VC-Clone for every test, measures,
compares against TOLERANCES and writes results/<run>/results.json + WAVs."""
from __future__ import annotations

import json
import math
import os
import time

import numpy as np
import soundfile as sf

from . import analysis as A
from .hosts import Job, Renderer
from .probes import TOLERANCES, Test

# ----------------------------------------------------------------------------------------
# metric -> (measure(audio, sr, test, companion_audio) -> value, compare(ref, ours) -> (error, unit, pass))


def _pct(ref, ours):
    if not (np.isfinite(ref) and np.isfinite(ours)):
        return (float('nan'), '%')
    if ref == 0:
        return (0.0 if ours == 0 else float('inf'), '%')
    return (100.0 * (ours - ref) / abs(ref), '%')


def m_pitch(x, sr, t, comp):
    return A.yin_f0(x[int(0.15 * sr):], sr, fmin=25.0, fmax=10000.0)


def m_level(x, sr, t, comp):
    return A.level_db(x[int(0.05 * sr):])


def m_cutoff(x, sr, t, comp):
    f, h = A.transfer_db(x, comp, sr)
    return A.fit_cutoff(f, h, t.info.get('mode', 'lp'))['cutoff_hz']


def m_resonance(x, sr, t, comp):
    f, h = A.transfer_db(x, comp, sr)
    return A.fit_cutoff(f, h, t.info.get('mode', 'lp'))['peak_db']


def m_attack(x, sr, t, comp):
    return A.attack_time(x, sr)


def m_decay(x, sr, t, comp):
    return A.decay_time(x, sr)


def m_release(x, sr, t, comp):
    return A.decay_time(x, sr, start_s=t.info.get('start', 0.5) - 0.01)


def m_sustain_slope(x, sr, t, comp):
    e = A.envelope_db(x, sr, 10.0)
    a, b = e[int(1.0 / 0.01)], e[int(5.0 / 0.01)]
    return float(b - a)            # dB change over 4 s of sustain


def m_lfo_rate(x, sr, t, comp):
    return A.modulation_rate(x, sr)


def m_harmonics(x, sr, t, comp):
    seg = x[int(0.2 * sr):]
    return A.harmonics(seg, sr, t.info.get('f0', 130.8128), 32).tolist()


def m_curve(x, sr, t, comp):
    return A.static_curve(x[int(0.2 * sr):], sr)['thd']


def m_glide(x, sr, t, comp):
    """Seconds after the second note until the pitch is within 50 cents of the target."""
    target = 523.2511
    hop = int(0.01 * sr)
    start = int(0.5 * sr)
    for k in range(start, len(A.mono(x)) - 4 * hop, hop):
        f = A.yin_f0(x[k:k + 4 * hop], sr, fmin=80, fmax=2000)
        if np.isfinite(f) and abs(A.cents(f, target)) < 50:
            return (k - start) / sr
    return float('nan')


def m_echo(x, sr, t, comp):
    """Time between the dry pluck and the first echo (onsets in the 1 ms envelope)."""
    from scipy import signal as S
    e = A.envelope_db(x, sr, 1.0)
    e = np.maximum(e, e.max() - 60)
    peaks, _ = S.find_peaks(e, prominence=6.0, distance=5)
    if len(peaks) < 2:
        return float('nan')
    return (peaks[1] - peaks[0]) * 0.001


def m_ltas(x, sr, t, comp):
    return A.ltas_bands(x, sr).tolist()


def m_rt60(x, sr, t, comp):
    return A.decay_time(x, sr, start_s=0.1)


METRICS = {
    'pitch': (m_pitch, lambda r, o: (A.cents(o, r) if r > 0 and o > 0 else float('nan'), 'cents'), 'pitch_cents'),
    'level': (m_level, lambda r, o: (o - r, 'dB'), 'level_db'),
    'cutoff': (m_cutoff, _pct, 'cutoff_pct'),
    'resonance': (m_resonance, lambda r, o: (o - r, 'dB'), 'level_db'),
    'attack': (m_attack, _pct, 'env_pct'),
    'decay': (m_decay, _pct, 'env_pct'),
    'release': (m_release, _pct, 'env_pct'),
    'sustain_slope': (m_sustain_slope, lambda r, o: (o - r, 'dB'), 'level_db'),
    'lfo_rate': (m_lfo_rate, _pct, 'lfo_pct'),
    'harmonics': (m_harmonics, None, 'harm_db'),
    'ltas': (m_ltas, None, 'ltas_db'),
    'curve': (m_curve, _pct, 'curve_pct'),
    'glide': (m_glide, _pct, 'env_pct'),
    'echo_delay': (m_echo, _pct, 'fx_time_pct'),
    'rt60': (m_rt60, _pct, 'fx_time_pct'),
    'stft': (None, None, None),
    'patch': (None, None, None),
}


def harmonics_error(r, o):
    r, o = np.asarray(r), np.asarray(o)
    mask = r > 10 ** (-40 / 20)
    if not mask.any():
        return 0.0
    d = 20 * np.log10(o[mask] + 1e-6) - 20 * np.log10(r[mask] + 1e-6)
    return float(np.sqrt(np.mean(d * d)))


def ltas_error(r, o):
    r, o = np.asarray(r), np.asarray(o)
    mask = r > r.max() - 50          # bands within 50 dB of the loudest
    d = o[mask] - r[mask]
    return float(np.sqrt(np.mean(d * d))) if mask.any() else 0.0


# ----------------------------------------------------------------------------------------
class Cache:
    """Osirus renders cached by hash(renderer id + preset + MIDI + settings)."""

    def __init__(self, folder):
        self.folder = folder
        os.makedirs(folder, exist_ok=True)

    def get(self, renderer: Renderer, job: Job, salt: str = ''):
        path = os.path.join(self.folder, job.key(renderer.id + salt) + '.wav')
        if os.path.exists(path):
            data, _ = sf.read(path, dtype='float32', always_2d=True)
            return data
        data = renderer.render(job)
        sf.write(path, data, job.sr, subtype='FLOAT')
        return data


def _write(path, data, sr):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sf.write(path, np.asarray(data, dtype=np.float32), sr, subtype='FLOAT')


def _safe(name: str) -> str:
    return ''.join(c if c.isalnum() or c in '-_.' else '_' for c in name)


def run_tests(tests: list[Test], ref: Renderer, ours: Renderer, out_dir: str, cache: Cache,
              stft_floor: dict | None = None, save_audio: str = 'patches', log=print):
    """save_audio: 'all', 'patches' (patch tests + failures) or 'none'."""
    results = []
    stft_floor = stft_floor or {}
    t0 = time.time()
    for i, t in enumerate(tests):
        row = {'id': t.id, 'block': t.block, 'metric': t.metric, 'info': t.info}
        try:
            sr = t.job.sr
            r = cache.get(ref, t.job)
            o = ours.render(t.job)
            rc = cache.get(ref, t.companion) if t.companion else None
            oc = ours.render(t.companion) if t.companion else None

            lag = A.find_lag(r, o, max_lag=min(4096, len(r) // 4))
            ra, oa = A.align(r, o, lag)
            row['lag_samples'] = lag
            row['null_db'] = A.null_residual_db(ra, oa)
            row['level_ref_db'] = A.level_db(ra)
            row['level_ours_db'] = A.level_db(oa)
            row['stft'] = A.mrstft_distance(ra, oa)
            passed = True
            notes = []

            measure, compare, tol_key = METRICS[t.metric]
            if measure is not None:
                mr = measure(ra, sr, t, rc)
                mo = measure(oa, sr, t, oc)
                both_nan = False
                if t.metric == 'harmonics':
                    err, unit = harmonics_error(mr, mo), 'dB rms'
                    row['ref'], row['ours'] = None, None
                    row['harm_ref'], row['harm_ours'] = mr, mo
                elif t.metric == 'ltas':
                    err, unit = ltas_error(mr, mo), 'dB rms'
                    row['ref'], row['ours'] = None, None
                else:
                    err, unit = compare(mr, mo)
                    row['ref'], row['ours'] = mr, mo
                    both_nan = not np.isfinite(mr) and not np.isfinite(mo)
                tol = TOLERANCES[tol_key]
                ok = bool(np.isfinite(err) and abs(err) <= tol) or both_nan
                if both_nan:
                    notes.append('not measurable on either side')
                row.update(error=err, unit=unit, tolerance=tol, metric_pass=ok)
                passed &= ok
                if not ok:
                    notes.append(f'{t.metric} error {err:+.3g} {unit} (tol {tol})')
            if t.deterministic:
                ok = row['null_db'] <= TOLERANCES['null_db']
                row['null_pass'] = ok
                passed &= ok
                if not ok:
                    notes.append(f"null {row['null_db']:.1f} dB (tol {TOLERANCES['null_db']})")
            if t.metric in ('stft', 'patch'):
                floor = stft_floor.get(t.id, stft_floor.get('_default'))
                thr = max(floor * 1.5, floor + 0.05) if floor is not None else None
                row['stft_threshold'] = thr
                row['lufs_delta'] = A.lufs(oa, sr) - A.lufs(ra, sr)
                row['env_corr'] = A.envelope_correlation(ra, oa, sr)
                row['centroid_cents'] = A.centroid_distance(ra, oa, sr)
                f_r, f_o = A.yin_f0(ra, sr), A.yin_f0(oa, sr)
                row['pitch_err_cents'] = A.cents(f_o, f_r) if np.isfinite(f_r) and np.isfinite(f_o) else None
                row['level_err_db'] = row['level_ours_db'] - row['level_ref_db']
                if thr is not None:
                    ok = row['stft'] <= thr
                    passed &= ok
                    if not ok:
                        notes.append(f"STFT {row['stft']:.3f} > {thr:.3f}")
                else:
                    notes.append('no noise floor yet (run `noise-floor`)')
                    passed = False
            row['pass'] = bool(passed)
            row['notes'] = '; '.join(notes)

            if save_audio == 'all' or (save_audio == 'patches' and (t.block in ('patch', 'out_of_range') or not passed)):
                base = os.path.join(out_dir, 'wav', _safe(t.id))
                _write(base + '_osirus.wav', ra, sr)
                _write(base + '_vcclone.wav', oa, sr)
                _write(base + '_diff.wav', A.mono(ra) - A.mono(oa), sr)
                row['wav'] = os.path.relpath(base, out_dir).replace('\\', '/')
        except Exception as e:     # a failing render is a FAIL, not a crash
            row.update({'pass': False, 'notes': f'error: {type(e).__name__}: {e}'})
        results.append(row)
        if log and (i % 20 == 0 or not row.get('pass')):
            log(f"[{i + 1}/{len(tests)}] {'PASS' if row.get('pass') else 'FAIL'} {t.id} {row.get('notes', '')}")
    if log:
        log(f'{len(tests)} tests in {time.time() - t0:.1f}s')
    return results


def summarize(results):
    blocks = {}
    for r in results:
        b = blocks.setdefault(r['block'], {'tests': 0, 'pass': 0, 'fail': 0, 'worst': None, 'worst_id': None})
        b['tests'] += 1
        b['pass' if r.get('pass') else 'fail'] += 1
        err = r.get('error')
        if err is not None and isinstance(err, (int, float)) and math.isfinite(err):
            tol = r.get('tolerance') or 1
            score = abs(err) / tol
            if b['worst'] is None or score > b['worst'][0]:
                b['worst'] = (score, f"{err:+.3g} {r.get('unit', '')}")
                b['worst_id'] = r['id']
        elif r['block'] in ('patch', 'out_of_range') and r.get('stft') is not None:
            if b['worst'] is None or r['stft'] > b['worst'][0]:
                b['worst'] = (r['stft'], f"STFT {r['stft']:.3f}")
                b['worst_id'] = r['id']
    return blocks


def print_summary(blocks, log=print):
    log(f"{'block':<14} | {'tests':>5} | {'pass':>5} | {'fail':>5} | worst error")
    log('-' * 70)
    for name, b in sorted(blocks.items()):
        worst = b['worst'][1] + f"  ({b['worst_id']})" if b['worst'] else '-'
        log(f"{name:<14} | {b['tests']:>5} | {b['pass']:>5} | {b['fail']:>5} | {worst}")
    tot = sum(b['tests'] for b in blocks.values())
    ok = sum(b['pass'] for b in blocks.values())
    log('-' * 70)
    log(f"{'TOTAL':<14} | {tot:>5} | {ok:>5} | {tot - ok:>5} | pass rate {100.0 * ok / max(1, tot):.1f} %")


def regressions(results, baseline_path):
    """Tests that passed in the last passing baseline and fail now."""
    if not os.path.exists(baseline_path):
        return []
    base = {r['id']: r for r in json.load(open(baseline_path))['results']}
    return [r['id'] for r in results if base.get(r['id'], {}).get('pass') and not r.get('pass')]


def save_run(out_dir, results, meta):
    os.makedirs(out_dir, exist_ok=True)

    def clean(o):
        if isinstance(o, float) and not math.isfinite(o):
            return None
        if isinstance(o, dict):
            return {k: clean(v) for k, v in o.items()}
        if isinstance(o, (list, tuple)):
            return [clean(v) for v in o]
        if isinstance(o, (np.floating,)):
            return clean(float(o))
        if isinstance(o, (np.integer,)):
            return int(o)
        if isinstance(o, np.bool_):
            return bool(o)
        return o
    with open(os.path.join(out_dir, 'results.json'), 'w') as f:
        json.dump(clean({'meta': meta, 'results': results}), f, indent=1)
