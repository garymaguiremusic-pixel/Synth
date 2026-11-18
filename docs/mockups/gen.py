import math, sys
MODE = sys.argv[1] if len(sys.argv)>1 else 'virus'
def knob(label, v, size=34, w=None):
    w = w or max(size+14, 50)
    dash = 122.5*v; deg = -135+270*v
    return f'''<div style="width:{w}px;display:flex;flex-direction:column;align-items:center;gap:3px">
<div style="width:{size}px;height:{size}px;position:relative">
<svg width="{size}" height="{size}" viewBox="0 0 60 60" style="position:absolute;left:0;top:0;filter:drop-shadow(0 0 4px rgba(63,169,255,.55))">
<circle cx="30" cy="30" r="26" fill="none" stroke="#14171a" stroke-width="5" stroke-dasharray="122.5 200" transform="rotate(135 30 30)"/>
<circle cx="30" cy="30" r="26" fill="none" stroke="#3fa9ff" stroke-width="5" stroke-linecap="round" stroke-dasharray="{dash:.1f} 200" transform="rotate(135 30 30)"/></svg>
<div style="position:absolute;left:18%;top:18%;width:64%;height:64%;border-radius:50%;background:radial-gradient(circle at 38% 30%,#5d636a,#22252a 72%);box-shadow:0 3px 6px rgba(0,0,0,.6);transform:rotate({deg:.0f}deg)">
<div style="position:absolute;left:46%;top:8%;width:8%;height:30%;border-radius:2px;background:#eaf5ff"></div></div></div>
<span style="font-size:10px;font-weight:700;color:#aab3bb">{label}</span></div>'''
def drop(v, label, w=110):
    return f'''<div style="display:flex;flex-direction:column;align-items:center;gap:4px"><span style="width:{w}px;height:26px;border-radius:4px;background:#1f6bc4;display:flex;align-items:center;justify-content:space-between;padding:0 9px;box-sizing:border-box;font-size:12px;font-weight:800">{v}<span>▾</span></span><span style="font-size:10px;font-weight:700;color:#aab3bb">{label}</span></div>'''
def card(x,y,w,h,title,inner,power=None,chip=''):
    pw = '' if power is None else f'<span style="color:{"#3fa9ff" if power else "#6c757e"};margin-right:6px">⏻</span>'
    ch = f'<span style="position:absolute;right:10px;top:8px;font-size:9px;font-weight:800;padding:2px 6px;border-radius:3px;border:1px solid #3a3f45;color:#8f99a3">{chip}</span>' if chip else ''
    return f'''<div style="position:absolute;left:{x}px;top:{y}px;width:{w}px;height:{h}px;box-sizing:border-box;background:linear-gradient(#2c3035,#23262a);border:1px solid #0c0d0f;border-radius:8px">
<span style="position:absolute;left:0;top:0;padding:6px 12px 6px 10px;border-radius:8px 0 8px 0;background:#1a1d20;font-size:13px;font-weight:800;letter-spacing:.04em">{pw}{title}</span>{ch}{inner}</div>'''
row = lambda x,y,items,gap=4: f'<div style="position:absolute;left:{x}px;top:{y}px;display:flex;align-items:center;gap:{gap}px">{"".join(items)}</div>'
H=[]
# top bar
H.append('''<div style="position:absolute;left:0;top:0;width:1200px;height:64px;display:flex;align-items:center;gap:12px;padding:0 22px;box-sizing:border-box;border-bottom:1px solid #0c0d0f;background:linear-gradient(#34383d,#23262a)">
<div style="font-weight:900;font-style:italic;font-size:26px;width:140px">vc<span style="font-weight:400;color:#3fa9ff">clone</span></div>
<div style="width:420px;height:40px;border-radius:8px;background:#15181b;display:flex;align-items:center;gap:10px;padding:0 8px 0 10px;box-sizing:border-box"><span style="font-size:10px;font-weight:900;padding:4px 7px;border-radius:4px;background:#3fa9ff;color:#06121e">VIRUS ▾</span><span style="font-size:16px;font-weight:800;flex-grow:1">Pacific KS</span><span style="font-size:10px;font-weight:800;padding:4px 8px;border:1px solid #3fa9ff;border-radius:5px;color:#7cc4ff">INIT</span><span>◀ ▶</span></div>
<div style="width:110px;height:40px;border-radius:8px;background:#15181b;font-size:10px;font-weight:800;display:flex;flex-direction:column;justify-content:center;padding-left:10px;gap:3px"><span>VOICES 6/16</span><span>CPU 3%</span></div>
<div style="width:110px;height:40px;border-radius:8px;background:#0b0d0f"></div>''' + knob('output',0.7,30) + '''<span style="flex-grow:1"></span>
<span style="font-size:10px;font-weight:800;padding:5px 9px;border-radius:5px;background:#3c4146">Browser</span><span style="font-size:10px;font-weight:800;padding:5px 9px;border-radius:5px;background:#3c4146">Import</span><span style="font-size:10px;font-weight:800;padding:5px 9px;border-radius:5px;background:#3c4146">Export</span></div>''')
# oscillators
osc=''
for i,(n,m,k3,k4) in enumerate([('1','HyperSaw','density','spread'),('2','HyperSaw','density','spread'),('3','Pulse','detune','pw')]):
    num=f'<span style="width:30px;height:30px;border:2px solid #3fa9ff;border-radius:4px;display:flex;align-items:center;justify-content:center;font-weight:900;color:#7cc4ff;box-shadow:0 0 10px rgba(63,169,255,.5)">{n}</span>'
    osc+=row(14,36+i*70,[num,knob('level',.8),knob('semi',.5),drop(m,'model',124),knob(k3,.7),knob(k4,.45)])
