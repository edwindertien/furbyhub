import json, re, sys, os
HERE=os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0,HERE); os.chdir(HERE)
from facelib import make
D=json.load(open('final.json')); P=D['presets']
BG,CARD,BORD,TXT,MUT,ACC,OK,WARN,BAD,PUR='#0b0f14','#151b23','#2a3441','#e8f0fb','#7f93a8','#4aa3ff','#35d07f','#ffb454','#ff6b6b','#b58cff'
EYE={'blue':'#3b8fe0','green':'#3fa655','brown':'#8a5a2b','grey':'#9aa3ad'}
FONT='system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif'
MONO='ui-monospace,Menlo,Consolas,monospace'
def svg(w,h,body,defs=''):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" font-family="%s">'
            '<defs><marker id="ar" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0 0L10 5L0 10z" fill="%s"/></marker>'
            '<marker id="ag" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0 0L10 5L0 10z" fill="%s"/></marker>%s</defs>'
            '<rect width="100%%" height="100%%" fill="%s"/>%s</svg>')%(w,h,w,h,FONT,MUT,OK,defs,BG,body)
def esc(s): return s.replace('&','&amp;').replace('<','&lt;').replace('>','&gt;')
def t(x,y,s,size=13,fill=TXT,anchor='start',weight=400,mono=False,op=1):
    return '<text x="%s" y="%s" font-size="%s" fill="%s" text-anchor="%s" font-weight="%s"%s%s>%s</text>'%(x,y,size,fill,anchor,weight,' font-family="%s"'%MONO if mono else '',' fill-opacity="%s"'%op if op!=1 else '',esc(s) if '<tspan' not in s else s)
def box(x,y,w,h,fill=CARD,stroke=BORD,r=12,sw=1.2,dash=None):
    return '<rect x="%s" y="%s" width="%s" height="%s" rx="%s" fill="%s" stroke="%s" stroke-width="%s"%s/>'%(x,y,w,h,r,fill,stroke,sw,' stroke-dasharray="%s"'%dash if dash else '')
def line(x1,y1,x2,y2,c=MUT,w=2,dash=None,arrow=0,mk='ar'):
    a=''
    if arrow>=1: a+=' marker-end="url(#%s)"'%mk
    if arrow==2: a+=' marker-start="url(#%s)"'%mk
    return '<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-width="%s"%s%s/>'%(x1,y1,x2,y2,c,w,' stroke-dasharray="%s"'%dash if dash else '',a)
def path(d,c=MUT,w=2,fill='none',dash=None,arrow=0):
    return '<path d="%s" stroke="%s" stroke-width="%s" fill="%s"%s%s/>'%(d,c,w,fill,' stroke-dasharray="%s"'%dash if dash else '',' marker-end="url(#ar)"' if arrow else '')
def face(k,x,y,size,awake=True,eyes='blue'):
    h=size*56/64
    return '<svg x="%s" y="%s" width="%s" height="%s" viewBox="-3 -8 70 68" overflow="visible">%s</svg>'%(x,y,size,size*68/70,make(P[k],EYE.get(eyes,eyes),awake=awake))
def pill(x,y,w,h,label,fill='#1d2733',stroke=BORD,color=TXT,size=12): return box(x,y,w,h,fill,stroke,h/2)+t(x+w/2,y+h/2+size*.35,label,size,color,'middle')
def btn(x,y,w,h,label,primary=False,disabled=False):
    return box(x,y,w,h,'#1b3550' if primary else '#222a35','#3b78b5' if primary else BORD,12)+t(x+w/2,y+h/2+5,label,13.5,TXT,'middle',fill_op if False else 400)
def caret(x,y): return '<path d="M%s %s L%s %s L%s %s" fill="none" stroke="%s" stroke-width="1.8" stroke-linecap="round"/>'%(x-5,y-3,x,y+3,x+5,y-3,MUT)
fill_op=None
out={}

# ───────────────────────── hero ─────────────────────────
b=''
b+=t(48,86,'reFurbyshment',46,TXT,weight=700)
b+=t(50,122,'A retirement home for 1999 Furbies',20,ACC)
b+=t(50,152,'Five Pico W hubs. Sixteen Furbies each. One wire per Furby.',14.5,MUT)
b+=t(50,176,'Wake them, talk to them, let them chatter - from a phone, a sensor, or another hub.',14.5,MUT)
for i,(lab,col) in enumerate([('Pico W',ACC),('PlatformIO',OK),('OSC',PUR),('LittleFS config',WARN)]):
    x=50+i*0; 
