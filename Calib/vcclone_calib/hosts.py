"""Renderers: Osirus (reference, hosted as a black box) and VC-Clone.

Osirus is hosted with `pedalboard` (default) or `dawdreamer`. The user's Virus C ROM is
loaded by Osirus itself (set it up once in the Osirus UI); this tooling never opens it.

Notes start after `settle` seconds of silence, so the plugin has applied the patch.
Default ("preset_method": "params"): every patch byte the plugin exposes as a host parameter
is set by name before rendering. "sysex" sends a single dump instead, but most VST3 hosts
(pedalboard included) do not deliver SysEx to VST3 plugins.
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import tempfile
from dataclasses import dataclass, field

import numpy as np
import soundfile as sf

from .sysex import Single, LAYOUT


@dataclass
class Note:
    note: int
    start: float
    dur: float
    vel: int = 100


@dataclass
class Job:
    patch: Single
    notes: list
    length: float
    sr: int = 44100
    block: int = 512
    cc: list = field(default_factory=list)   # (time, cc, value)
    seed: int = 1

    def key(self, renderer_id: str) -> str:
        h = hashlib.sha256()
        h.update(renderer_id.encode())
        h.update(self.patch.key())
        h.update(json.dumps([[n.note, n.start, n.dur, n.vel] for n in self.notes]).encode())
        h.update(json.dumps([self.length, self.sr, self.block, self.cc, self.seed]).encode())
        return h.hexdigest()[:24]


class Renderer:
    id = 'base'

    def render(self, job: Job) -> np.ndarray:   # returns (frames, 2) float32
        raise NotImplementedError


class VcRenderCli(Renderer):
    """VC-Clone through the deterministic vc_render CLI."""

    def __init__(self, exe: str):
        self.exe = exe
        self.id = 'vc_render:' + _file_id(exe)

    def render(self, job: Job) -> np.ndarray:
        with tempfile.TemporaryDirectory() as td:
            syx = os.path.join(td, 'p.syx')
            out = os.path.join(td, 'o.wav')
            open(syx, 'wb').write(job.patch.dump())
            args = [self.exe, '--preset', syx, '--len', str(job.length), '--sr', str(job.sr),
                    '--block', str(job.block), '--seed', str(job.seed), '--out', out]
            if job.notes:
                args += ['--notes', ','.join(f'{n.note}:{n.start}:{n.dur}:{n.vel}' for n in job.notes)]
            for t, c, v in job.cc:
                args += ['--cc', f'{t}:{c}:{v}']
            subprocess.run(args, check=True, capture_output=True)
            data, _ = sf.read(out, dtype='float32', always_2d=True)
            return data


class PluginRenderer(Renderer):
    """Any VST3 instrument (Osirus, or the VC-Clone VST3) via pedalboard or dawdreamer."""

    def __init__(self, path: str, host: str = 'pedalboard', settle: float = 0.5, preset_method: str = 'params',
                 plugin_name: str | None = None):
        self.path, self.host, self.settle, self.method = path, host, settle, preset_method
        self.plugin_name = plugin_name
        self.id = f'{host}:{os.path.basename(path)}:{_file_id(path)}:{preset_method}:{settle}'
        self._plugin = None
        self.params_matched = None

    # -- pedalboard ---------------------------------------------------------------------
    def _pb(self):
        if self._plugin is None:
            import pedalboard
            kw = {'plugin_name': self.plugin_name} if self.plugin_name else {}
            self._plugin = pedalboard.load_plugin(self.path, **kw)
        return self._plugin

    def _messages(self, job: Job):
        msgs = []
        t0 = self.settle                 # let the plugin apply the patch before the first note
        if self.method == 'sysex':
            msgs.append((job.patch.dump(), 0.0))
        for t, c, v in job.cc:
            msgs.append((bytes([0xB0, c & 0x7F, v & 0x7F]), t0 + t))
        for n in job.notes:
            msgs.append((bytes([0x90, n.note, n.vel]), t0 + n.start))
            msgs.append((bytes([0x80, n.note, 0]), t0 + n.start + n.dur))
        msgs.sort(key=lambda m: m[1])
        return msgs, t0

    def _set_params(self, plugin, job: Job):
        """Sets every patch byte that the plugin exposes as a host parameter (matched by name).
        Returns the number of parameters set; bytes without a host parameter keep the plugin's value."""
        params = getattr(plugin, 'parameters', {})
        lookup = {_norm(k): k for k in params}
        count = 0
        for p in LAYOUT:
            k = lookup.get(_norm(p.name)) if p.name else None
            if k is None:
                continue
            try:
                prm = params[k]
                v = max(p.min, min(p.max, job.patch.raw[p.offset]))
                prm.raw_value = (v - p.min) / max(1, p.max - p.min)
                count += 1
            except Exception:
                pass
        if count == 0:
            raise RuntimeError('no plugin parameter matched a Virus parameter name; check preset_method')
        if getattr(self, 'params_matched', None) is None:
            print(f'[{os.path.basename(self.path)}] {count} of the Virus patch parameters matched plugin parameters '
                  f'({len(params)} plugin parameters in total)')
        self.params_matched = count
        return count

    def render(self, job: Job) -> np.ndarray:
        if self.host == 'dawdreamer':
            return self._render_dawdreamer(job)
        plugin = self._pb()
        if self.method == 'params':
            self._set_params(plugin, job)
        msgs, t0 = self._messages(job)
        audio = plugin(msgs, duration=t0 + job.length, sample_rate=job.sr, num_channels=2,
                       buffer_size=job.block, reset=True)
        audio = np.asarray(audio, dtype=np.float32).T          # (frames, ch)
        start = int(round(t0 * job.sr))
        return audio[start:start + int(round(job.length * job.sr))]

    # -- dawdreamer ---------------------------------------------------------------------
    def _render_dawdreamer(self, job: Job) -> np.ndarray:
        import dawdreamer as daw
        engine = daw.RenderEngine(job.sr, job.block)
        synth = engine.make_plugin_processor('synth', self.path)
        msgs, t0 = self._messages(job)
        if self.method == 'sysex':
            if not hasattr(synth, 'add_midi_sysex'):
                raise RuntimeError('this dawdreamer build cannot send SysEx; use host "pedalboard" or preset_method "params"')
            synth.add_midi_sysex(job.patch.dump(), 0.0)
        else:
            names = {synth.get_parameter_name(i).lower().replace(' ', '_'): i for i in range(synth.get_plugin_parameter_size())}
            for p in LAYOUT:
                i = names.get(p.name.lower().replace(' ', '_').replace('/', '_')) if p.name else None
                if i is not None:
                    v = max(p.min, min(p.max, job.patch.raw[p.offset]))
                    synth.set_parameter(i, (v - p.min) / max(1, p.max - p.min))
        for t, c, v in job.cc:
            if hasattr(synth, 'add_midi_cc'):
                synth.add_midi_cc(c, v, t0 + t)
        for n in job.notes:
            synth.add_midi_note(n.note, n.vel, t0 + n.start, n.dur)
        engine.load_graph([(synth, [])])
        engine.render(t0 + job.length)
        audio = engine.get_audio().T.astype(np.float32)
        start = int(round(t0 * job.sr))
        return audio[start:start + int(round(job.length * job.sr))]


def _norm(name: str) -> str:
    import re
    return re.sub(r'[^a-z0-9]+', '_', name.lower()).strip('_')


def _file_id(path: str) -> str:
    """Identifies a binary by size + mtime (+ hash of the first MB): re-render when it changes."""
    try:
        if os.path.isdir(path):   # .vst3 bundles are directories on some platforms
            items = []
            for root, _, files in os.walk(path):
                for f in files:
                    p = os.path.join(root, f)
                    st = os.stat(p)
                    items.append(f'{os.path.relpath(p, path)}:{st.st_size}:{int(st.st_mtime)}')
            return hashlib.sha1('|'.join(sorted(items)).encode()).hexdigest()[:12]
        st = os.stat(path)
        with open(path, 'rb') as f:
            head = f.read(1 << 20)
        return hashlib.sha1(head + f'{st.st_size}:{int(st.st_mtime)}'.encode()).hexdigest()[:12]
    except OSError:
        return 'missing'