osc+=row(110,262,[knob('sub',.3,30),knob('noise',0,30),knob('voices',.4,30),knob('detune',.5,48),knob('spread',.6,30)])
H.append(card(20,78,432,340,'OSCILLATORS',osc,chip='retrig'))

def envlabel(name, sub):
    return f'<div style="width:50px;display:flex;flex-direction:column;gap:2px"><span style="font-size:12px;font-weight:900;color:#7cc4ff">{name}</span><span style="font-size:9px;font-weight:700;color:#8f99a3">{sub}</span></div>'
def target(text, fixed, w=118, fs=11):
    bg = '#15181b' if fixed else '#1f6bc4'
    arrow = '' if fixed else '<span>▾</span>'
    return f'<div style="display:flex;flex-direction:column;align-items:center;gap:4px"><span style="width:{w}px;height:26px;border-radius:4px;background:{bg};border:1px solid #2a3a4a;display:flex;align-items:center;justify-content:space-between;padding:0 8px;box-sizing:border-box;font-size:{fs}px;font-weight:800;white-space:nowrap;color:{"#9fd2ff" if fixed else "#ffffff"}">→ {text}{arrow}</span><span style="font-size:10px;font-weight:700;color:#aab3bb">send to</span></div>'
adsr = lambda vals: [knob(l, v, 30, 38) for l, v in zip(['a','d','s','r'], vals)]
env = row(10, 36, [envlabel('AMP', 'volume')] + adsr([.02,.35,.3,.25]) + [knob('vel', .6, 30, 46), target('Volume', True, 92)], 2)
env += row(10, 98, [envlabel('FILTER', 'cutoff')] + adsr([.05,.45,.2,.3]) + [knob('amount', .7, 30, 46), target('Cutoff', True, 92)], 2)
env += row(10, 160, [envlabel('MOD', 'free')] + adsr([0,.5,0,.2]) + [knob('amount', .5, 30, 46), target('Pitch 2', False, 92)], 2)
env += '<div style="position:absolute;left:10px;right:10px;top:94px;height:1px;background:#1a1d20"></div><div style="position:absolute;left:10px;right:10px;top:156px;height:1px;background:#1a1d20"></div>'
H.append(card(806,78,374,226,'ENVELOPES',env,chip='sync'))
def smalldrop(v, label, w):
    return f'<div style="display:flex;flex-direction:column;align-items:center;gap:4px"><span style="width:{w}px;height:24px;border-radius:4px;background:#1f6bc4;display:flex;align-items:center;justify-content:space-between;padding:0 6px;box-sizing:border-box;font-size:10px;font-weight:800">{v}<span>▾</span></span><span style="font-size:10px;font-weight:700;color:#aab3bb">{label}</span></div>'
