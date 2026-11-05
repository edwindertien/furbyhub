#pragma once
// The dashboard, embedded in flash. Dark card look adapted from the picoControl
// vacuum-robot remote. Each furby tile shows its eyes: closed = asleep (as far
// as we know), open = awake, dashes = unknown. Furbies cannot report back over
// the signal wire, so these states are "what we last told it", not a sensor.

static const char DASH_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#0b0f14">
<title>reFurbyshment</title>
<style>
:root{
  --bg:#0b0f14; --text:#e8f0fb; --muted:#7f93a8; --accent:#4aa3ff; --ok:#35d07f; --warn:#ffb454;
  --border:rgba(255,255,255,.09); --card:rgba(255,255,255,.04); --shadow:0 12px 30px rgba(0,0,0,.45);
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{
  margin:0;min-height:100vh;display:flex;justify-content:center;padding:16px;
  font-family:system-ui,-apple-system,"Segoe UI",Roboto,Arial,sans-serif;color:var(--text);
  background:radial-gradient(1200px 800px at 20% 0%,rgba(74,163,255,.16),transparent 60%),
             radial-gradient(900px 700px at 90% 20%,rgba(53,208,127,.11),transparent 55%),var(--bg);
}
.wrap{width:min(980px,100%);display:grid;gap:14px;align-content:start}
header{display:flex;align-items:flex-end;justify-content:space-between;gap:12px;padding:8px 6px 0}
h1{margin:0;font-size:20px;letter-spacing:.2px}
.sub{color:var(--muted);font-size:12px;margin-top:4px}
.pill{font-size:12px;color:var(--muted);border:1px solid var(--border);padding:8px 11px;border-radius:999px;background:var(--card);white-space:nowrap}
.pill.ok{color:var(--ok);border-color:rgba(53,208,127,.45)}
.grid{display:grid;grid-template-columns:1.15fr .85fr;gap:14px}
@media(max-width:820px){.grid{grid-template-columns:1fr}}
.col{display:grid;gap:14px;align-content:start}
.card{background:linear-gradient(180deg,rgba(255,255,255,.05),rgba(255,255,255,.02));border:1px solid var(--border);border-radius:18px;box-shadow:var(--shadow)}
.head{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:14px 14px 10px;border-bottom:1px solid var(--border)}
.title{font-size:14px;font-weight:600}
.hint{font-size:12px;color:var(--muted)}
.body{padding:14px}

/* furby tiles */
.tiles{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}
.tile{
  position:relative;border:1px solid var(--border);border-radius:16px;background:rgba(0,0,0,.2);color:var(--text);
  padding:10px 6px 9px;display:flex;flex-direction:column;align-items:center;gap:6px;cursor:pointer;font:inherit;
  transition:border-color .15s,background .15s,transform .05s;
}
.tile:active{transform:scale(.97)}
.tile svg{width:44px;height:18px;overflow:visible}
.tile .n{font-size:17px;font-weight:600;letter-spacing:.3px}
.tile .s{font-size:11px;color:var(--muted);min-height:13px}
.tile.awake{border-color:rgba(53,208,127,.4)}
.tile.awake .s{color:var(--ok)}
.tile.sel{border-color:var(--accent);background:rgba(74,163,255,.14);box-shadow:0 0 0 1px var(--accent) inset}
.tile.off{border-style:dashed;opacity:.35;cursor:default}
.tile.busy::after{content:"";position:absolute;top:7px;right:7px;width:7px;height:7px;border-radius:50%;background:var(--warn)}
.eye-open{fill:var(--ok)} .eye-closed{fill:none;stroke:var(--muted);stroke-width:2.2;stroke-linecap:round}
.eye-unk{stroke:#516173;stroke-width:2.2;stroke-linecap:round}
.row{display:flex;gap:10px;flex-wrap:wrap;margin-top:12px}
.row:first-child{margin-top:0}

/* controls */
button.b,select,input.t{
  font:inherit;font-size:14px;color:var(--text);border:1px solid var(--border);border-radius:14px;
  background:rgba(255,255,255,.06);padding:12px 14px;min-height:46px;
}
button.b{cursor:pointer;flex:1 1 120px;transition:background .15s,border-color .15s,transform .05s}
button.b:active:not(:disabled){transform:scale(.98)}
button.b:disabled{opacity:.4;cursor:default}
button.go{background:rgba(74,163,255,.18);border-color:rgba(74,163,255,.55)}
button.warm{background:rgba(255,180,84,.12);border-color:rgba(255,180,84,.5)}
button.quiet{flex:0 0 auto;min-height:38px;padding:8px 12px;font-size:13px}
select{flex:1 1 160px;min-width:0}
input.t{flex:1 1 160px;min-width:0;min-height:42px;font-family:ui-monospace,Menlo,Consolas,monospace;font-size:13px}
:focus-visible{outline:2px solid var(--accent);outline-offset:2px}

.kv{display:grid;gap:10px}
.line{display:flex;justify-content:space-between;align-items:center;gap:12px;font-size:14px}
.line .k{color:var(--muted)}
.switch{width:52px;height:30px;border-radius:999px;border:1px solid rgba(255,255,255,.14);background:rgba(255,255,255,.06);position:relative;flex:0 0 auto;cursor:pointer;padding:0}
.switch i{width:24px;height:24px;border-radius:50%;background:rgba(255,255,255,.25);position:absolute;top:2px;left:3px;transition:left .2s,background .2s}
.switch.on{background:rgba(53,208,127,.2);border-color:rgba(53,208,127,.55)}
.switch.on i{left:25px;background:rgba(53,208,127,.75)}
.note{font-size:12px;color:var(--muted);line-height:1.45;margin:10px 0 0}
.fb{font-size:12px;color:var(--muted);min-height:16px;margin-top:10px}

details{border-top:1px solid var(--border)}
summary{cursor:pointer;padding:13px 14px;font-size:14px;font-weight:600;list-style:none}
summary::-webkit-details-marker{display:none}
summary::after{content:"+";float:right;color:var(--muted)}
details[open] summary::after{content:"\2212"}
.conn{display:grid;grid-template-columns:repeat(8,1fr);gap:6px}
.conn label{display:flex;flex-direction:column;align-items:center;gap:3px;font-size:11px;color:var(--muted)}
.conn input{width:20px;height:20px;accent-color:var(--accent);margin:0}
pre{margin:10px 0 0;max-height:240px;overflow:auto;white-space:pre-wrap;font-size:11.5px;line-height:1.4;color:#b9c8d8;
    background:rgba(0,0,0,.28);border:1px solid var(--border);border-radius:12px;padding:10px;font-family:ui-monospace,Menlo,Consolas,monospace}
footer{color:var(--muted);font-size:12px;padding:2px 6px 12px;display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap}
a{color:var(--accent);text-decoration:none}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <div>
      <h1>reFurbyshment</h1>
      <div class="sub" id="unit">Retirement home for 1999 furbies</div>
    </div>
    <div class="pill" id="pill">Connecting&#8230;</div>
  </header>

  <div class="grid">
    <div class="col">
      <section class="card">
        <div class="head"><div class="title">Residents</div><div class="hint" id="selhint">tap to select</div></div>
        <div class="body">
          <div class="tiles" id="tiles"></div>
          <div class="row">
            <button class="b quiet" id="all">Select all</button>
            <button class="b quiet" id="none">Clear</button>
          </div>
        </div>
      </section>
    </div>

    <div class="col">
      <section class="card">
        <div class="head"><div class="title">Selected furbies</div><div class="hint" id="count">none selected</div></div>
        <div class="body">
          <div class="row">
            <button class="b go" id="wake" disabled>Wake up</button>
            <button class="b" id="sleep" disabled>Send to sleep</button>
          </div>
          <div class="row">
            <select id="msg" aria-label="Message"></select>
            <button class="b" id="send" disabled>Send message</button>
          </div>
          <div class="fb" id="fb" aria-live="polite"></div>
        </div>
      </section>

      <section class="card">
        <div class="head"><div class="title">Whole home</div><div class="hint" id="home">&#8230;</div></div>
        <div class="body">
          <div class="row">
            <button class="b go" id="wakeall">Wake everyone</button>
            <button class="b" id="sleepall">Everyone to sleep</button>
          </div>
          <div class="kv" style="margin-top:14px">
            <div class="line"><span>Run by itself</span><button class="switch" id="auto" role="switch" aria-checked="false" aria-label="Run by itself"><i></i></button></div>
            <div class="line"><span class="k">Presence sensor</span><span id="sensor">&#8230;</span></div>
          </div>
          <p class="note">When on, this unit wakes the furbies when someone walks up and puts them to sleep again a minute after the last movement. Turn it off to stay in manual control.</p>
        </div>
      </section>
    </div>
  </div>

  <section class="card">
    <details>
      <summary>Which furbies are connected</summary>
      <div class="body"><div class="conn" id="conn"></div>
        <p class="note">Unticked furbies are skipped by &ldquo;Wake everyone&rdquo; and the automatic mode.</p></div>
    </details>
    <details>
      <summary>Console</summary>
      <div class="body">
        <div class="row">
          <input class="t" id="cmd" placeholder="status, help, sweep 1, timing pulse 120" autocapitalize="off" autocomplete="off" spellcheck="false">
          <button class="b quiet go" id="run">Run</button>
        </div>
        <pre id="out">Same commands as the USB serial console. Try: help</pre>
      </div>
    </details>
  </section>

  <footer><div>Tip: add to home screen for a full-screen remote.</div><div><a href="/api/status" target="_blank" rel="noopener">/api/status</a></div></footer>
</div>

<script>
(() => {
  const $ = s => document.querySelector(s);
  const MSG = [[0,'a','Say my name'],[1,'b',''],[2,'c',''],[3,'d',''],[4,'e',''],[5,'f','Dance'],[6,'g',''],[7,'h',''],
               [8,'i','Sneeze'],[9,'j',''],[10,'k',''],[11,'l','Ba ba black sheep'],[12,'m',''],[13,'n',''],[14,'o','Go to bed'],[15,'p','Sleep']];
  const EYES = {
    0: '<svg viewBox="0 0 44 18"><path class="eye-unk" d="M6 9h10M28 9h10" fill="none"/></svg>',
    1: '<svg viewBox="0 0 44 18"><path class="eye-closed" d="M5 7q6 7 12 0M27 7q6 7 12 0"/></svg>',
    2: '<svg viewBox="0 0 44 18"><circle class="eye-open" cx="11" cy="9" r="5"/><circle class="eye-open" cx="33" cy="9" r="5"/></svg>'
  };
  const WORD = {0:'unknown',1:'asleep',2:'awake'};
  const sel = new Set();
  let st = null, tiles = [], failures = 0, fbTimer = 0;

  // build tiles and message list once
  const tilesEl = $('#tiles'), connEl = $('#conn');
  for (let i = 1; i <= 16; i++) {
    const b = document.createElement('button');
    b.className = 'tile'; b.type = 'button'; b.setAttribute('aria-pressed', 'false');
    b.innerHTML = '<span class="eyes"></span><span class="n">' + i + '</span><span class="s"></span>';
    b.addEventListener('click', () => { if (b.classList.contains('off')) return; sel.has(i) ? sel.delete(i) : sel.add(i); paint(); });
    tilesEl.appendChild(b); tiles.push(b);
    const l = document.createElement('label');
    l.innerHTML = '<input type="checkbox" data-n="' + i + '">' + i;
    connEl.appendChild(l);
  }
  connEl.addEventListener('change', e => { const n = e.target.dataset.n; if (n) { sel.delete(+n); run((e.target.checked ? 'enable ' : 'disable ') + n, 1); } });
  MSG.forEach(m => { const o = document.createElement('option'); o.value = m[0]; o.textContent = m[1] + (m[2] ? '  ' + m[2] : '  (code ' + m[0] + ')'); $('#msg').appendChild(o); });
  $('#msg').value = 0;

  async function api(line) {
    const r = await fetch('/api/cli?line=' + encodeURIComponent(line), { cache: 'no-store' });
    return r.text();
  }
  function say(t) { const f = $('#fb'); f.textContent = t; clearTimeout(fbTimer); fbTimer = setTimeout(() => f.textContent = '', 4000); }
  async function run(line, quiet) {
    try { const t = await api(line); if (!quiet) say(t.trim().split('\n')[0]); poll(); return t; }
    catch (e) { say('No connection to the unit'); }
  }
  const list = () => [...sel].sort((a, b) => a - b).join(',');

  function paint() {
    const n = sel.size;
    $('#count').textContent = n ? n + ' selected' : 'none selected';
    ['wake', 'sleep', 'send'].forEach(id => $('#' + id).disabled = !n);
    tiles.forEach((t, i) => { const on = sel.has(i + 1); t.classList.toggle('sel', on); t.setAttribute('aria-pressed', on); });
  }

  function render() {
    if (!st) return;
    $('#unit').textContent = 'Unit ' + st.unit + '  \u2013  WiFi ' + st.ssid;
    const p = $('#pill'); p.textContent = st.stations + (st.stations === 1 ? ' device' : ' devices') + ' connected'; p.classList.add('ok');
    st.f.forEach((f, i) => {
      const t = tiles[i], en = (st.mask >> i) & 1;
      t.classList.toggle('off', !en);
      t.classList.toggle('awake', en && f[0] === 2);
      t.classList.toggle('busy', !!f[2]);
      t.querySelector('.eyes').innerHTML = en ? EYES[f[0]] : '';
      t.querySelector('.s').textContent = en ? WORD[f[0]] : 'unused';
      if (!en) sel.delete(i + 1);
      connEl.children[i].firstChild.checked = !!en;
    });
    const sw = $('#auto'); sw.classList.toggle('on', !!st.auto); sw.setAttribute('aria-checked', !!st.auto);
    $('#sensor').textContent = st.sensor ? 'someone is there' : 'nobody';
    $('#sensor').style.color = st.sensor ? 'var(--warn)' : '';
    let h = st.state;
    if (st.state === 'awake' && st.auto && st.remaining) h = 'awake, sleeps in ' + st.remaining + ' s';
    else if (st.state === 'idle') h = 'quiet';
    $('#home').textContent = h;
    paint();
  }

  async function poll() {
    try {
      const r = await fetch('/api/status', { cache: 'no-store' });
      st = await r.json(); failures = 0; render();
      if (st.script) { const l = await api('log'); $('#out').textContent = l; }
    } catch (e) {
      if (++failures > 2) { const p = $('#pill'); p.textContent = 'Not connected'; p.classList.remove('ok'); }
    }
  }

  $('#wake').onclick  = () => run('wake ' + list());
  $('#sleep').onclick = () => run('sleep ' + list());
  $('#send').onclick  = () => run('send ' + list() + ' ' + $('#msg').value);
  $('#wakeall').onclick  = () => run('wakeall');
  $('#sleepall').onclick = () => run('sleepall');
  $('#auto').onclick  = () => run('auto ' + (st && st.auto ? 'off' : 'on'));
  $('#all').onclick   = () => { st.f.forEach((f, i) => { if ((st.mask >> i) & 1) sel.add(i + 1); }); paint(); };
  $('#none').onclick  = () => { sel.clear(); paint(); };
  const go = async () => { const c = $('#cmd').value.trim(); if (!c) return; const r = await run(c, 1); $('#out').textContent = '> ' + c + '\n' + (r === undefined ? 'no connection to the unit' : r); };
  $('#run').onclick = go;
  $('#cmd').addEventListener('keydown', e => { if (e.key === 'Enter') go(); });

  poll(); setInterval(poll, 1000);
})();
</script>
</body>
</html>
)HTML";
