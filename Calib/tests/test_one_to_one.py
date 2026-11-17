"""The 1:1 regression suite against Osirus (needs Calib/config.json and Osirus + ROM).

    pytest Calib/tests/test_one_to_one.py              # probes and sweeps (cached Osirus renders)
    VCCLONE_PATCHES=1 pytest ...                        # plus the user's patch files
"""
import os

import pytest

from vcclone_calib import probes as P
from vcclone_calib.__main__ import make_renderers
from vcclone_calib.suite import Cache, run_tests
from vcclone_calib.sysex import load_singles

_TESTS = P.build_probes() + P.build_out_of_range_tests()


@pytest.fixture(scope='module')
def env(config, tmp_path_factory):
    ref, ours = make_renderers(config)
    return ref, ours, Cache(config['cache_dir']), str(tmp_path_factory.mktemp('one_to_one'))


@pytest.mark.parametrize('test', [t for t in _TESTS if t.metric not in ('stft', 'patch')], ids=lambda t: t.id)
def test_probe(env, test):
    ref, ours, cache, out = env
    row = run_tests([test], ref, ours, out, cache, log=None)[0]
    assert row['pass'], row.get('notes')


def test_patches(env, config):
    if not os.environ.get('VCCLONE_PATCHES'):
        pytest.skip('set VCCLONE_PATCHES=1')
    ref, ours, cache, out = env
    import json
    floor_path = os.path.join(config['results_dir'], 'noise_floor.json')
    floor = json.load(open(floor_path)) if os.path.exists(floor_path) else None
    if not floor:
        pytest.skip('run `python -m vcclone_calib noise-floor` first')
    tests = []
    for path in config['patch_files']:
        tests += P.build_patch_tests(load_singles(path), os.path.basename(path))
    rows = run_tests(tests, ref, ours, out, cache, stft_floor=floor, log=None)
    failed = [r['id'] for r in rows if not r['pass']]
    assert not failed, f'{len(failed)}/{len(rows)} patch renders outside the thresholds'