def sync(syn, rtg):
    on = 'border:1px solid #3fa9ff;color:#7cc4ff'; off = 'border:1px solid #3a3f45;color:#8f99a3'
    return f'<div style="font-size:8px;font-weight:800;display:flex;flex-direction:column;gap:3px"><span style="padding:1px 3px;border-radius:3px;{on if syn else off}">syn</span><span style="padding:1px 3px;border-radius:3px;{on if rtg else off}">rtg</span></div>'
def lfolabel(n):
    return f'<div style="width:38px;display:flex;align-items:center;gap:5px"><span style="width:8px;height:8px;border-radius:50%;background:#3fa9ff;box-shadow:0 0 6px #3fa9ff"></span><span style="font-size:12px;font-weight:900;color:#7cc4ff">{n}</span></div>'
lfo = ''
for i, (n, shape, rate, amt, syn, rtg, dest) in enumerate([('1', 'tri', .6, .3, True, True, 'Pitch 1+2'), ('2', 'sine', .4, .6, False, True, 'Cutoff 1'), ('3', 'saw', .3, .2, False, False, 'PW 1+2')]):
    lfo += row(10, 36 + i * 62, [lfolabel(n), smalldrop(shape, 'shape', 56), knob('rate', rate, 30, 44), knob('amount', amt, 30, 48), sync(syn, rtg),
                                  target(dest, False, 84, 10)], 4)
lfo += '<div style="position:absolute;left:10px;right:10px;top:94px;height:1px;background:#1a1d20"></div><div style="position:absolute;left:10px;right:10px;top:156px;height:1px;background:#1a1d20"></div>'
H.append(card(462, 78, 334, 226, 'LFOS', lfo))
H.append(card(462,314,426,104,'FILTER',row(10,34,[drop('LP 24 dB','type',104),knob('cutoff',.55,44),knob('reso',.3,32),knob('sat',.2,32),knob('keytrk',.5,32),knob('veltrk',.5,32)],6),power=True,chip='F1 · F2 · GLOBAL'))
H.append(card(898,314,282,104,'AMPLIFIER',row(6,36,[knob(l,v,32,54) for l,v in [('volume',.75),('pan',.5),('veltrk',.6),('mono hz',.4),('ph lock',.3)]],0)))
steps=''.join(f'<div style="flex-grow:1;border-radius:2px;background:{"#3fa9ff" if v else "#1d2125"}"></div>' for v in [1,0,1,1,0,1,0,1,1,0,1,0,1,1,0,1])
seq=row(10,38,[knob('gate',.75,34,48),knob('swing',.15,34,48),knob('octave',.33,34,48),drop('1/16','rate',64),drop('C','key',48),drop('minor','scale',78),'<div style="display:flex;flex-direction:column;align-items:center;gap:4px"><span style="width:46px;height:26px;border-radius:4px;background:#3fa9ff;color:#06121e;box-shadow:0 0 10px rgba(63,169,255,.6);display:flex;align-items:center;justify-content:center;font-size:11px;font-weight:900">ON</span><span style="font-size:10px;font-weight:700;color:#aab3bb">scale lock</span></div>'],4)+f'<div style="position:absolute;left:12px;top:118px;width:406px;height:36px;border-radius:4px;background:#0b0d0f;display:flex;gap:2px;padding:3px;box-sizing:border-box">{steps}</div>'
btns='<div style="position:absolute;right:10px;top:7px;display:flex;gap:6px"><span style="height:22px;padding:0 10px;border-radius:5px;background:#3c4146;border:1px solid #0a0b0c;display:flex;align-items:center;font-size:10px;font-weight:800">RANDOMISE</span><span style="height:22px;padding:0 10px;border-radius:5px;background:#1f6bc4;display:flex;align-items:center;font-size:10px;font-weight:800">&#10303; DRAG MIDI</span></div>'
H.append(card(20,428,432,178,'STEP SEQUENCER',seq+btns,power=True))
H.append(card(462,428,290,178,'DELAY',row(6,36,[knob(l,v,32,55) for l,v in [('send',.45),('fbk',.38),('depth',.1),('color',.5),('width',.5)]],0)+row(16,116,[drop('Pattern 3+4','type',118),drop('1/16','time',118)],14),power=True))
H.append(card(762,428,186,178,'REVERB',row(4,36,[knob(l,v,30) for l,v in [('mix',.3),('decay',.64),('pre-dly',.2)]],10)+row(4,104,[knob(l,v,30) for l,v in [('damp',.15),('color',.65),('wide',.5)]],10),power=True,chip='hall'))
H.append(card(958,428,222,178,'DISTORTION','<div style="position:absolute;left:18px;top:40px;width:186px;height:40px;border-radius:4px;background:#1f6bc4"></div>'+row(56,92,[knob('drive',.55,44),knob('mix',.8,34)],10),power=False,chip='soft'))
H.append(card(20,616,432,96,'ARPEGGIATOR',row(12,40,[drop('up','mode',94),drop('1/16','speed',94),drop('75 %','gate',94),drop('2','octave',94)],8),power=True,chip='retrig'))
H.append(card(462,616,124,96,'GLIDE',row(14,40,[drop('40 ms','time',94)]),power=True))
H.append(card(596,616,232,96,'PITCH',row(14,40,[drop('±2','bend',80),knob('transpose',.5,30,56),knob('fine',.5,30)],6)))
H.append(card(838,616,124,96,'VOICES',row(14,40,[drop('16','poly',94)])))
H.append('<div style="position:absolute;left:990px;top:640px;font-weight:900;font-style:italic;font-size:28px;color:#4a5057">vc<span style="font-weight:400">clone</span></div>')

