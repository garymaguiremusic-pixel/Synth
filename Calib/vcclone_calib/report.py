"""HTML report (ranked worst-first) and the blind A/B listening page."""
from __future__ import annotations

import html
import json
import math
import os
import random
from collections import defaultdict

from .probes import TOLERANCES
from .suite import summarize

CSS = """
:root{--bg:#1e2023;--panel:#2b2e32;--head:#50555a;--text:#e3e6e8;--dim:#9aa1a8;--blue:#3fa9ff;--ok:#48c774;--bad:#ff5a4f;--warn:#ffb340}
@media (prefers-color-scheme: light){:root:not([data-theme=dark]){--bg:#f3f4f6;--panel:#fff;--head:#d9dde2;--text:#1d2125;--dim:#5b636b}}
body{background:var(--bg);color:var(--text);font:14px/1.45 system-ui,Segoe UI,sans-serif;margin:0;padding:16px}
h1{font-size:20px;margin:0 0 4px} h2{font-size:16px;margin:24px 0 8px;border-bottom:1px solid var(--head);padding-bottom:4px}
table{border-collapse:collapse;width:100%;background:var(--panel)} th,td{padding:4px 8px;border-bottom:1px solid var(--head);text-align:left;vertical-align:top}
th{background:var(--head);position:sticky;top:0} td.num{text-align:right;font-variant-numeric:tabular-nums}
.pass{color:var(--ok);font-weight:600}.fail{color:var(--bad);font-weight:600}.dim{color:var(--dim)}
audio{height:28px;width:180px} .wrap{overflow-x:auto} svg text{fill:var(--dim);font-size:10px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(320px,1fr));gap:12px}
.card{background:var(--panel);padding:8px;border-radius:6px}
"""


def _f(v, fmt='{:.3g}'):
    if v is None or (isinstance(v, float) and not math.isfinite(v)):
        return '-'
    return fmt.format(v)


def _audio(row, which):
    if not row.get('wav'):
        return ''
    return f'<audio controls preload="none" src="{html.escape(row["wav"])}_{which}.wav"></audio>'


def _svg_sweep(points, title, logy=False):
    """points: list of (x, ref, ours)."""
    pts = [(x, r, o) for x, r, o in points if isinstance(r, (int, float)) and isinstance(o, (int, float))
           and math.isfinite(r) and math.isfinite(o) and (not logy or (r > 0 and o > 0))]
    if len(pts) < 2:
        return ''
    W, H, P = 320, 160, 28
    tr = (lambda v: math.log10(v)) if logy else (lambda v: v)
    ys = [tr(r) for _, r, _ in pts] + [tr(o) for _, _, o in pts]
    y0, y1 = min(ys), max(ys)
    if y1 - y0 < 1e-9:
        y1 = y0 + 1
    xs = [p[0] for p in pts]
    x0, x1 = min(xs), max(xs) or 1

    def X(x): return P + (x - x0) / max(1e-9, x1 - x0) * (W - P - 8)
    def Y(y): return H - P + 8 - (tr(y) - y0) / (y1 - y0) * (H - P - 8)
    ref = ' '.join(f'{X(x):.1f},{Y(r):.1f}' for x, r, _ in pts)
    our = ' '.join(f'{X(x):.1f},{Y(o):.1f}' for x, _, o in pts)
    return (f'<div class="card"><b>{html.escape(title)}</b><svg viewBox="0 0 {W} {H}" width="100%">'
            f'<polyline fill="none" stroke="#9aa1a8" stroke-width="2" points="{ref}"/>'
            f'<polyline fill="none" stroke="#3fa9ff" stroke-width="1.5" stroke-dasharray="4 2" points="{our}"/>'
            f'<text x="{P}" y="12">grey = Osirus, blue = VC-Clone{" (log)" if logy else ""}</text>'
            f'<text x="{P}" y="{H - 4}">{x0}</text><text x="{W - 24}" y="{H - 4}">{x1}</text></svg></div>')


