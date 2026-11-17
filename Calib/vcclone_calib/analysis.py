"""Audio measurement for the 1:1 comparison (numpy/scipy only)."""
from __future__ import annotations

import numpy as np
from scipy import signal

EPS = 1e-12


def mono(x: np.ndarray) -> np.ndarray:
    x = np.asarray(x, dtype=np.float64)
    return x.mean(axis=1) if x.ndim == 2 else x


def rms(x) -> float:
    x = np.asarray(x, dtype=np.float64)
    return float(np.sqrt(np.mean(x * x) + EPS))


def db(v: float) -> float:
    return 20.0 * np.log10(max(v, EPS))


def level_db(x) -> float:
    return db(rms(x))


# --- alignment / null test ------------------------------------------------------------
def find_lag(ref: np.ndarray, test: np.ndarray, max_lag: int = 4096) -> int:
    """Samples by which `test` lags `ref` (positive = test is late)."""
    a, b = mono(ref), mono(test)
    n = min(len(a), len(b))
    a, b = a[:n], b[:n]
    size = 1 << int(np.ceil(np.log2(2 * n)))
    xc = np.fft.irfft(np.fft.rfft(b, size) * np.conj(np.fft.rfft(a, size)), size)
    xc = np.concatenate([xc[-max_lag:], xc[:max_lag + 1]])
    return int(np.argmax(xc)) - max_lag


def align(ref: np.ndarray, test: np.ndarray, lag: int):
    if lag > 0:
        test = test[lag:]
    elif lag < 0:
        ref = ref[-lag:]
    n = min(len(ref), len(test))
    return ref[:n], test[:n]


def null_residual_db(ref, test, level_match: bool = False) -> float:
    r, t = mono(ref), mono(test)
    if rms(r) <= 1e-5 and rms(t) <= 1e-5:
        return -240.0                      # both silent: a perfect null
    if level_match:
        t = t * (rms(r) / rms(t))
    return db(rms(r - t)) - db(rms(r))


