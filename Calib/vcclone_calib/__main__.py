"""VC-Clone calibration and 1:1 test harness.

  python -m vcclone_calib suite        [--blocks pitch,filter,...] [--full] [--patches] [--save-audio all|patches|none]
  python -m vcclone_calib noise-floor  (Osirus vs Osirus repeat renders -> STFT thresholds)
  python -m vcclone_calib rate         (measure Osirus's internal sample rate)
  python -m vcclone_calib fit RESULTS  (write Engine/Tables/Measured.h from a sweep run)
  python -m vcclone_calib probes OUT   (write every probe patch as .syx, for manual checks)

Configuration: Calib/config.json (copy config.example.json). Paths are relative to Calib/.
"""
from __future__ import annotations

import argparse
import datetime
import json
import os
import sys

import numpy as np

from . import analysis as A
from . import fit as fitmod
from . import probes as P
from .hosts import Job, Note, PluginRenderer, VcRenderCli
from .report import write_report
from .suite import Cache, print_summary, regressions, run_tests, save_run, summarize
from .sysex import load_singles

HERE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))


def load_config(path):
    cfg = json.load(open(path))
    for k in ('osirus', 'vc_render', 'vcclone_vst3', 'results_dir', 'cache_dir'):
        if cfg.get(k):
            cfg[k] = os.path.normpath(os.path.join(HERE, cfg[k]))
    cfg['patch_files'] = [os.path.normpath(os.path.join(HERE, p)) for p in cfg.get('patch_files', [])]
    cfg.setdefault('results_dir', os.path.join(HERE, 'results'))
    cfg.setdefault('cache_dir', os.path.join(HERE, 'cache'))
    return cfg


def make_renderers(cfg):
    ref = PluginRenderer(cfg['osirus'], host=cfg.get('host', 'pedalboard'), settle=cfg.get('settle_seconds', 0.5),
                         preset_method=cfg.get('preset_method', 'params'), plugin_name=cfg.get('osirus_plugin_name'))
    if cfg.get('ours') == 'vst3':
        ours = PluginRenderer(cfg['vcclone_vst3'], host=cfg.get('host', 'pedalboard'), settle=cfg.get('settle_seconds', 0.5))
    else:
        ours = VcRenderCli(cfg['vc_render'])
    return ref, ours


def collect_tests(cfg, args):
    tests = []
    want = set(args.blocks.split(',')) if args.blocks else None
    for t in P.build_probes(full=args.full):
        if want is None or t.block in want:
            tests.append(t)
    if want is None or 'out_of_range' in want:
        tests += P.build_out_of_range_tests()
    if args.patches or (want and 'patch' in want):
        for path in cfg['patch_files']:
            singles = load_singles(path)
            if args.limit:
                singles = singles[:args.limit]
            tests += P.build_patch_tests(singles, os.path.basename(path))
    for t in tests:
        t.job.sr = cfg.get('sample_rate', 44100)
        t.job.block = cfg.get('block_size', 512)
        if t.companion:
            t.companion.sr, t.companion.block = t.job.sr, t.job.block
    return tests


def cmd_suite(cfg, args):
    ref, ours = make_renderers(cfg)
    cache = Cache(cfg['cache_dir'])
    tests = collect_tests(cfg, args)
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    out_dir = os.path.join(cfg['results_dir'], stamp)
    floor_path = os.path.join(cfg['results_dir'], 'noise_floor.json')
    floor = json.load(open(floor_path)) if os.path.exists(floor_path) else {}
    results = run_tests(tests, ref, ours, out_dir, cache, stft_floor=floor, save_audio=args.save_audio)
    blocks = summarize(results)
    print_summary(blocks)

    hist_path = os.path.join(cfg['results_dir'], 'history.json')
    history = json.load(open(hist_path)) if os.path.exists(hist_path) else []
    passed = sum(1 for r in results if r.get('pass'))
    history.append({'run': stamp, 'tests': len(results), 'pass': passed, 'pass_rate': 100.0 * passed / max(1, len(results))})
    json.dump(history, open(hist_path, 'w'), indent=1)

    rate_path = os.path.join(cfg['results_dir'], 'osirus_rate.json')
    meta = {'run': stamp, 'osirus': ref.id, 'vcclone': ours.id, 'sample_rate': cfg.get('sample_rate', 44100),
            'block_size': cfg.get('block_size', 512),
            'osirus_internal_rate': json.load(open(rate_path)).get('estimate_hz') if os.path.exists(rate_path) else 'not measured (run `rate`)'}
    save_run(out_dir, results, meta)
    write_report(out_dir, results, meta, history)
    print(f'report: {os.path.join(out_dir, "report.html")}')

    baseline = os.path.join(cfg['results_dir'], 'baseline.json')
    regs = regressions(results, baseline)
    if regs:
        print(f'REGRESSIONS against the last passing baseline ({len(regs)}):')
        for r in regs[:50]:
            print('  ' + r)
        return 1
    # the new baseline is the union of what passes now
    import shutil
    shutil.copy(os.path.join(out_dir, 'results.json'), baseline)
    return 0


