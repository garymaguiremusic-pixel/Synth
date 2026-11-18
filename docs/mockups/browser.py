top = '''<div style="position:absolute;left:0;top:0;width:1200px;height:64px;display:flex;align-items:center;gap:12px;padding:0 22px;box-sizing:border-box;border-bottom:1px solid #0c0d0f;background:linear-gradient(#34383d,#23262a)">
<div style="font-weight:900;font-style:italic;font-size:26px;width:140px">vc<span style="font-weight:400;color:#3fa9ff">clone</span></div>
<div style="width:420px;height:40px;border-radius:8px;background:#15181b;display:flex;align-items:center;gap:10px;padding:0 8px 0 10px;box-sizing:border-box"><span style="font-size:10px;font-weight:900;padding:4px 7px;border-radius:4px;background:#3fa9ff;color:#06121e">VIRUS ▾</span><span style="font-size:16px;font-weight:800;flex-grow:1">Pacific KS</span><span style="font-size:10px;font-weight:800;padding:4px 8px;border:1px solid #3fa9ff;border-radius:5px;color:#7cc4ff">INIT</span><span>◀ ▶</span></div>
<span style="flex-grow:1"></span>
<span style="font-size:10px;font-weight:800;padding:5px 11px;border-radius:5px;background:#3fa9ff;color:#06121e;box-shadow:0 0 10px rgba(63,169,255,.6)">Browser</span><span style="font-size:10px;font-weight:800;padding:5px 9px;border-radius:5px;background:#3c4146">Import</span><span style="font-size:10px;font-weight:800;padding:5px 9px;border-radius:5px;background:#3c4146">Export</span></div>'''

def column(x, title, active, banks, patches, selbank, selpatch):
    bk = ''
    for i, (name, n) in enumerate(banks):
        sel = i == selbank
        bk += f'''<div style="height:30px;display:flex;align-items:center;gap:8px;padding:0 10px;border-radius:6px;background:{"#1f6bc4" if sel else "#181b1e"};border:1px solid {"#3fa9ff" if sel else "#0c0d0f"}">
<span style="font-size:12px">▤</span><span style="font-size:12px;font-weight:800;flex-grow:1;white-space:nowrap;overflow:hidden">{name}</span><span style="font-size:10px;color:{"#dbeeff" if sel else "#8f99a3"};font-weight:700">{n}</span><span style="font-size:11px;color:{"#dbeeff" if sel else "#6c757e"}">✕</span></div>'''
    pt = ''
    for i, p in enumerate(patches):
        sel = i == selpatch
        pt += f'''<div style="height:24px;display:flex;align-items:center;gap:8px;padding:0 10px;border-radius:4px;background:{"rgba(63,169,255,.18)" if sel else "transparent"};{"box-shadow:inset 3px 0 0 #3fa9ff;" if sel else ""}font-size:12px;font-weight:{800 if sel else 600};color:{"#ffffff" if sel else "#cfd6dc"}"><span style="width:26px;color:#6c757e;font-size:10px">{i+1:03d}</span>{p}</div>'''
    glow = 'box-shadow:0 0 0 2px #3fa9ff,0 0 18px rgba(63,169,255,.25);' if active else ''
    return f'''<div style="position:absolute;left:{x}px;top:78px;width:380px;height:612px;box-sizing:border-box;background:linear-gradient(#2c3035,#23262a);border:1px solid #0c0d0f;border-radius:10px;{glow}">
<span style="position:absolute;left:0;top:0;padding:7px 14px;border-radius:10px 0 8px 0;background:#1a1d20;font-size:14px;font-weight:900;letter-spacing:.06em;color:{"#7cc4ff" if active else "#e8edf2"}">{title}</span>
<span style="position:absolute;right:10px;top:8px;height:24px;padding:0 10px;border-radius:5px;background:#1f6bc4;display:flex;align-items:center;font-size:11px;font-weight:800">+ LOAD BANK</span>
<div style="position:absolute;left:12px;right:12px;top:46px;font-size:10px;font-weight:800;color:#8f99a3;letter-spacing:.08em">BANKS</div>
<div style="position:absolute;left:12px;right:12px;top:62px;display:flex;flex-direction:column;gap:5px">{bk}</div>
<div style="position:absolute;left:12px;right:12px;top:200px;display:flex;align-items:center;justify-content:space-between"><span style="font-size:10px;font-weight:800;color:#8f99a3;letter-spacing:.08em">PATCHES</span>
<span style="width:170px;height:24px;border-radius:5px;background:#121417;border:1px solid #0a0b0c;display:flex;align-items:center;padding:0 8px;box-sizing:border-box;font-size:11px;color:#6c757e">⌕  search</span></div>
<div style="position:absolute;left:12px;right:12px;top:232px;bottom:12px;border-radius:6px;background:#15181b;border:1px solid #0c0d0f;padding:4px;display:flex;flex-direction:column;gap:1px;overflow:hidden">{pt}</div>
</div>'''

virus = column(20, 'VIRUS', True, [('EDM_Soundbank_1', '128'), ('EDM_Soundbank_2', '128'), ('Virus C Factory A', '128')],
               ['Cut KS', 'DirtyBitKS', 'Plugged KS', 'ES Groovy', 'Modern UXS', 'Squashy', 'BaliaRp BB', 'Disco KS', 'ES Clasynt', 'ChampionKS', 'Chip KS',
                'ES Highpas', '303Friends', 'Airwave', 'Clubing KS', 'Overtones', 'Pacific KS', 'Rave Stab'], 0, 16)
syl = column(410, 'SYLENTH', False, [('Factorybank1', '512')],
             ['ARP 303 Saw', 'ARP Doves', 'BS FreeBass', 'GAT Double HP', 'GAT Hades', 'KEY Crystal Bells', 'KEY House Organ', 'LD HydroSphere', 'LD PMW SID',
              'LD Turpan', 'LD Unicorn', 'PD ChillPad', 'PD MoonCloud', 'PD Morphing Pad', 'PD Overture', 'SFX Dungeon', 'SFX Eerie Mist', 'DR Shaker'], 0, -1)
spire = column(800, 'SPIRE', False, [('Spire Factory', '1')], ['LD 2000'], 0, -1)
foot = '<div style="position:absolute;left:20px;right:20px;top:698px;font-size:11px;color:#8f99a3;font-weight:600">Banks you load stay in their column every time you open the Browser. Click a patch to load it: the synth switches to that mode automatically.</div>'
open('browser.html', 'w').write('<html><body style="margin:0;background:#111"><div style="width:1200px;height:732px;position:relative;background:radial-gradient(ellipse at 30% 0%,#30353b,#1a1c1f 70%);color:#e8edf2;font-family:Inter,DejaVu Sans,sans-serif">' + top + virus + syl + spire + foot + '</div></body></html>')