# --- spectral distances -----------------------------------------------------------------
def mrstft_distance(ref, test, sizes=(512, 1024, 2048)) -> float:
    """Multi-resolution STFT distance: spectral convergence + log-magnitude L1, averaged."""
    r, t = mono(ref), mono(test)
    total = 0.0
    for n in sizes:
        _, _, R = signal.stft(r, nperseg=n, noverlap=n * 3 // 4)
        _, _, T = signal.stft(t, nperseg=n, noverlap=n * 3 // 4)
        R, T = np.abs(R), np.abs(T)
        sc = np.linalg.norm(T - R) / (np.linalg.norm(R) + EPS)
        lm = np.mean(np.abs(np.log(T + 1e-7) - np.log(R + 1e-7)))
        total += sc + lm
    return float(total / len(sizes))


def ltas_bands(x, sr) -> np.ndarray:
    """Long-term average spectrum in 1/3-octave bands (dB), for free-running material."""
    f, p = signal.welch(mono(x), fs=sr, nperseg=8192)
    edges = 25.0 * 2.0 ** (np.arange(0, 30) / 3.0)
    out = []
    for lo, hi in zip(edges[:-1], edges[1:]):
        m = (f >= lo) & (f < hi)
        out.append(10 * np.log10(p[m].mean() + EPS) if m.any() else -200.0)
    return np.array(out)


def spectral_centroid(x, sr, frame=2048, hop=512) -> np.ndarray:
    x = mono(x)
    f, _, Z = signal.stft(x, fs=sr, nperseg=frame, noverlap=frame - hop)
    mag = np.abs(Z)
    return (f[:, None] * mag).sum(0) / (mag.sum(0) + EPS)


def envelope_db(x, sr, frame_ms=10.0) -> np.ndarray:
    x = mono(x)
    n = max(1, int(sr * frame_ms / 1000))
    frames = len(x) // n
    if frames == 0:
        return np.array([-240.0])
    e = np.sqrt(np.mean(x[:frames * n].reshape(frames, n) ** 2, axis=1) + EPS)
    return 20 * np.log10(e)


def envelope_correlation(ref, test, sr) -> float:
    a, b = envelope_db(ref, sr), envelope_db(test, sr)
    n = min(len(a), len(b))
    a, b = np.maximum(a[:n], -90), np.maximum(b[:n], -90)
    if np.std(a) < 1e-6 or np.std(b) < 1e-6:
        return 1.0 if np.allclose(a, b, atol=1.0) else 0.0
    return float(np.corrcoef(a, b)[0, 1])


def centroid_distance(ref, test, sr) -> float:
    a, b = spectral_centroid(ref, sr), spectral_centroid(test, sr)
    n = min(len(a), len(b))
    a, b = a[:n], b[:n]
    mask = (a > 20) & (b > 20)
    if not mask.any():
        return 0.0
    return float(np.median(np.abs(1200 * np.log2(b[mask] / a[mask]))))   # cents


# --- loudness (ITU-R BS.1770, integrated, gated) -------------------------------------------
def lufs(x, sr) -> float:
    x = np.asarray(x, dtype=np.float64)
    if x.ndim == 1:
        x = x[:, None]
    # K-weighting: high-shelf + high-pass (coefficients derived for any sr)
    f0, G, Q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    K = np.tan(np.pi * f0 / sr)
    Vh, Vb = 10 ** (G / 20), 10 ** (G / 20) ** 0.499666774155
    a0 = 1 + K / Q + K * K
    b1 = [(Vh + Vb * K / Q + K * K) / a0, 2 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0]
    a1 = [1, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0]
    f0, Q = 38.13547087602444, 0.5003270373238773
    K = np.tan(np.pi * f0 / sr)
    a2 = [1, 2 * (K * K - 1) / (1 + K / Q + K * K), (1 - K / Q + K * K) / (1 + K / Q + K * K)]
    b2 = [1, -2, 1]
    y = signal.lfilter(b2, a2, signal.lfilter(b1, a1, x, axis=0), axis=0)
    block, hop = int(0.4 * sr), int(0.1 * sr)
    if len(y) < block:
        z = np.mean(y ** 2, axis=0).sum()
        return float(-0.691 + 10 * np.log10(z + EPS))
    zs = np.array([np.mean(y[i:i + block] ** 2, axis=0).sum() for i in range(0, len(y) - block + 1, hop)])
    lk = -0.691 + 10 * np.log10(zs + EPS)
    zs = zs[lk > -70]
    if len(zs) == 0:
        return -70.0
    rel = -0.691 + 10 * np.log10(zs.mean()) - 10
    lk = -0.691 + 10 * np.log10(zs + EPS)
    zs = zs[lk > rel]
    return float(-0.691 + 10 * np.log10(zs.mean() + EPS)) if len(zs) else -70.0


# --- pitch ------------------------------------------------------------------------------------
def yin_f0(x, sr, fmin=30.0, fmax=4000.0, threshold=0.1) -> float:
    """YIN fundamental estimate over the whole segment (median of frames)."""
    x = mono(x)
    w = int(sr / fmin) * 2
    hop = w // 2
    tmin, tmax = int(sr / fmax), int(sr / fmin)
    est = []
    for s in range(0, len(x) - w - tmax, hop):
        f = x[s:s + w + tmax]
        d = np.array([np.sum((f[:w] - f[t:t + w]) ** 2) for t in range(tmax + 1)])
        cmnd = d.copy()
        cmnd[0] = 1
        cmnd[1:] = d[1:] * np.arange(1, tmax + 1) / (np.cumsum(d[1:]) + EPS)
        cand = np.where(cmnd[tmin:] < threshold)[0]
        if len(cand) == 0:
            continue
        t = cand[0] + tmin
        while t + 1 <= tmax and cmnd[t + 1] < cmnd[t]:
            t += 1
        if 1 <= t < tmax:
            a, b, c = cmnd[t - 1], cmnd[t], cmnd[t + 1]
            denom = a - 2 * b + c
            t = t + (0.5 * (a - c) / denom if abs(denom) > EPS else 0.0)
        est.append(sr / t)
    return float(np.median(est)) if est else float('nan')


def cents(a, b) -> float:
    return float(1200 * np.log2(a / b))


# --- filters: transfer function from noise renders ------------------------------------------------
def transfer_db(filtered, reference, sr, nperseg=8192):
    f, pf = signal.welch(mono(filtered), fs=sr, nperseg=nperseg)
    _, pr = signal.welch(mono(reference), fs=sr, nperseg=nperseg)
    return f, 10 * np.log10((pf + EPS) / (pr + EPS))


def band_average(f, h_db, per_octave=12, fmin=20.0, fmax=20000.0):
    """Power-average the transfer function into fractional-octave bands (reduces noise variance)."""
    edges = fmin * 2.0 ** (np.arange(0, np.log2(fmax / fmin) * per_octave + 1) / per_octave)
    fc, hb = [], []
    p = 10 ** (h_db / 10)
    for lo, hi in zip(edges[:-1], edges[1:]):
        m = (f >= lo) & (f < hi)
        if m.sum() >= 1:
            fc.append(np.sqrt(lo * hi))
            hb.append(10 * np.log10(p[m].mean() + EPS))
    return np.array(fc), np.array(hb)


def _model_db(f, fc, q, kind, sr=None):
    # bilinear (prewarped) digital prototype when sr is known, analog otherwise
    if sr:
        fc = min(fc, 0.4999 * sr)
        x = np.tan(np.pi * np.minimum(f, 0.4999 * sr) / sr) / np.tan(np.pi * fc / sr)
    else:
        x = f / fc
    if kind == 'lp':        # 2-pole state variable low-pass
        return -10 * np.log10((1 - x * x) ** 2 + (x / q) ** 2 + EPS)
    if kind == 'hp':
        return 10 * np.log10(x ** 4 + EPS) - 10 * np.log10((1 - x * x) ** 2 + (x / q) ** 2 + EPS)
    if kind == 'lp4':       # 4 x one-pole ladder with feedback k = q
        G = 1.0 / (1.0 + 1j * x)
        return 20 * np.log10(np.abs(G ** 4 / (1.0 + q * G ** 4)) + EPS)
    raise ValueError(kind)


def fit_filter(f, h_db, kind='lp') -> dict:
    """Least-squares fit of a filter model (free gain): returns the model cutoff (fc),
    resonance (Q, or k for the ladder) and the peak height of the fitted curve."""
    from scipy import optimize
    sr = 2.0 * float(np.max(f))          # welch frequencies run up to Nyquist
    fb, hb = band_average(f, h_db, fmax=min(20000.0, 0.49 * sr))
    valid = hb > hb.max() - 45            # ignore bands down in the noise floor
    fb, hb = fb[valid], hb[valid]
    if len(fb) < 6:
        return {'cutoff_hz': float('nan'), 'q': float('nan'), 'peak_db': float('nan')}
    qs = np.linspace(0.0, 3.9, 14) if kind == 'lp4' else np.geomspace(0.4, 60, 16)

    def err(params):
        fc, q = np.exp(params[0]), params[1]
        if kind != 'lp4' and q <= 0.05:
            return 1e9
        if kind == 'lp4' and not 0.0 <= q < 4.2:
            return 1e9
        m = _model_db(fb, fc, q, kind, sr)
        g = np.mean(hb - m)
        return float(np.mean((hb - m - g) ** 2))
    best = None
    for fc in np.geomspace(10, 0.49 * sr, 120):
        for q in qs:
            e = err([np.log(fc), q])
            if best is None or e < best[0]:
                best = (e, np.log(fc), q)
    r = optimize.minimize(err, [best[1], best[2]], method='Nelder-Mead', options={'xatol': 1e-4, 'fatol': 1e-6, 'maxiter': 400})
    # second pass: only the bands within 3 octaves of the first estimate (digital filters
    # depart from the analog model close to Nyquist)
    fc0 = float(np.exp(r.x[0]))
    near = (fb > fc0 / 8) & (fb < fc0 * 8)
    if near.sum() >= 6:
        fb, hb = fb[near], hb[near]
        r = optimize.minimize(err, r.x, method='Nelder-Mead', options={'xatol': 1e-4, 'fatol': 1e-6, 'maxiter': 400})
    fc, q = float(np.exp(r.x[0])), float(r.x[1])
    grid = np.geomspace(10, 0.49 * sr, 2000)
    m = _model_db(grid, fc, q, kind, sr)
    ref = m[0] if kind != 'hp' else m[-1]
    return {'cutoff_hz': fc, 'q': q, 'peak_db': float(m.max() - ref)}


def fit_cutoff(f, h_db, mode='lp') -> dict:
    return fit_filter(f, h_db, mode)


# --- envelopes -------------------------------------------------------------------------------------
def attack_time(x, sr) -> float:
    """10 % -> 90 % rise time of the amplitude envelope, seconds."""
    e = 10 ** (envelope_db(x, sr, 1.0) / 20)
    pk = e.max()
    if pk < 1e-6:
        return float('nan')
    i10 = int(np.argmax(e >= 0.1 * pk))
    i90 = int(np.argmax(e >= 0.9 * pk))
    return max(0.0, (i90 - i10) * 0.001)


def decay_time(x, sr, start_s=0.0, floor_db=-60.0) -> float:
    """Time from the peak (after start_s) until the level is 60 dB below the peak, extrapolated by a line fit."""
    e = envelope_db(x, sr, 2.0)
    s = int(start_s / 0.002)
    seg = e[s:]
    if len(seg) < 5:
        return float('nan')
    p = int(np.argmax(seg))
    seg = seg[p:]
    pk = seg[0]
    usable = np.where(seg > pk - 40)[0]
    if len(usable) < 3:
        return 0.002 * len(seg)
    n = usable[-1] + 1
    t = np.arange(n) * 0.002
    slope = np.polyfit(t, seg[:n], 1)[0]
    if slope >= -1e-3:
        return float('inf')
    return float(-60.0 / slope) if floor_db == -60.0 else float(floor_db / slope)


# --- LFO rate --------------------------------------------------------------------------------------
def modulation_rate(x, sr, min_hz=0.02, max_hz=80.0) -> float:
    """Rate of amplitude modulation (tremolo / filter wobble) from the envelope's autocorrelation."""
    e = envelope_db(x, sr, 2.0)
    e = e - e.mean()
    fs = 500.0
    ac = np.correlate(e, e, 'full')[len(e) - 1:]
    lo, hi = int(fs / max_hz), min(len(ac) - 1, int(fs / min_hz))
    if hi <= lo + 2:
        return float('nan')
    seg = ac[lo:hi]
    peaks, _ = signal.find_peaks(seg)
    if len(peaks) == 0:
        return float('nan')
    p = peaks[np.argmax(seg[peaks])] + lo
    return fs / p


def pitch_modulation_rate(x, sr, f0_guess: float) -> float:
    """Rate of vibrato: instantaneous frequency of the analytic signal around the fundamental."""
    x = mono(x)
    b, a = signal.butter(4, [f0_guess * 0.5 / (sr / 2), min(0.99, f0_guess * 1.5 / (sr / 2))], 'band')
    y = signal.filtfilt(b, a, x)
    inst = np.diff(np.unwrap(np.angle(signal.hilbert(y)))) * sr / (2 * np.pi)
    dec = max(1, int(sr / 1000))
    inst = signal.decimate(inst, dec) if dec > 1 else inst
    inst = inst - np.median(inst)
    fs = sr / dec
    f, p = signal.periodogram(inst, fs)
    m = (f > 0.02) & (f < 80)
    return float(f[m][np.argmax(p[m])]) if m.any() else float('nan')


# --- waves -----------------------------------------------------------------------------------------
def harmonics(x, sr, f0, count=64) -> np.ndarray:
    """Magnitudes of harmonics 1..count, normalised to the largest."""
    x = mono(x)
    n = len(x)
    win = signal.windows.blackmanharris(n)
    spec = np.abs(np.fft.rfft(x * win))
    freqs = np.fft.rfftfreq(n, 1 / sr)
    mags = np.zeros(count)
    bw = max(2, int(0.3 * f0 / (sr / n)))
    for k in range(1, count + 1):
        fk = k * f0
        if fk > sr * 0.45:
            break
        i = int(round(fk / (sr / n)))
        lo, hi = max(0, i - bw), min(len(spec), i + bw + 1)
        mags[k - 1] = spec[lo:hi].max()
    return mags / (mags.max() + EPS)


def internal_rate_estimate(noise_render, sr) -> float:
    """Highest frequency with energy within 40 dB of the 1-10 kHz median, doubled.
    Render white noise (filters open, FX off) at a high host rate (e.g. 96 kHz)."""
    f, p = signal.welch(mono(noise_render), fs=sr, nperseg=16384)
    ref = np.median(10 * np.log10(p[(f > 1000) & (f < 10000)] + EPS))
    lvl = 10 * np.log10(p + EPS)
    above = np.where(lvl > ref - 40)[0]
    return float(2 * f[above[-1]]) if len(above) else float('nan')


def static_curve(x, sr) -> dict:
    """Odd/even harmonic balance of a driven sine: a fingerprint of a saturation curve."""
    f0 = yin_f0(x, sr)
    if not np.isfinite(f0):
        return {'thd': float('nan'), 'even_odd': float('nan')}
    h = harmonics(x, sr, f0, 16)
    thd = float(np.sqrt(np.sum(h[1:] ** 2)) / (h[0] + EPS))
    even, odd = np.sum(h[1::2] ** 2), np.sum(h[2::2] ** 2)
    return {'thd': thd, 'even_odd': float(10 * np.log10((even + EPS) / (odd + EPS)))}