def tabs(names, active, on, y=34, x=12, w=408):
    tw=(w-(len(names)-1)*6)//len(names); out=''
    for i,n in enumerate(names):
        act=i==active
        dot=f'<span style="width:8px;height:8px;border-radius:50%;background:{"#3fa9ff" if on[i] else "#3a3f45"};box-shadow:{"0 0 6px #3fa9ff" if on[i] else "none"}"></span>'
        out+=f'<div style="position:absolute;left:{x+i*(tw+6)}px;top:{y}px;width:{tw}px;height:28px;box-sizing:border-box;border-radius:5px;display:flex;align-items:center;justify-content:center;gap:7px;font-size:12px;font-weight:900;background:{"#15181b" if act else "#1d2024"};border:1px solid {"#3fa9ff" if act else "#0c0d0f"};color:{"#7cc4ff" if act else "#8f99a3"};{"box-shadow:0 0 8px rgba(63,169,255,.45)" if act else ""}">{dot}{n}</div>'
    return out
def scope(y, kind='saw', x=12, w=408, h=66):
    pts=[]
    for i in range(201):
        t=i/200; p=(t*3)%1
        v={'saw':1-2*p,'sq':(1 if p<.5 else -1),'wt':math.sin(2*math.pi*p)*0.6+math.sin(6*math.pi*p)*0.3}[kind]
        pts.append(f'{6+t*(w-12):.1f},{h/2-v*(h/2-8):.1f}')
    return f'<svg style="position:absolute;left:{x}px;top:{y}px;background:#0b0d0f;border-radius:4px;filter:drop-shadow(0 0 3px rgba(63,169,255,.5))" width="{w}" height="{h}"><polyline fill="none" stroke="#3fa9ff" stroke-width="2" points="{" ".join(pts)}"/></svg>'
