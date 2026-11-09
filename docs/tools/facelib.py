import re, numpy as np
FACE=('<svg class="fb" viewBox="0 0 64 56" aria-hidden="true">'
 '<path class="mn" d="M22 15Q22 4 27 9Q29 0 33 8Q36 0 39 9Q43 4 42 15Z" fill="var(--mane)"/>'
 '<path d="M8 26L4 4Q16 6 24 14Z" fill="var(--body)"/><path d="M10 22L8 9Q15 11 20 16Z" fill="var(--ear)"/>'
 '<path d="M56 26L60 4Q48 6 40 14Z" fill="var(--body)"/><path d="M54 22L56 9Q49 11 44 16Z" fill="var(--ear)"/>'
 '<ellipse cx="32" cy="32" rx="26" ry="22" fill="var(--body)"/>'
 '<ellipse cx="32" cy="50" rx="14" ry="6" fill="var(--belly)"/>'
 '<g class="p p-spots" fill="var(--patc)"><circle cx="13" cy="33" r="2.6"/><circle cx="18" cy="43" r="2.1"/><circle cx="51" cy="33" r="2.6"/><circle cx="46" cy="43" r="2.1"/><circle cx="10" cy="24" r="1.8"/><circle cx="54" cy="24" r="1.8"/><circle cx="32" cy="14" r="2"/></g>'
 '<g class="p p-stripes" stroke="var(--patc)" stroke-width="2.6" stroke-linecap="round" fill="none"><path d="M8 32q4 2 7-1"/><path d="M10 40q4 2 7-1"/><path d="M56 32q-4 2-7-1"/><path d="M54 40q-4 2-7-1"/><path d="M26 11q6-2 12 0"/></g>'
 '<g class="p p-patches" fill="var(--patc)" opacity=".75"><ellipse cx="14" cy="38" rx="6" ry="5"/><ellipse cx="50" cy="26" rx="5" ry="6"/><ellipse cx="42" cy="46" rx="6" ry="4"/></g>'
 '<rect x="12" y="20" width="40" height="20" rx="10" fill="var(--plate)"/>'
 '<g><circle cx="23" cy="29" r="7" fill="#fff"/><circle cx="23" cy="29" r="4.3" fill="var(--iris)"/><circle cx="23" cy="29" r="1.9" fill="#111"/><circle class="lid" cx="23" cy="29" r="7.4" fill="var(--plate)"/><path class="lash" d="M16.5 30q6.5 5 13 0" stroke="#222" stroke-width="1.6" fill="none" stroke-linecap="round"/></g>'
 '<g><circle cx="41" cy="29" r="7" fill="#fff"/><circle cx="41" cy="29" r="4.3" fill="var(--iris)"/><circle cx="41" cy="29" r="1.9" fill="#111"/><circle class="lid" cx="41" cy="29" r="7.4" fill="var(--plate)"/><path class="lash" d="M34.5 30q6.5 5 13 0" stroke="#222" stroke-width="1.6" fill="none" stroke-linecap="round"/></g>'
 '<path d="M26 35h12l-6 5z" fill="#f2b830"/><path d="M27.5 39h9l-4.5 4.5z" fill="#e39a1c"/>'
 '<g class="h h-santa"><path d="M49 17C47 4 35 -3 23 -1C18 0 16 4 17 8C18 12 13 14 14 18Z" fill="#d6302a"/><rect x="12" y="14" width="40" height="7" rx="3.5" fill="#fff"/><circle cx="18" cy="-1" r="4.2" fill="#fff"/></g>'
 '</svg>')

def make(P, iris='#3b8fe0', awake=True):
    """P = [body, belly, ear, plate, pattern, patc, mane, hat]  -> inner svg markup with colours substituted"""
    P=list(P)+[None]*(8-len(P))
    body,belly,ear,plate,pat,patc,mane,hat=P
    x=FACE
    for k,v in {'--body':body,'--belly':belly,'--ear':ear,'--plate':plate,'--patc':patc or '#000','--iris':iris,'--mane':mane or body}.items():
        x=x.replace('var(%s)'%k,v)
    for p in ('spots','stripes','patches'):
        if pat!=p: x=re.sub(r'<g class="p p-%s".*?</g>'%p,'',x,flags=re.S)
    x=x.replace('class="p p-%s"'%pat,'')
    if not mane: x=re.sub(r'<path class="mn"[^>]*/>','',x)
    else: x=x.replace(' class="mn"','')
    if hat!='santa': x=re.sub(r'<g class="h h-santa">.*?</g>','',x,flags=re.S)
    else: x=x.replace(' class="h h-santa"','')
    if awake:
        x=re.sub(r'<circle class="lid"[^>]*/>','',x); x=re.sub(r'<path class="lash"[^>]*/>','',x)
    return x.replace('<svg class="fb" viewBox="0 0 64 56" aria-hidden="true">','').replace('</svg>','')

def hexc(rgb): return '#%02x%02x%02x'%tuple(int(round(v)) for v in rgb)
def lum(rgb): return 0.299*rgb[0]+0.587*rgb[1]+0.114*rgb[2]
