import glob
import json
import os
import sys

import pytest

HERE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, HERE)


def find_vc_render():
    env = os.environ.get('VC_RENDER')
    if env and os.path.exists(env):
        return env
    pats = ['../build/Release/vc_render.exe', '../build/vc_render', '../build-linux/vc_render', '../build*/**/vc_render*']
    for p in pats:
        for hit in glob.glob(os.path.join(HERE, p), recursive=True):
            if os.path.isfile(hit) and os.access(hit, os.X_OK):
                return hit
    return None


@pytest.fixture(scope='session')
def vc_render():
    exe = find_vc_render()
    if not exe:
        pytest.skip('vc_render not built (set VC_RENDER)')
    return exe


@pytest.fixture(scope='session')
def config():
    path = os.environ.get('VCCLONE_CALIB_CONFIG', os.path.join(HERE, 'config.json'))
    if not os.path.exists(path):
        pytest.skip('no Calib/config.json (Osirus not configured)')
    from vcclone_calib.__main__ import load_config
    return load_config(path)