def toggle(text,on,label):
    st='background:#3fa9ff;color:#06121e;box-shadow:0 0 10px rgba(63,169,255,.6)' if on else 'background:#1d2024;color:#8f99a3;border:1px solid #3a3f45'
    return f'<div style="display:flex;flex-direction:column;align-items:center;gap:4px"><span style="width:44px;height:24px;box-sizing:border-box;border-radius:4px;{st};display:flex;align-items:center;justify-content:center;font-size:10px;font-weight:900">{text}</span><span style="font-size:10px;font-weight:700;color:#aab3bb">{label}</span></div>'
def hbtn(text, blue=False):
    return f'<span style="position:absolute;right:10px;top:7px;height:20px;padding:0 9px;border-radius:5px;background:{"#1f6bc4" if blue else "#3c4146"};display:flex;align-items:center;font-size:10px;font-weight:800">{text}</span>'
sep=lambda y: f'<div style="position:absolute;left:10px;right:10px;top:{y}px;height:1px;background:#1a1d20"></div>'

if MODE!='virus':
    drop_titles=['OSCILLATORS','ENVELOPES','>LFOS','>FILTER','AMPLIFIER']
    H=[h for h in H if not any(t in h for t in drop_titles)]
    name={'sylenth':'LD HydroSphere','spire':'LD 2000'}[MODE]
    H[0]=H[0].replace('VIRUS ▾',MODE.upper()+' ▾').replace('Pacific KS',name)
    k=lambda l,v,s=32,w=None: knob(l,v,s,w)
    if MODE=='sylenth':
        osc=tabs(['A1','A2','B1','B2'],0,[1,1,1,0])+scope(70,'saw')
        osc+=row(12,146,[drop('Saw','wave',96),k('voices',.5,32,52),k('detune',.35,32,52),k('octave',.5,32,52),k('note',.5,32,52),k('fine',.5,32,52)],4)
        osc+=row(12,214,[k('volume',.8,32,52),k('pan',.5,32,52),k('phase',.0,32,52),k('stereo',.6,32,52),toggle('ON',True,'retrig'),toggle('OFF',False,'invert')],8)
        osc+=sep(278)+row(12,286,[k('mix A',.8,30,52),k('mix B',.6,30,52),drop('A → B','filter input',96)],8)
        H.append(card(20,78,432,340,'OSCILLATORS',osc,chip='part A · part B'))
        lfo=''
        for i,(n,shape,rate,amt,syn,rtg,dest) in enumerate([('1','tri',.55,.3,True,True,'Cutoff AB'),('2','sine',.35,.5,False,True,'Pitch AB')]):
            lfo+=row(10,40+i*78,[lfolabel(n),smalldrop(shape,'shape',56),knob('rate',rate,30,44),knob('amount',amt,30,48),sync(syn,rtg),target(dest,False,84,10)],4)
        lfo+=sep(106)+sep(170)+row(10,190,[ '<span style="font-size:10px;font-weight:800;color:#8f99a3">2nd target</span>', target('Pan AB',False,84,10).replace('send to','LFO 1'), target('none',False,84,10).replace('send to','LFO 2')],10).replace('top:190px','top:176px')
        H.append(card(462,78,334,226,'LFOS',lfo))
        envr=lambda y,lab,sub,vals,last: row(10,y,[envlabel(lab,sub)]+[knob(l,v,26,36) for l,v in zip(['a','d','s','r'],vals)]+last,2)
        env=envr(34,'AMP A','volume A',[.02,.35,.3,.25],[knob('vel',.6,26,46),target('Volume A',True,92)])
        env+=envr(82,'AMP B','volume B',[.02,.4,.5,.3],[knob('vel',.6,26,46),target('Volume B',True,92)])
        env+=envr(130,'MOD 1','filter',[.05,.45,.2,.3],[knob('amount',.7,26,46),target('Cutoff AB',False,92)])
        env+=envr(178,'MOD 2','free',[0,.5,0,.2],[knob('amount',.5,26,46),target('Pitch AB',False,92)])
        env+=sep(78)+sep(126)+sep(174)
        H.append(card(806,78,374,226,'ENVELOPES',env,chip='sync'))
        H.append(card(462,314,426,104,'FILTER',row(10,34,[drop('Lowpass','type',96),drop('24 dB','slope',64),knob('cutoff',.55,44),knob('reso',.3,32),knob('drive',.2,32),knob('keytrk',.5,32)],6),power=True,chip='A · B · CONTROL'))
        H.append(card(898,314,282,104,'AMPLIFIER',row(6,36,[knob(l,v,32,54) for l,v in [('volume',.75),('pan',.5),('veltrk',.6),('mono hz',.4),('ph lock',.3)]],0)))
    else:
        osc=tabs(['OSC 1','OSC 2','OSC 3','OSC 4'],0,[1,1,1,0])+scope(70,'wt')
        osc+=row(12,146,[drop('Classic','mode',96),k('ctrl A',.4,32,52),k('ctrl B',.6,32,52),k('octave',.5,32,52),k('note',.5,32,52),k('fine',.5,32,52)],4)
        osc+=row(12,214,[k('volume',.8,32,52),k('pan',.5,32,52),k('phase',0,32,52),k('wt mix',.3,32,52),toggle('ON',True,'retrig'),k('fm',.0,32,52)],6)
        osc+=sep(278)+row(12,286,[drop('Hyper','unison',86),k('voices',.6,30,46),k('detune',.4,30,46),k('density',.5,30,50),k('wide',.7,30,46)],6)
        H.append(card(20,78,432,340,'OSCILLATORS',osc,chip='unison'))
        lfo=''
        for i,(n,shape,rate,amt,syn,rtg,dest) in enumerate([('1','tri',.55,.3,True,True,'Cutoff 1'),('2','sine',.35,.5,False,True,'Pitch 1-4'),('3','saw',.2,.2,False,False,'Ctrl A 1'),('4','sq',.7,.1,True,True,'Pan')]):
            lfo+=row(10,32+i*47,[lfolabel(n),smalldrop(shape,'shape',56),knob('rate',rate,26,44),knob('amount',amt,26,48),sync(syn,rtg),target(dest,False,84,10)],4).replace('font-size:10px;font-weight:700;color:#aab3bb">shape','font-size:9px;font-weight:700;color:#aab3bb">shape')
        lfo+=sep(76)+sep(123)+sep(170)
        H.append(card(462,78,334,226,'LFOS',lfo+hbtn('MATRIX ▸',True)))
        envr=lambda y,lab,sub,vals,last: row(10,y,[envlabel(lab,sub)]+[knob(l,v,26,36) for l,v in zip(['a','d','s','r'],vals)]+last,2)
        env=envr(34,'AMP','volume',[.02,.35,.3,.25],[knob('vel',.6,26,46),target('Volume',True,92)])
        env+=envr(82,'ENV 1','filter',[.05,.45,.2,.3],[knob('amount',.7,26,46),target('Cutoff 1',False,92)])
        env+=envr(130,'ENV 2','free',[0,.5,0,.2],[knob('amount',.5,26,46),target('Pitch 1-4',False,92)])
        env+=envr(178,'ENV 3·4','free',[0,.3,.6,.4],[knob('amount',.4,26,46),target('Ctrl B 1',False,92)])
        env+=sep(78)+sep(126)+sep(174)
        H.append(card(806,78,374,226,'ENVELOPES',env,chip='sync'))
        H.append(card(462,314,426,104,'FILTER',row(10,34,[drop('LP 24','type',96),drop('Serial','routing',72),knob('cutoff',.55,44),knob('reso',.3,32),knob('drive',.2,32),knob('keytrk',.5,32)],6),power=True,chip='F1 · F2'))
        H.append(card(898,314,282,104,'AMPLIFIER',row(6,36,[knob(l,v,32,54) for l,v in [('volume',.75),('pan',.5),('veltrk',.6),('mono hz',.4),('ph lock',.3)]],0)))

open(f'mock_{MODE}.html','w').write('<html><body style="margin:0;background:#111"><div style="width:1200px;height:732px;position:relative;background:radial-gradient(ellipse at 30% 0%,#30353b,#1a1c1f 70%);color:#e8edf2;font-family:Inter,DejaVu Sans,sans-serif">'+''.join(H)+'</div></body></html>')