def write_report(out_dir, results, meta, history):
    blocks = summarize(results)
    rows = []
    for name, b in sorted(blocks.items()):
        worst = b['worst'][1] if b['worst'] else '-'
        rows.append(f'<tr><td>{name}</td><td class="num">{b["tests"]}</td><td class="num pass">{b["pass"]}</td>'
                    f'<td class="num fail">{b["fail"]}</td><td>{html.escape(worst)} <span class="dim">{html.escape(b["worst_id"] or "")}</span></td></tr>')
    total = len(results)
    passed = sum(1 for r in results if r.get('pass'))

    # history chart
    hist_svg = ''
    if len(history) >= 1:
        W, H = 600, 120
        pts = ' '.join(f'{20 + i * (W - 40) / max(1, len(history) - 1):.1f},{H - 15 - h["pass_rate"] * (H - 30) / 100:.1f}'
                       for i, h in enumerate(history))
        hist_svg = (f'<svg viewBox="0 0 {W} {H}" width="100%" style="max-width:{W}px;background:var(--panel)">'
                    f'<polyline fill="none" stroke="#3fa9ff" stroke-width="2" points="{pts}"/>'
                    f'<text x="4" y="12">pass rate over runs (latest {history[-1]["pass_rate"]:.1f} %)</text></svg>')

    # patches ranked worst-first (aggregate the three jobs per patch)
    per = defaultdict(list)
    for r in results:
        if r['block'] in ('patch', 'out_of_range'):
            per[r['info'].get('patch') or r['id']].append(r)
    prow = []
    for name, rs in sorted(per.items(), key=lambda kv: -max((x.get('stft') or 0) for x in kv[1])):
        for r in rs:
            ok = r.get('pass')
            prow.append(
                f'<tr><td>{html.escape(r["id"])}</td><td class="{"pass" if ok else "fail"}">{"PASS" if ok else "FAIL"}</td>'
                f'<td class="num">{_f(r.get("stft"))}</td><td class="num">{_f(r.get("stft_threshold"))}</td>'
                f'<td class="num">{_f(r.get("pitch_err_cents"))}</td><td class="num">{_f(r.get("level_err_db"))}</td>'
                f'<td class="num">{_f(r.get("lufs_delta"))}</td><td class="num">{_f(r.get("env_corr"))}</td>'
                f'<td class="num">{_f(r.get("centroid_cents"))}</td><td class="num">{_f(r.get("null_db"))}</td>'
                f'<td>{_audio(r, "osirus")}{_audio(r, "vcclone")}{_audio(r, "diff")}</td></tr>')

    # failing non-patch tests, worst-first by error / tolerance
    fails = [r for r in results if not r.get('pass') and r['block'] not in ('patch', 'out_of_range')]
    fails.sort(key=lambda r: -(abs(r['error']) / (r.get('tolerance') or 1) if isinstance(r.get('error'), (int, float)) and math.isfinite(r['error']) else 1e9))
    frow = [f'<tr><td>{html.escape(r["id"])}</td><td class="num">{_f(r.get("ref"))}</td><td class="num">{_f(r.get("ours"))}</td>'
            f'<td class="num">{_f(r.get("error"))} {html.escape(r.get("unit", ""))}</td><td class="num">{_f(r.get("tolerance"))}</td>'
            f'<td class="num">{_f(r.get("null_db"))}</td><td>{html.escape(r.get("notes", ""))}</td>'
            f'<td>{_audio(r, "osirus")}{_audio(r, "vcclone")}{_audio(r, "diff")}</td></tr>' for r in fails[:300]]

    # sweep plots
    sweeps = defaultdict(list)
    for r in results:
        p = r['id'].rsplit('/', 1)
        if len(p) == 2 and p[1].lstrip('-').isdigit() and r.get('ref') is not None:
            sweeps[p[0]].append((int(p[1]), r.get('ref'), r.get('ours')))
    plots = ''.join(_svg_sweep(sorted(v), k, logy=any(s in k for s in ('cutoff', 'env/', 'lfo', 'rt60', 'reverb', 'delay')))
                    for k, v in sorted(sweeps.items()))

    tol = ', '.join(f'{k} {v}' for k, v in TOLERANCES.items())
    meta_html = '<br>'.join(f'<b>{html.escape(str(k))}</b>: {html.escape(str(v))}' for k, v in meta.items())
    doc = f"""<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>VC-Clone 1:1 Report</title><style>{CSS}</style></head><body>
<h1>VC-Clone vs Osirus: 1:1 report</h1>
<div class="dim">{meta_html}<br><b>tolerances</b>: {html.escape(tol)}<br>
Results are "matches within X", never bit-identical. <a href="ab.html">Blind A/B listening test</a></div>
<h2>Summary: {passed}/{total} pass ({100.0 * passed / max(1, total):.1f} %)</h2>
<div class="wrap"><table><tr><th>block</th><th>tests</th><th>pass</th><th>fail</th><th>worst error</th></tr>{''.join(rows)}</table></div>
{hist_svg}
<h2>Patches (worst first)</h2>
<div class="wrap"><table><tr><th>test</th><th></th><th>STFT</th><th>threshold</th><th>pitch err (c)</th><th>level err (dB)</th><th>LUFS Δ</th>
<th>env corr</th><th>centroid (c)</th><th>null (dB)</th><th>listen: Osirus / VC-Clone / diff</th></tr>{''.join(prow) or '<tr><td colspan=11 class=dim>no patch tests in this run</td></tr>'}</table></div>
<h2>Failing probes and sweeps (worst first)</h2>
<div class="wrap"><table><tr><th>test</th><th>Osirus</th><th>VC-Clone</th><th>error</th><th>tol</th><th>null</th><th>notes</th><th>listen</th></tr>
{''.join(frow) or '<tr><td colspan=8 class=pass>none</td></tr>'}</table></div>
<h2>Sweeps</h2><div class="grid">{plots}</div>
</body></html>"""
    with open(os.path.join(out_dir, 'report.html'), 'w', encoding='utf-8') as f:
        f.write(doc)
    write_ab_page(out_dir, results)