xs=50
for lab,col in [('Raspberry Pi Pico W',ACC),('PlatformIO / arduino-pico',OK),('WiFi dashboard',PUR),('OSC between hubs',WARN)]:
    w=len(lab)*7.6+26; b+=box(xs,204,w,28,'#10161d',col,14)+t(xs+w/2,223,lab,12,col,'middle'); xs+=w+10
row=['leopard','giraffe','pink_flamingo','frog','lady_bug','bumble_bee','rooster','santa']
for i,k in enumerate(row):
    b+=face(k,772+(i%4)*104,30+(i//4)*118,100,awake=(i%3!=1))
out['hero']=svg(1200,300,b)

# ───────────────────────── dashboard mock ─────────────────────────
b=t(500,26,'Illustration of the dashboard - not a screenshot',12,MUT,'middle')
b+=t(30,70,'reFurbyshment',22,TXT,weight=700)+t(30,92,'Hall  –  Unit 1  –  reFurby-1',12,MUT)
b+=pill(770,52,200,34,'2 devices connected',color=OK,stroke='#2c6a4a')
# residents card
b+=box(24,110,560,512)+t(44,140,'Residents',14,TXT,weight=600)+t(564,140,'tap to select',12,MUT,'end')+line(24,152,584,152,BORD,1)
names=['leopard','giraffe','pink_flamingo','frog','lady_bug','bumble_bee','rooster','santa','tiger','dalmatian','koala','lion','gold_fish','blue_jay','eagle','penguin']
state=[2,2,1,2,1,0,2,2,1,1,2,0,1,2,1,2]   # 0 unknown 1 asleep 2 awake
labels={1:'Gerda',7:'Santa'}
off=[11]
for i,k in enumerate(names):
    cx=44+(i%4)*134; cy=168+(i//4)*100
    sel=i in (2,3); isoff=i in off
    stroke=ACC if sel else (('#2c6a4a') if state[i]==2 else BORD)
    fill='#17304a' if sel else '#0e1319'
    b+=box(cx,cy,122,92,fill,stroke,14,1.4 if not sel else 2,'4 4' if isoff else None)
    if not isoff:
        b+=face(k,cx+36,cy+6,50,awake=state[i]==2)
    b+=t(cx+61,cy+72,str(i+1),14,TXT,'middle',600)
    lab=labels.get(i,{0:'unknown',1:'asleep',2:'awake'}[state[i]]) if not isoff else 'unused'
    b+=t(cx+61,cy+86,lab,10.5,OK if state[i]==2 and not isoff else MUT,'middle')
b+=btn(44,578,108,30,'Select all')+btn(160,578,76,30,'Clear')
# looks card
b+='<g transform="translate(0,44)">'
b+=box(24,594,560,188)+t(44,622,'Looks of furby 3',14,TXT,weight=600)+t(564,622,'fur pattern & eyes',12,MUT,'end')+line(24,634,584,634,BORD,1)
b+=box(44,648,520,38,'#222a35',BORD,12)+t(60,672,'Name (optional)',13,MUT)
b+=box(44,696,252,38,'#222a35',BORD,12)+t(60,720,'Series 3  ›  Pink Flamingo',13,TXT)+caret(278,716)
b+=box(312,696,252,38,'#222a35',BORD,12)+t(328,720,'Blue eyes',13,TXT)+caret(546,716)
b+=btn(44,742,520,32,'Save looks',True)
b+='</g>'
# right column
b+=box(604,110,372,176)+t(624,140,'Selected furbies',14,TXT,weight=600)+t(956,140,'2 selected',12,MUT,'end')+line(604,152,976,152,BORD,1)
b+=btn(624,166,160,40,'Wake up',True)+btn(796,166,160,40,'Send to sleep')
b+=box(624,218,170,40,'#222a35',BORD,12)+t(638,243,'a  Say my name',13,TXT)+caret(776,239)+btn(806,218,150,40,'Send message')
b+=box(604,302,372,232)+t(624,332,'Whole home',14,TXT,weight=600)+t(956,332,'awake, sleeps in 41 s',12,MUT,'end')+line(604,344,976,344,BORD,1)
b+=btn(624,358,160,40,'Wake everyone',True)+btn(796,358,160,40,'Everyone to sleep')
b+=t(624,430,'Run by itself',14,TXT)+box(902,412,54,30,'#173a2a','#2c6a4a',15)+'<circle cx="941" cy="427" r="12" fill="#35d07f" fill-opacity=".8"/>'
b+=t(624,470,'Distance sensor',14,MUT)+t(956,470,'someone is there  (842 mm)',13,WARN,'end')
b+=t(624,506,'When on, this hub wakes the furbies when someone walks up.',11.5,MUT)
b+=box(604,550,372,232)+t(624,580,'Other hubs',14,TXT,weight=600)+t(956,580,'3 hubs on the network',12,MUT,'end')+line(604,592,976,592,BORD,1)
for i,(hub,txt,col) in enumerate([('Hub 2','quiet  1730 mm',MUT),('Hub 3','someone there  640 mm',WARN),('Hub 4','no signal for 31 s',BAD)]):
    y=606+i*54
    b+=box(624,y,332,42,'#10161d',BORD,12)+t(640,y+26,hub,13,TXT)+t(940,y+26,txt,13,col,'end')
out['dashboard']=svg(1000,850,b)

# ───────────────────────── architecture ─────────────────────────
b=t(550,30,'One hub: two cores, one queue, one JSON file',15,TXT,'middle',600)
# left external
ext=[('USB serial','CLI, 115200 baud',ACC),('WiFi radio (CYW43)','own AP or router client',ACC),('LittleFS','/furbyhub.json',WARN),('Sensor + button','PIR / HC-SR04, GP19-22',OK)]
for i,(a,s,c) in enumerate(ext):
    y=70+i*118; b+=box(24,y,190,84,CARD,c,14)+t(119,y+36,a,14,TXT,'middle',600)+t(119,y+58,s,11.5,MUT,'middle')
# core 0
b+=box(260,56,480,470,'#0f151c',BORD,18)+t(284,86,'CORE 0  -  Arduino setup() / loop()',13,ACC,weight=700)
mods=[('Cli','serial + web console, scripts'),('Dashboard','HTTP :80, captive DNS / mDNS, /api/*'),('Osc','UDP :9000, peers, rules'),('Sensor','median + hysteresis presence'),('Standalone','idle / waking / awake / sleeping'),('Settings','furbyhub.json  <->  RAM struct')]
for i,(a,s) in enumerate(mods):
    x=284+(i%2)*226; y=110+(i//2)*84
    b+=box(x,y,208,68,CARD,BORD,12)+t(x+14,y+28,a,14,TXT,weight=600,mono=True)+t(x+14,y+50,s,11,MUT)
b+=box(284,366,432,66,'#10161d',BORD,12)+t(300,392,'Diag',13,TXT,weight=600,mono=True)+t(300,414,'live log, breadcrumbs that survive a watchdog reset, subsystem switches',11,MUT)
b+=box(284,444,432,62,'#10161d',BORD,12)+t(300,470,'Watchdog 8 s',13,TXT,weight=600)+t(300,490,'core 0 reboots if it ever stops looping',11,MUT)
# queue
b+=box(756,230,70,130,'#1a2430',PUR,12)+t(791,282,'queue',12,PUR,'middle',600)+t(791,300,'64 actions',10.5,MUT,'middle')+t(791,316,'lock-free',10.5,MUT,'middle')
b+=line(740,295,756,295,PUR,2.5,arrow=1)+line(826,295,850,295,PUR,2.5,arrow=1)
# core 1
b+=box(850,56,226,470,'#0f151c',BORD,18)+t(870,86,'CORE 1  -  setup1()/loop1()',13,OK,weight=700)
b+=box(870,106,186,110,CARD,OK,12)+t(963,132,'FurbyBus',14,TXT,'middle',600,True)+t(963,154,'one tick = 1050 µs',11.5,MUT,'middle')+t(963,172,'steps all 16 furbies',11.5,MUT,'middle')+t(963,190,'in parallel',11.5,MUT,'middle')
for i in range(4):
    b+=box(870+i*48,234,42,34,'#10161d',BORD,8)+t(891+i*48,256,'#%d'%(i+1) if i<3 else '...',11,MUT,'middle')
b+=t(963,288,'16 FIFOs x 8 actions',11.5,MUT,'middle')+t(963,306,'Wake · Msg · Wait · Level',11,MUT,'middle')
b+=box(870,330,186,70,'#10161d',BORD,12)+t(963,356,'GPIO pulses',13,TXT,'middle',600)+t(963,378,'GP0 … GP15',12,MUT,'middle',mono=True)
b+=line(963,400,963,430,OK,2.5,arrow=1,mk='ag')
b+=box(870,432,186,74,CARD,OK,12)+t(963,462,'16 Furbies',14,TXT,'middle',600)+t(963,484,'3-wire servo leads',11.5,MUT,'middle')
# arrows left
for i in range(4):
    b+=line(214,112+i*118,260,[130,200,330,420][i],MUT,2,arrow=2)
b+=t(550,556,'Core 1 never waits for WiFi, flash or HTTP, so a pulse train is never delayed. Core 0 only enqueues small actions.',12.5,MUT,'middle')
out['architecture']=svg(1100,580,b)

# ───────────────────────── network ─────────────────────────
b=t(550,30,'Fixed network: a dedicated router, every hub is a peer',15,TXT,'middle',600)
b+=box(430,230,240,92,'#10161d',WARN,16)+t(550,264,'Furby router',16,TXT,'middle',600)+t(550,286,'dedicated, private, WPA2',12,MUT,'middle')+t(550,305,'192.168.50.0/24',12,WARN,'middle',mono=True)
# phone + laptop on top
b+=box(120,70,230,90,CARD,ACC,14)+t(235,104,'Phone / tablet',14,TXT,'middle',600)+t(235,126,'dashboard: http://furbyhub3.local',11.5,MUT,'middle')+t(235,144,'or the hub IP',11.5,MUT,'middle')
b+=box(750,70,230,90,CARD,PUR,14)+t(865,104,'Laptop (optional)',14,TXT,'middle',600)+t(865,126,'Max / TouchDesigner / QLab',11.5,MUT,'middle')+t(865,144,'listens and sends OSC',11.5,MUT,'middle')
b+=line(235,160,480,230,ACC,2,arrow=2)+line(865,160,620,230,PUR,2,arrow=2)
for i in range(5):
    x=40+i*204; hubk=['leopard','giraffe','frog','lady_bug','tiger'][i]
    b+=box(x,420,186,186,CARD,BORD,16)+t(x+93,450,'Hub %d'%(i+1),15,TXT,'middle',700)+t(x+93,470,'192.168.50.%d'%(11+i),11.5,WARN,'middle',mono=True)
    b+=face(hubk,x+28,484,50)+face(['pink_flamingo','bumble_bee','santa','dalmatian','koala'][i],x+78,484,50,awake=False)
    b+=t(x+93,562,'16 Furbies',12,TXT,'middle')+t(x+93,582,'sensor + button',11,MUT,'middle')
    b+=line(x+93,420,[455,500,550,600,645][i],322,WARN,2,arrow=2)
b+=box(60,626,470,58,'#10161d',BORD,12)+t(76,650,'every hub, every 2 s  (UDP broadcast :9000)',12,TXT,weight=600)+t(76,670,'/furby/hub/N/status  presence, distance, state, uptime',10.5,MUT,mono=True)
b+=box(570,626,470,58,'#10161d',BORD,12)+t(586,650,'commands any hub accepts',12,TXT,weight=600)+t(586,670,'/furby/hub/N|all/wake|sleep|send|wakeall|...',10.5,MUT,mono=True)
b+=t(550,712,'No router found? The hub keeps retrying and works standalone. Hold the button 3 s at power-up for its own WiFi: reFurby-N @ 192.168.42.1',12,MUT,'middle')
out['network']=svg(1100,732,b)

# ───────────────────────── wiring ─────────────────────────
b=t(550,30,'Wiring: one wire per Furby (plus power and ground)',15,TXT,'middle',600)
b+=box(24,60,330,290,CARD,ACC,16)+t(189,90,'Pico W on Waveshare Pico-Servo-Driver',13.5,TXT,'middle',600)
b+=box(48,110,282,76,'#10161d',BORD,10)+t(189,138,'Pico W (RP2040 + CYW43)',13,TXT,'middle')+t(189,160,'USB: power, CLI, firmware',11.5,MUT,'middle')
for r in range(2):
    for c in range(8):
        n=r*8+c
        b+=box(48+c*35.5,204+r*44,31,36,'#10161d',BORD,6)+t(63.5+c*35.5,226+r*44,str(n+1),11,TXT,'middle',600)+t(63.5+c*35.5,254-6+r*44-1,'',8,MUT,'middle')
b+=t(189,312,'servo ports 1-16  =  GP0 … GP15',12,MUT,'middle')+t(189,332,'(signal pin = GPIO, middle = V+, outer = GND)',10.5,MUT,'middle')
# lead
b+=t(430,120,'standard 3-wire servo lead',12,TXT,'middle',600)
for i,(c,l) in enumerate([('#f0b040','signal'),('#d6302a','V+'),('#6b4a3a','GND')]):
    y=150+i*40; b+=line(354,y,540,y,c,6)+t(447,y-10,l,11.5,MUT,'middle')
# furby inside
b+=box(540,60,536,290,CARD,OK,16)+t(808,90,'inside the Furby (the original "hack")',13.5,TXT,'middle',600)
b+=t(556,176,'battery +  (6 V)',11.5,MUT)+t(556,216,'battery -',11.5,MUT)
b+=box(900,120,150,40,'#10161d',BORD,10)+t(975,145,'IR receiver output',12,TXT,'middle')
b+=box(900,196,150,40,'#10161d',BORD,10)+t(975,221,'tilt sensor  node',12,TXT,'middle')
b+=line(540,150,800,150,'#f0b040',5)+path('M800 150 L800 140 L820 140',MUT,0)
b+=path('M800 150 L800 140',MUT,1)
b+=line(800,150,800,216,'#f0b040',3)+line(800,140,826,140,'#f0b040',3)+line(800,216,826,216,'#f0b040',3)
for y in (140,216):
    b+=box(826,y-9,56,18,'#2a2417',WARN,4)+t(854,y+4,'10 k',11,WARN,'middle')+line(882,y,900,y,'#f0b040',3)
b+=t(808,274,'signal wire  →  10 k  →  IR receiver input',11.5,MUT,'middle')+t(808,292,'signal wire  →  10 k  →  tilt input',11.5,MUT,'middle')
b+=t(808,320,'long HIGH pulse + float = wakes it · short pulses = the IR protocol',11.5,OK,'middle')
# power + sensor strips
b+=box(24,372,520,216,'#10161d',WARN,16)+t(44,400,'Power  -  read before connecting 6 V',13.5,WARN,weight=700)
for i,s in enumerate(['The servo rail is the output of the board\'s 5 V buck regulator,','and the Pico\'s VSYS shares it. 6 V into the VIN terminal gives ~5 V.','Do NOT put 6 V on the rail pins while the regulator is attached.','1) Try one Furby at 5 V first.','2) If it needs 6 V: separate 6 V bus, COMMON GROUND with the board,','   board = signal + GND only (de-pin the red wire at the board end).','Measure first: derived from the schematic netlist, not verified on the board.']):
    b+=t(44,428+i*22,s,12,TXT if i<3 else MUT,mono=False)
b+=box(560,372,516,216,'#10161d',OK,16)+t(580,400,'Hub inputs (free Pico pins)',13.5,OK,weight=700)
rows=[('GP22','PIR sensor out (sensor.type = pir), active high'),('GP20','HC-SR04 trigger (sensor.type = hcsr04)'),('GP19','HC-SR04 echo  -  3.3 V ONLY (divider for 5 V modules)'),('GP21','button to GND: wake/sleep, 3 s at boot = network reset'),('GP23-25,29','used by the WiFi chip, never touch'),('I²C','NOT on GP4/GP5 (= Furby 5 and 6): use I²C1 on GP26/GP27')]
for i,(a,s) in enumerate(rows):
    b+=t(580,430+i*26,a,12,WARN,weight=700,mono=True)+t(662,430+i*26,s,11.5,TXT)
out['wiring']=svg(1100,610,b)

# ───────────────────────── protocol ─────────────────────────
import importlib
def bitv(code,k):
    if k==0: return 1
    if k<=4: return (code>>(k-1))&1
    return 1-((code>>(k-5))&1)
code=5
b=t(550,30,'Wire protocol (as implemented from the original Mega sketch)',15,TXT,'middle',600)
# lane A: wake
b+=t(30,70,'A  Wake  -  fake the tilt sensor',13,ACC,weight=700)
y0=128;hi=96
b+=path('M60 %d H150 V%d H430 V%d H760'%(y0,hi,y0),'#f0b040',3)
b+=path('M430 %d H760'%y0,MUT,2,dash='4 5')
b+=t(300,hi-8,'HIGH 100 ms  (wake_ms)',12,TXT,'middle')+t(595,y0-8,'floating (high-Z) ≥ 50 ms, then the next action',11.5,MUT,'middle')
b+=t(60,y0+22,'The wire is only driven while talking; otherwise it is a floating input, like the original pinMode(INPUT).',11.5,MUT)
# lane B: packet
b+=t(30,190,'B  One packet = 9 bits  (here code 5, "dance")',13,ACC,weight=700)
x0=60; slot=56; ytop=250; ybase=292
labels=['start']+['d%d'%i for i in range(4)]+['~d%d'%i for i in range(4)]
for k in range(9):
    x=x0+k*2*slot; v=bitv(code,k)
    col=OK if v else '#2a3441'
    b+=box(x,206,2*slot-4,26,'#10161d',col,6)+t(x+slot-2,224,'%s = %d'%(labels[k],v),11.5,TXT if v else MUT,'middle',mono=True)
pts='M%d %d'%(x0,ybase)
for s in range(18):
    k=s//2; second=s%2; v=bitv(code,k); x=x0+s*slot
    if (not second) or v: pts+=' H%d V%d H%d V%d'%(x,ytop,x+16,ybase)
pts+=' H%d'%(x0+18*slot)
b+=path(pts,'#f0b040',2.5)
for s in range(19): b+=line(x0+s*slot,240,x0+s*slot,300,BORD,1,'2 4')
b+=t(x0,322,'150 µs pulses on a 1050 µs grid.  First half-slot: always a pulse.  Second half-slot: a pulse only if the bit is 1.',11.5,MUT)
b+=t(x0,340,'Data bits go LSB first; the next four bits are the same four bits inverted. Every packet has 14 pulses.',11.5,MUT)
# lane C: repeats
b+=t(30,388,'C  A message repeats 6 times, ~109 ms apart',13,ACC,weight=700)
for i in range(6):
    x=60+i*160; b+=box(x,404,60,30,'#3a3014',WARN,6)+t(x+30,424,'packet',11,TXT,'middle')+line(x+60,419,x+160,419,MUT,1.5,'3 4')
    b+=t(x+110,412,'90 ms',10,MUT,'middle')
b+=t(60,462,'sleep = message 15, wait 800 ms, message 15 again (a Furby that is busy ignores packets, so it is sent twice).',11.5,MUT)
b+=t(60,484,'The 9-bit structure comes from the working sketch and has NOT been independently confirmed. Every timing is tunable: timing pulse|half|repeats|gap|wake.',11.5,WARN)
out['protocol']=svg(1100,510,b)

# ───────────────────────── standalone ─────────────────────────
b=t(550,30,'Standalone behaviour of one hub',15,TXT,'middle',600)
nodes=[('Idle','furbies quiet',MUT),('Waking','one by one, 300 ms apart',ACC),('Awake','chatter every 6-20 s',OK),('Sleeping','sleep sent twice, 150 ms apart',PUR)]
for i,(a_,s_,c) in enumerate(nodes):
    x=30+i*300; b+=box(x,90,160,92,CARD,c,16,2)+t(x+80,128,a_,18,TXT,'middle',700)+t(x+80,154,s_,10,MUT,'middle')
trans=[['sensor sees someone,','button, or','"Wake everyone"'],['all furbies woken'],['awake_s (60 s) after','the last movement']]
for i,l in enumerate(trans):
    x=190+i*300; b+=line(x+8,136,x+132,136,TXT,2,arrow=1)
    for j,ln in enumerate(l): b+=t(x+70,160+j*14,ln,10.5,MUT,'middle')
b+=path('M1010 182 V236 H110 V186',PUR,2,arrow=1)+t(560,256,'every furby asleep  ->  cooldown 3 s  ->  idle again',11.5,MUT,'middle')
b+=box(30,290,1040,92,'#10161d',BORD,14)
b+=t(50,318,'auto = on',13,OK,weight=700)+t(150,318,'the sensor triggers, a present person keeps the furbies awake, the timeout puts them to sleep, they chatter at random.',12,TXT)
b+=t(50,344,'auto = off',13,WARN,weight=700)+t(150,344,'nothing happens by itself: no sensor reaction, no timeout, no chatter.',12,TXT)
b+=t(50,370,'always',13,ACC,weight=700)+t(150,370,'the button and every dashboard, CLI and OSC command work in both modes.',12,TXT)
out['standalone']=svg(1100,410,b)

for k,v in out.items():
    open(os.path.join(HERE,'..','img','%s.svg'%k),'w').write(v)
import xml.dom.minidom as m
for k in out: m.parse(os.path.join(HERE,'..','img','%s.svg'%k))
print('ok', list(out))