def cmd_noise_floor(cfg, args):
    """Render every patch/STFT job twice through Osirus: the STFT distance between the
    two renders is the floor the VC-Clone thresholds are set from."""
    ref, _ = make_renderers(cfg)
    cache = Cache(cfg['cache_dir'])
    args.blocks = None
    tests = [t for t in collect_tests(cfg, args) if t.metric in ('stft', 'patch')]
    floor = {}
    for t in tests:
        a = cache.get(ref, t.job)
        b = cache.get(ref, t.job, salt=':repeat')
        lag = A.find_lag(a, b, max_lag=min(4096, len(a) // 4))
        ra, rb = A.align(a, b, lag)
        floor[t.id] = A.mrstft_distance(ra, rb)
        print(f'{t.id}: floor {floor[t.id]:.4f}')
    if floor:
        floor['_default'] = float(np.median(list(floor.values())))
    os.makedirs(cfg['results_dir'], exist_ok=True)
    json.dump(floor, open(os.path.join(cfg['results_dir'], 'noise_floor.json'), 'w'), indent=1)
    print(f"default floor {floor.get('_default', float('nan')):.4f}")
    return 0


def cmd_rate(cfg, args):
    ref, _ = make_renderers(cfg)
    p = P.init_patch().set('Osc Mainvolume', 0).set('Noise Volume', 127)
    out = {}
    for host_rate in (96000, 192000):
        audio = ref.render(Job(p, [Note(60, 0, 2.0)], 2.0, sr=host_rate))
        out[str(host_rate)] = A.internal_rate_estimate(audio, host_rate)
        print(f'host {host_rate} Hz -> content up to {out[str(host_rate)] / 2:.0f} Hz -> internal rate ~{out[str(host_rate)]:.0f} Hz')
    out['estimate_hz'] = float(np.median([v for v in out.values()]))
    os.makedirs(cfg['results_dir'], exist_ok=True)
    json.dump(out, open(os.path.join(cfg['results_dir'], 'osirus_rate.json'), 'w'), indent=1)
    return 0


def cmd_probes(cfg, args):
    os.makedirs(args.out, exist_ok=True)
    for t in P.build_probes(full=args.full) + P.build_out_of_range_tests():
        name = t.id.replace('/', '_') + '.syx'
        open(os.path.join(args.out, name), 'wb').write(t.job.patch.dump())
    print(f'wrote probes to {args.out}')
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(prog='vcclone_calib', description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--config', default=os.path.join(HERE, 'config.json'))
    sub = ap.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('suite')
    s.add_argument('--blocks', default='')
    s.add_argument('--full', action='store_true', help='every value 0..127 instead of every 8th')
    s.add_argument('--patches', action='store_true', help='also A/B the user patch files from the config')
    s.add_argument('--limit', type=int, default=0, help='max patches per file')
    s.add_argument('--save-audio', default='patches', choices=['all', 'patches', 'none'])
    n = sub.add_parser('noise-floor')
    n.add_argument('--full', action='store_true')
    n.add_argument('--patches', action='store_true', default=True)
    n.add_argument('--limit', type=int, default=0)
    n.add_argument('--blocks', default='')
    sub.add_parser('rate')
    f = sub.add_parser('fit')
    f.add_argument('results')
    pr = sub.add_parser('probes')
    pr.add_argument('out')
    pr.add_argument('--full', action='store_true')
    args = ap.parse_args(argv)

    if args.cmd == 'fit':
        fitmod.fit(args.results)
        return 0
    cfg = load_config(args.config)
    return {'suite': cmd_suite, 'noise-floor': cmd_noise_floor, 'rate': cmd_rate, 'probes': cmd_probes}[args.cmd](cfg, args)


if __name__ == '__main__':
    sys.exit(main())