def write_ab_page(out_dir, results):
    items = []
    rng = random.Random(1234)
    for r in results:
        if r.get('wav') and r['block'] in ('patch', 'out_of_range'):
            flip = rng.random() < 0.5
            items.append({'id': r['id'], 'a': f'{r["wav"]}_{"vcclone" if flip else "osirus"}.wav',
                          'b': f'{r["wav"]}_{"osirus" if flip else "vcclone"}.wav', 'orig': 'b' if flip else 'a'})
    rng.shuffle(items)
    doc = f"""<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Blind A/B Test</title><style>{CSS} .item{{background:var(--panel);margin:8px 0;padding:8px;border-radius:6px}}
button{{background:var(--head);color:var(--text);border:0;padding:6px 10px;border-radius:4px;cursor:pointer}} button.sel{{background:var(--blue);color:#fff}}</style></head><body>
<h1>Blind A/B: which one is the original (Osirus)?</h1>
<div class="dim">Order is randomised per patch. Answers stay in this browser. <span id="score"></span>
<button onclick="exportAnswers()">Export answers (JSON)</button> <button onclick="reveal()">Reveal</button></div>
<div id="list"></div>
<script>
const ITEMS = {json.dumps(items)};
const KEY = 'vcclone-ab';
let answers = {{}};
try {{ answers = JSON.parse(localStorage.getItem(KEY) || '{{}}'); }} catch (e) {{}}
function save() {{ try {{ localStorage.setItem(KEY, JSON.stringify(answers)); }} catch (e) {{}} render(); }}
function pick(id, v) {{ answers[id] = v; save(); }}
let revealed = false;
function reveal() {{ revealed = !revealed; render(); }}
function render() {{
  const list = document.getElementById('list'); list.innerHTML = '';
  let n = 0, right = 0;
  ITEMS.forEach((it, i) => {{
    const d = document.createElement('div'); d.className = 'item';
    const a = answers[it.id];
    if (a) {{ n++; if (a === it.orig) right++; }}
    d.innerHTML = `<b>#${{i + 1}}</b> ${{revealed ? it.id : ''}}<br>A <audio controls preload="none" src="${{it.a}}"></audio>
      B <audio controls preload="none" src="${{it.b}}"></audio>
      <button class="${{a === 'a' ? 'sel' : ''}}" onclick="pick(ITEMS[${{i}}].id,'a')">A is original</button>
      <button class="${{a === 'b' ? 'sel' : ''}}" onclick="pick(ITEMS[${{i}}].id,'b')">B is original</button>
      ${{revealed ? '<span class="dim">original: ' + it.orig.toUpperCase() + '</span>' : ''}}`;
    list.appendChild(d);
  }});
  document.getElementById('score').textContent = n ? `${{right}}/${{n}} identified (${{(100 * right / n).toFixed(0)}} %; 50 % = indistinguishable)` : '';
}}
function exportAnswers() {{
  const blob = new Blob([JSON.stringify({{items: ITEMS, answers}}, null, 1)], {{type: 'application/json'}});
  const u = URL.createObjectURL(blob); const l = document.createElement('a'); l.href = u; l.download = 'ab_answers.json'; l.click();
}}
render();
</script></body></html>"""
    with open(os.path.join(out_dir, 'ab.html'), 'w', encoding='utf-8') as f:
        f.write(doc)
