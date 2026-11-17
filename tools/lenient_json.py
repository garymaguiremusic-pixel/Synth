import json,re,sys
def load(path):
    s=open(path,encoding='utf-8-sig').read()
    out=[];i=0;n=len(s);ins=False
    while i<n:
        c=s[i]
        if ins:
            out.append(c)
            if c=='\\': out.append(s[i+1]); i+=2; continue
            if c=='"': ins=False
            i+=1; continue
        if c=='"': ins=True; out.append(c); i+=1; continue
        if s.startswith('//',i):
            while i<n and s[i]!='\n': i+=1
            continue
        if s.startswith('/*',i):
            i=s.index('*/',i)+2; continue
        out.append(c); i+=1
    t=''.join(out)
    t=re.sub(r',(\s*[\]}])',r'\1',t)
    t=t.strip()
    if not t.startswith('{'): t='{'+t+'}'
    return json.loads(t)
