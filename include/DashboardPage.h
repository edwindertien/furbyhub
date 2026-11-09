#pragma once
// The dashboard, embedded in flash. Dark card look adapted from the picoControl
// vacuum-robot remote. Each furby tile shows a small face drawn in the colours of
// its preset (fur, belly, ears, faceplate, pattern, eye colour) from furbyhub.json.
// Eyes closed = asleep (as far as we know), open = awake, half = unknown. Furbies
// cannot report back over the signal wire, so these states are "what we last told
// it", not a measurement.
//
// The preset colours are APPROXIMATIONS, not official values: series 1-4 and Santa were sampled from photos of
// real furbies, series 5-10 from a small chart (lower fidelity), and patterned fur was corrected by eye.
// Edit PRESETS to taste.

static const char DASH_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#0b0f14">
<title>reFurbyshment</title>
<style>
:root{
  --bg:#0b0f14; --text:#e8f0fb; --muted:#7f93a8; --accent:#4aa3ff; --ok:#35d07f; --warn:#ffb454; --bad:#ff6b6b;
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
  padding:8px 4px 8px;display:flex;flex-direction:column;align-items:center;gap:3px;cursor:pointer;font:inherit;min-width:0;
  transition:border-color .15s,background .15s,transform .05s;
}
.tile:active{transform:scale(.97)}
.tile .n{font-size:15px;font-weight:600;letter-spacing:.3px}
.tile .s{font-size:11px;color:var(--muted);min-height:13px;max-width:100%;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.tile.awake{border-color:rgba(53,208,127,.4)}
.tile.awake .s{color:var(--ok)}
.tile.sel{border-color:var(--accent);background:rgba(74,163,255,.14);box-shadow:0 0 0 1px var(--accent) inset}
.tile.off{border-style:dashed;opacity:.35;cursor:default}
.tile.busy::after{content:"";position:absolute;top:7px;right:7px;width:7px;height:7px;border-radius:50%;background:var(--warn)}
svg.fb{width:100%;max-width:62px;height:auto;display:block;overflow:visible}
.fb .lid{transform-box:fill-box;transform-origin:50% 0;transform:scaleY(var(--lid,.5));transition:transform .25s}
.fb .lash{opacity:var(--lash,0);transition:opacity .25s}
.fb .p{display:none}
.fb[data-pat=spots] .p-spots,.fb[data-pat=stripes] .p-stripes,.fb[data-pat=patches] .p-patches{display:inline}
.fb .mn{display:none}
.fb[data-mane="1"] .mn{display:inline}
.fb .h{display:none}
.fb[data-hat=santa] .h-santa{display:inline}
.row{display:flex;gap:10px;flex-wrap:wrap;margin-top:12px}
.row:first-child{margin-top:0}

/* controls */
button.b,select,input.t,textarea.t{
  font:inherit;font-size:14px;color:var(--text);border:1px solid var(--border);border-radius:14px;
  background:rgba(255,255,255,.06);padding:12px 14px;min-height:46px;
}
button.b{cursor:pointer;flex:1 1 120px;transition:background .15s,border-color .15s,transform .05s}
button.b:active:not(:disabled){transform:scale(.98)}
button.b:disabled{opacity:.4;cursor:default}
button.go{background:rgba(74,163,255,.18);border-color:rgba(74,163,255,.55)}
button.quiet{flex:0 0 auto;min-height:38px;padding:8px 12px;font-size:13px}
select{flex:1 1 150px;min-width:0}
select option{background:#151b23;color:var(--text)}
select optgroup{background:#151b23;color:#9fb0c3;font-style:normal}
input.t{flex:1 1 150px;min-width:0;min-height:42px}
textarea.t{width:100%;min-height:260px;font-family:ui-monospace,Menlo,Consolas,monospace;font-size:12px;line-height:1.4;resize:vertical;white-space:pre}
input.t.mono{font-family:ui-monospace,Menlo,Consolas,monospace;font-size:13px}
:focus-visible{outline:2px solid var(--accent);outline-offset:2px}

.kv{display:grid;gap:10px}
.line{display:flex;justify-content:space-between;align-items:center;gap:12px;font-size:14px}
.line .k{color:var(--muted)}
.switch{width:52px;height:30px;border-radius:999px;border:1px solid rgba(255,255,255,.14);background:rgba(255,255,255,.06);position:relative;flex:0 0 auto;cursor:pointer;padding:0}
.switch i{width:24px;height:24px;border-radius:50%;background:rgba(255,255,255,.25);position:absolute;top:2px;left:3px;transition:left .2s,background .2s}
.switch.on{background:rgba(53,208,127,.2);border-color:rgba(53,208,127,.55)}
.switch.on i{left:25px;background:rgba(53,208,127,.75)}
.note{font-size:12px;color:var(--muted);line-height:1.45;margin:10px 0 0}
.fb-msg{font-size:12px;color:var(--muted);min-height:16px;margin-top:10px}
.fb-msg.err{color:var(--bad)}

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
.peers{display:grid;gap:8px}
.peer{display:flex;justify-content:space-between;gap:10px;font-size:13px;padding:8px 10px;border:1px solid var(--border);border-radius:12px;background:rgba(0,0,0,.18)}
.peer .on{color:var(--warn)} .peer .stale{color:var(--bad)}
footer{color:var(--muted);font-size:12px;padding:2px 6px 12px;display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap}
a{color:var(--accent);text-decoration:none}
[hidden]{display:none!important}
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
      <section class="card" id="looks" hidden>
        <div class="head"><div class="title" id="looksT">Looks</div><div class="hint">fur pattern &amp; eyes</div></div>
        <div class="body">
          <div class="row">
            <input class="t" id="lname" placeholder="Name (optional)" maxlength="19" autocomplete="off">
          </div>
          <div class="row">
            <select id="lpreset" aria-label="Fur pattern"></select>
            <select id="leyes" aria-label="Eye colour"></select>
          </div>
          <div class="row"><button class="b go" id="lsave">Save looks</button></div>
          <div class="fb-msg" id="lmsg" aria-live="polite"></div>
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
          <div class="fb-msg" id="fb" aria-live="polite"></div>
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
            <div class="line"><span class="k" id="sname">Presence sensor</span><span id="sensor">&#8230;</span></div>
          </div>
          <p class="note">When on, this unit wakes the furbies when someone walks up and puts them to sleep again a while after the last movement. Turn it off to stay in manual control.</p>
        </div>
      </section>

      <section class="card" id="peerscard" hidden>
        <div class="head"><div class="title">Other hubs</div><div class="hint" id="peerhint"></div></div>
        <div class="body"><div class="peers" id="peers"></div></div>
      </section>
    </div>
  </div>

  <section class="card">
    <details>
      <summary>Which furbies are connected</summary>
      <div class="body"><div class="conn" id="conn"></div>
        <p class="note">Unticked furbies are skipped by &ldquo;Wake everyone&rdquo; and the automatic mode.</p></div>
    </details>
    <details id="cfgd">
      <summary>Configuration (furbyhub.json)</summary>
      <div class="body">
        <textarea class="t" id="cfg" spellcheck="false" autocapitalize="off" autocomplete="off" aria-label="furbyhub.json"></textarea>
        <div class="row">
          <button class="b quiet" id="cfgload">Reload</button>
          <button class="b quiet go" id="cfgsave">Save to hub</button>
          <button class="b quiet" id="cfgreboot" hidden>Reboot now</button>
        </div>
        <div class="fb-msg" id="cfgmsg" aria-live="polite"></div>
        <p class="note">Passwords show as ******** and stay unchanged unless you type a new one. Network changes need a reboot. If a wrong network setting locks you out, hold the button while powering up for 3 seconds to get the hub's own WiFi back.</p>
      </div>
    </details>
    <details>
      <summary>Console</summary>
      <div class="body">
        <div class="row">
          <input class="t mono" id="cmd" placeholder="status, help, net, sensor watch" autocapitalize="off" autocomplete="off" spellcheck="false">
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

  // ── fur presets: [body, belly, ears, faceplate, pattern, pattern colour, mane, hat] ──
  // Series 1-4 sampled from a photo of real furbies, 5-10 from a small chart, Santa from a photo; patterned fur
  // corrected by eye. Approximations, not official colour values.
  const PRESETS = {
    wolf: ['#cac0b1', '#9aa49a', '#e3adb7', '#8d7e66', 'none', '#5a3f25', '#b4b9af'],
    leopard: ['#8b877d', '#d2b7ae', '#dba9b4', '#96978f', 'spots', '#3b382e', '#c7c5c6'],
    witchs_cat: ['#181614', '#100d0d', '#f1c7aa', '#0d0a09', 'none', '#060605', '#989293'],
    skunk: ['#17171b', '#f2f2f4', '#e2adb8', '#2a2a2e', 'none', '#070506', '#f2f2f4'],
    church_mouse: ['#857b7b', '#dddad7', '#b79bc6', '#99827f', 'none', '#504c46', '#e0e3ed'],
    snowball: ['#ecf2ee', '#c6c9c3', '#d59fa2', '#cad1ce', 'none', '#2c3127', '#e9e9e8'],
    tiger: ['#b9742c', '#f1efe9', '#b4a7c6', '#55442a', 'stripes', '#251a10', '#1a1a1a'],
    giraffe: ['#7e6440', '#9e9262', '#aea8b8', '#5e3a1e', 'spots', '#472809', '#59554c'],
    owl: ['#7c7975', '#7b7772', '#e8b9c7', '#8c8478', 'none', '#2a2118', '#b7b7b2'],
    dalmatian: ['#f2f2f0', '#e0dfd3', '#9b9ca5', '#dedccf', 'spots', '#1b1b1f', '#eaeae4'],
    bear: ['#82726a', '#ece6dc', '#aaa2a0', '#8d7869', 'none', '#5c4e42', '#4b3b33'],
    zebra: ['#ebe7da', '#f3f0e6', '#d6d0c0', '#78807a', 'stripes', '#25252b', '#efece2'],
    snow_leopard: ['#e8e4dc', '#f2f0ea', '#d9c6c2', '#a7a49e', 'spots', '#5f4a3d', '#f1efea'],
    lamb: ['#f4f2ee', '#c9ac93', '#f0c8b0', '#c2b4a6', 'none', '#431a0f', '#d7d4d1'],
    gorilla: ['#3e3334', '#a76148', '#6b4637', '#281f1f', 'none', '#1b120f', '#837e7e'],
    frog: ['#879a65', '#e8e3b8', '#c1be8d', '#727b47', 'none', '#494e1d', '#4fb83c'],
    elephant: ['#7e8893', '#cccbbe', '#bda7c4', '#576474', 'none', '#343b40', '#ada2a2'],
    pink_flamingo: ['#ebc4cc', '#f64170', '#ec4a8a', '#f3a0b1', 'none', '#4e2835', '#ee4f8c'],
    lady_bug: ['#bb2b2d', '#dc2428', '#cfc8e0', '#afabb5', 'spots', '#160103', '#17171a'],
    dragon: ['#485453', '#f0e77c', '#cfc472', '#193338', 'none', '#0e2228', '#f2f0ec'],
    tie_dye: ['#e9ddd0', '#f3edd6', '#d7ccbd', '#b98876', 'patches', '#b59ad8', '#f2efe9'],
    coffee: ['#b4a185', '#b59f80', '#c8b49c', '#804b2f', 'none', '#3d2412', '#f3f1ee'],
    bumble_bee: ['#eed31e', '#171717', '#e9d12a', '#222222', 'stripes', '#161616', '#161616'],
    cheetah: ['#e8d48c', '#f6f3ea', '#e0c677', '#c0b36c', 'spots', '#4a3b26', '#efe8d4'],
    rooster: ['#c9391e', '#f39a1c', '#d9904a', '#a02c19', 'none', '#701408', '#f0541a'],
    turtle: ['#2f4f33', '#e6d6b4', '#b9bda5', '#645e50', 'patches', '#16261a', '#2f8a3c'],
    kiwi: ['#d8dc3a', '#e8e05a', '#c9c9a0', '#aea24a', 'none', '#6c5218', '#1b2a6b'],
    sherbert: ['#e6286f', '#58b6ee', '#7ea6e6', '#8a6c86', 'none', '#741a5d', '#f4e04a'],
    koala: ['#a9b1c4', '#16161a', '#c8d0e0', '#8e95a8', 'none', '#0d0818', '#f2f2f4'],
    lizard: ['#e8a63a', '#d9902c', '#f0bd5a', '#d49a3a', 'none', '#8c5b16', '#2a2118'],
    alligator: ['#a7cfa0', '#dfeaea', '#b7cde0', '#acc08f', 'patches', '#5a8f5a', '#2f9a45'],
    blue_turtle: ['#1d2b4a', '#0f0f14', '#3b6fb5', '#1b3658', 'none', '#050705', '#1d3f7a'],
    color_change: ['#7a4a24', '#f08a1a', '#b57b4a', '#6f4c2b', 'patches', '#371500', '#1a1410'],
    champagne: ['#dfe3ea', '#f4f4f6', '#c9cfd8', '#cfd6df', 'none', '#9ca3b0', '#f3f3f5'],
    tart_tangerine: ['#1a1411', '#f2541a', '#e9531c', '#741f14', 'none', '#160000', '#f06a1a'],
    husky: ['#1d1d22', '#16161a', '#9aa0ae', '#4c516a', 'none', '#030200', '#1b1b1f'],
    red_wolf: ['#8c1c1c', '#1a0f0d', '#b84a3a', '#a63834', 'none', '#440704', '#8c1c1c'],
    gold_stripes: ['#b9792f', '#f3ead8', '#b7a3c0', '#c38a6e', 'stripes', '#2a1d12', '#d9a24a'],
    banana_peel: ['#f3d92a', '#f7e24a', '#f0c93a', '#c39830', 'none', '#1c1a1a', '#f8e24f'],
    racoon: ['#d9d7d5', '#1a1a1c', '#f0a58a', '#ddd6ce', 'stripes', '#2a2a2e', '#e8e6e2'],
    juicy_grape: ['#4a1330', '#d12a6e', '#2a0f20', '#3c0f0f', 'none', '#140000', '#d12a6e'],
    mink: ['#4a4540', '#2a2724', '#8a8a90', '#6e6f76', 'none', '#0d0a03', '#17171a'],
    puppy_dog: ['#9c6b3a', '#d7b58a', '#a79ab0', '#9c7a7d', 'none', '#251404', '#8a5a2e'],
    green_bean: ['#8fa6bf', '#dfe6ee', '#9bb0c9', '#0f5078', 'none', '#172c36', '#2c6fb5'],
    rusty_dots: ['#c0391f', '#f0cf2a', '#c9a24a', '#9f5016', 'spots', '#15110e', '#17120f'],
    blue_blueberry: ['#1f2f7a', '#1b3f8f', '#2b6fc4', '#0b2564', 'none', '#01040e', '#1f84d6'],
    labrador: ['#b8874a', '#d9ba86', '#b08040', '#aa9564', 'none', '#3b2000', '#b8874a'],
    penguin: ['#1e1e22', '#ffffff', '#f2cf4a', '#f2cf4a', 'none', '#000000', '#1e1e22'],
    parrot: ['#f2c93a', '#f4f0e0', '#2b5fd1', '#70202d', 'none', '#0a0c20', '#d6302a'],
    gold_fish: ['#f3a05a', '#f8d9b0', '#f6b878', '#e48b68', 'none', '#4a2820', '#f6c88a'],
    ocean: ['#2a8a96', '#e6efe8', '#2a6f86', '#645f44', 'none', '#042227', '#1f6f7a'],
    eagle: ['#aaa6a0', '#e8e6e2', '#f0d84a', '#8c7f8a', 'none', '#342226', '#f4f2ee'],
    raspberry_sundae: ['#e8e2e6', '#f6f0f2', '#e9a3c0', '#905a78', 'patches', '#d25a8a', '#f1eaf0'],
    tabby_cat: ['#d8d2c4', '#f3efe6', '#2b2724', '#32251c', 'stripes', '#2f2a26', '#1d1a18'],
    blue_jay: ['#3a7fc4', '#e9f0f6', '#3a7fc4', '#0c4a7c', 'none', '#072e5e', '#2a6fb5'],
    ocean_ripples: ['#1f6a6e', '#3a8a90', '#1d5a60', '#226d72', 'patches', '#0f3a40', '#164a52'],
    snowy_owl: ['#e9e0d0', '#f6f3ec', '#d9cfc0', '#b0b0a4', 'none', '#342d1b', '#f1ece2'],
    coyote: ['#7a5a40', '#ece6dc', '#7a5a40', '#442c24', 'none', '#270d05', '#e8e4dc'],
    red_snapper: ['#a8322a', '#f2ece6', '#e9d0c0', '#5e7465', 'none', '#340d00', '#f2eee8'],
    lion: ['#c9693f', '#d98a55', '#d7a07c', '#c9824a', 'none', '#764341', '#8a3b20'],
    santa: ['#d9262c', '#eeeae4', '#d7d3d4', '#e7c9a0', 'none', '#000000', '#d9262c', 'santa']
  };
  const SERIES = [['Series 1', ['wolf', 'leopard', 'witchs_cat', 'skunk', 'church_mouse', 'snowball']], ['Series 2', ['tiger', 'giraffe', 'owl', 'dalmatian', 'bear', 'zebra']], ['Series 3', ['snow_leopard', 'lamb', 'gorilla', 'frog', 'elephant', 'pink_flamingo']], ['Series 4', ['lady_bug', 'dragon', 'tie_dye', 'coffee', 'bumble_bee', 'cheetah']], ['Series 5', ['rooster', 'turtle', 'kiwi', 'sherbert', 'koala', 'lizard']], ['Series 6', ['alligator', 'blue_turtle', 'color_change', 'champagne', 'tart_tangerine', 'husky']], ['Series 7', ['red_wolf', 'gold_stripes', 'banana_peel', 'racoon', 'juicy_grape', 'mink']], ['Series 8', ['puppy_dog', 'green_bean', 'rusty_dots', 'blue_blueberry', 'labrador', 'penguin']], ['Series 9', ['parrot', 'gold_fish', 'ocean', 'eagle', 'raspberry_sundae', 'tabby_cat']], ['Series 10', ['blue_jay', 'ocean_ripples', 'snowy_owl', 'coyote', 'red_snapper', 'lion']], ['Specials', ['santa']]];
  const NAME = {witchs_cat: "Witch\u2019s Cat", racoon: "Racoon", sherbert: "Sherbert", blue_blueberry: "Blue Blueberry", santa: "Santa (Christmas)", tie_dye: "Tie Dye"};
  const NEUTRAL = ['#b0a090','#e6d9c8','#d8b8a8','#cdbfae','none','#000000',''];
  const EYECOL = { blue:'#3b8fe0', green:'#3fa655', brown:'#8a5a2b', grey:'#9aa3ad', silver:'#cfd6de', purple:'#8e55c8' };
  const pretty = k => NAME[k] || k.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());

  const FACE = '<svg class="fb" viewBox="0 0 64 56" aria-hidden="true"><path class="mn" d="M22 15Q22 4 27 9Q29 0 33 8Q36 0 39 9Q43 4 42 15Z" fill="var(--mane)"/><path d="M8 26L4 4Q16 6 24 14Z" fill="var(--body)"/><path d="M10 22L8 9Q15 11 20 16Z" fill="var(--ear)"/><path d="M56 26L60 4Q48 6 40 14Z" fill="var(--body)"/><path d="M54 22L56 9Q49 11 44 16Z" fill="var(--ear)"/><ellipse cx="32" cy="32" rx="26" ry="22" fill="var(--body)"/><ellipse cx="32" cy="50" rx="14" ry="6" fill="var(--belly)"/><g class="p p-spots" fill="var(--patc)"><circle cx="13" cy="33" r="2.6"/><circle cx="18" cy="43" r="2.1"/><circle cx="51" cy="33" r="2.6"/><circle cx="46" cy="43" r="2.1"/><circle cx="10" cy="24" r="1.8"/><circle cx="54" cy="24" r="1.8"/><circle cx="32" cy="14" r="2"/></g><g class="p p-stripes" stroke="var(--patc)" stroke-width="2.6" stroke-linecap="round" fill="none"><path d="M8 32q4 2 7-1"/><path d="M10 40q4 2 7-1"/><path d="M56 32q-4 2-7-1"/><path d="M54 40q-4 2-7-1"/><path d="M26 11q6-2 12 0"/></g><g class="p p-patches" fill="var(--patc)" opacity=".75"><ellipse cx="14" cy="38" rx="6" ry="5"/><ellipse cx="50" cy="26" rx="5" ry="6"/><ellipse cx="42" cy="46" rx="6" ry="4"/></g><rect x="12" y="20" width="40" height="20" rx="10" fill="var(--plate)"/><g><circle cx="23" cy="29" r="7" fill="#fff"/><circle cx="23" cy="29" r="4.3" fill="var(--iris)"/><circle cx="23" cy="29" r="1.9" fill="#111"/><circle class="lid" cx="23" cy="29" r="7.4" fill="var(--plate)"/><path class="lash" d="M16.5 30q6.5 5 13 0" stroke="#222" stroke-width="1.6" fill="none" stroke-linecap="round"/></g><g><circle cx="41" cy="29" r="7" fill="#fff"/><circle cx="41" cy="29" r="4.3" fill="var(--iris)"/><circle cx="41" cy="29" r="1.9" fill="#111"/><circle class="lid" cx="41" cy="29" r="7.4" fill="var(--plate)"/><path class="lash" d="M34.5 30q6.5 5 13 0" stroke="#222" stroke-width="1.6" fill="none" stroke-linecap="round"/></g><path d="M26 35h12l-6 5z" fill="#f2b830"/><path d="M27.5 39h9l-4.5 4.5z" fill="#e39a1c"/><g class="h h-santa"><path d="M49 17C47 4 35 -3 23 -1C18 0 16 4 17 8C18 12 13 14 14 18Z" fill="#d6302a"/><rect x="12" y="14" width="40" height="7" rx="3.5" fill="#fff"/><circle cx="18" cy="-1" r="4.2" fill="#fff"/></g></svg>';

  function setFace(svg, preset, eyes, state) {
    const P = PRESETS[preset] || NEUTRAL, s = svg.style;
    s.setProperty('--body', P[0]); s.setProperty('--belly', P[1]); s.setProperty('--ear', P[2]);
    s.setProperty('--plate', P[3]); s.setProperty('--patc', P[5] || '#000'); s.setProperty('--mane', P[6] || P[0]);
    s.setProperty('--iris', EYECOL[eyes] || EYECOL.blue);
    s.setProperty('--lid', state === 1 ? 1 : state === 2 ? 0 : .5);
    s.setProperty('--lash', state === 1 ? 1 : 0);
    svg.dataset.pat = P[4] || 'none'; svg.dataset.mane = P[6] ? '1' : '0'; svg.dataset.hat = P[7] || '';
  }

  const MSG = [[0,'a','Say my name'],[1,'b',''],[2,'c',''],[3,'d',''],[4,'e',''],[5,'f','Dance'],[6,'g',''],[7,'h',''],
               [8,'i','Sneeze'],[9,'j',''],[10,'k',''],[11,'l','Ba ba black sheep'],[12,'m',''],[13,'n',''],[14,'o','Go to bed'],[15,'p','Sleep']];
  const WORD = {0:'unknown',1:'asleep',2:'awake'};
  const sel = new Set();
  let st = null, cfg = null, tiles = [], failures = 0, fbTimer = 0, looksFor = 0;

  // ── build tiles, selects ──
  const tilesEl = $('#tiles'), connEl = $('#conn');
  for (let i = 1; i <= 16; i++) {
    const b = document.createElement('button');
    b.className = 'tile'; b.type = 'button'; b.setAttribute('aria-pressed', 'false');
    b.innerHTML = FACE + '<span class="n">' + i + '</span><span class="s"></span>';
    b.addEventListener('click', () => { if (b.classList.contains('off')) return; sel.has(i) ? sel.delete(i) : sel.add(i); paint(); });
    tilesEl.appendChild(b); tiles.push(b);
    const l = document.createElement('label');
    l.innerHTML = '<input type="checkbox" data-n="' + i + '">' + i;
    connEl.appendChild(l);
  }
  connEl.addEventListener('change', e => { const n = e.target.dataset.n; if (n) { sel.delete(+n); run((e.target.checked ? 'enable ' : 'disable ') + n, 1); } });
  MSG.forEach(m => { const o = document.createElement('option'); o.value = m[0]; o.textContent = m[1] + (m[2] ? '  ' + m[2] : '  (code ' + m[0] + ')'); $('#msg').appendChild(o); });
  $('#msg').value = 0;
  $('#lpreset').innerHTML = '<option value="">Plain (no preset)</option>' + SERIES.map(g => '<optgroup label="' + g[0] + '">' + g[1].map(k => '<option value="' + k + '">' + pretty(k) + '</option>').join('') + '</optgroup>').join('');
  $('#leyes').innerHTML = Object.keys(EYECOL).map(k => '<option value="' + k + '">' + pretty(k) + ' eyes</option>').join('');

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
  const look = n => (cfg && cfg.furbies && cfg.furbies[n]) || { preset: '', eyes: 'blue', name: '' };

  async function loadCfg(fillEditor) {
    try {
      const r = await fetch('/api/config', { cache: 'no-store' });
      const text = await r.text();
      cfg = JSON.parse(text);
      if (fillEditor) $('#cfg').value = text;
      looksFor = 0; render();
    } catch (e) { /* keep the old copy */ }
  }

  function paint() {
    const n = sel.size;
    $('#count').textContent = n ? n + ' selected' : 'none selected';
    ['wake', 'sleep', 'send'].forEach(id => $('#' + id).disabled = !n);
    tiles.forEach((t, i) => { const on = sel.has(i + 1); t.classList.toggle('sel', on); t.setAttribute('aria-pressed', on); });
    // looks editor for exactly one selected furby
    const one = n === 1 ? [...sel][0] : 0;
    $('#looks').hidden = !one;
    if (one && one !== looksFor && cfg) {
      looksFor = one; const L = look(one);
      $('#looksT').textContent = 'Looks of furby ' + one;
      $('#lname').value = L.name || ''; $('#lpreset').value = L.preset || ''; $('#leyes').value = L.eyes || 'blue'; $('#lmsg').textContent = '';
    }
    if (!one) looksFor = 0;
  }

  function render() {
    if (!st) return;
    $('#unit').textContent = (cfg && cfg.hub && cfg.hub.name ? cfg.hub.name + '  \u2013  ' : '') + 'Unit ' + st.unit + '  \u2013  ' + st.ssid;
    const p = $('#pill');
    p.textContent = st.net === 'sta' ? 'on the furby network' : st.stations + (st.stations === 1 ? ' device' : ' devices') + ' connected';
    p.classList.add('ok');
    st.f.forEach((f, i) => {
      const t = tiles[i], en = (st.mask >> i) & 1, L = look(i + 1);
      t.classList.toggle('off', !en);
      t.classList.toggle('awake', en && f[0] === 2);
      t.classList.toggle('busy', !!f[2]);
      setFace(t.querySelector('svg'), L.preset, L.eyes, f[0]);
      t.querySelector('.s').textContent = en ? (L.name || WORD[f[0]]) : 'unused';
      if (!en) sel.delete(i + 1);
      connEl.children[i].firstChild.checked = !!en;
    });
    const sw = $('#auto'); sw.classList.toggle('on', !!st.auto); sw.setAttribute('aria-checked', !!st.auto);
    const stype = st.stype || 'pir';
    $('#sname').textContent = stype === 'hcsr04' ? 'Distance sensor' : stype === 'none' ? 'Sensor' : 'Presence sensor';
    let sv = stype === 'none' ? 'not used' : st.sensor ? 'someone is there' : 'nobody';
    if (stype === 'hcsr04' && st.dist >= 0) sv += '  (' + st.dist + ' mm)';
    $('#sensor').textContent = sv;
    $('#sensor').style.color = st.sensor ? 'var(--warn)' : '';
    let h = st.state;
    if (st.state === 'awake' && st.auto && st.remaining) h = 'awake, sleeps in ' + st.remaining + ' s';
    else if (st.state === 'idle') h = 'quiet';
    $('#home').textContent = h;
    // other hubs
    const peers = st.peers || [];
    $('#peerscard').hidden = !peers.length;
    if (peers.length) {
      $('#peerhint').textContent = peers.length + (peers.length === 1 ? ' hub' : ' hubs') + ' on the network';
      $('#peers').innerHTML = peers.map(q => {
        const stale = q[3] > 15;
        return '<div class="peer"><span>Hub ' + q[0] + '</span><span class="' + (stale ? 'stale' : q[1] ? 'on' : '') + '">' +
               (stale ? 'no signal for ' + q[3] + ' s' : (q[1] ? 'someone there' : 'quiet') + (q[2] >= 0 ? '  ' + q[2] + ' mm' : '')) + '</span></div>';
      }).join('');
    }
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

  // ── actions ──
  $('#wake').onclick  = () => run('wake ' + list());
  $('#sleep').onclick = () => run('sleep ' + list());
  $('#send').onclick  = () => run('send ' + list() + ' ' + $('#msg').value);
  $('#wakeall').onclick  = () => run('wakeall');
  $('#sleepall').onclick = () => run('sleepall');
  $('#auto').onclick  = () => run('auto ' + (st && st.auto ? 'off' : 'on'));
  $('#all').onclick   = () => { st.f.forEach((f, i) => { if ((st.mask >> i) & 1) sel.add(i + 1); }); paint(); };
  $('#none').onclick  = () => { sel.clear(); paint(); };

  // looks: one partial JSON POST, saved to flash by the hub
  async function postCfg(obj) {
    const r = await fetch('/api/config', { method: 'POST', body: typeof obj === 'string' ? obj : JSON.stringify(obj) });
    return r.json();
  }
  $('#lsave').onclick = async () => {
    const n = looksFor; if (!n) return;
    const m = $('#lmsg'); m.className = 'fb-msg'; m.textContent = 'Saving\u2026';
    try {
      const o = { furbies: {} };
      o.furbies[n] = { name: $('#lname').value.trim(), preset: $('#lpreset').value, eyes: $('#leyes').value };
      const j = await postCfg(o);
      if (j.ok) { m.textContent = j.saved ? 'Saved.' : 'Applied, but could not write the file.'; await loadCfg(true); }
      else { m.className = 'fb-msg err'; m.textContent = j.error; }
    } catch (e) { m.className = 'fb-msg err'; m.textContent = 'No connection to the unit'; }
  };

  // configuration editor
  $('#cfgd').addEventListener('toggle', () => { if ($('#cfgd').open) loadCfg(true); });
  $('#cfgload').onclick = () => { $('#cfgmsg').textContent = ''; loadCfg(true); };
  $('#cfgsave').onclick = async () => {
    const m = $('#cfgmsg'); m.className = 'fb-msg'; m.textContent = 'Saving\u2026'; $('#cfgreboot').hidden = true;
    try {
      const j = await postCfg($('#cfg').value);
      if (j.ok) {
        m.textContent = (j.saved ? 'Saved to flash.' : 'Applied, but could not write the file.') + (j.reboot ? ' This change needs a reboot.' : '');
        $('#cfgreboot').hidden = !j.reboot; await loadCfg(true);
      } else { m.className = 'fb-msg err'; m.textContent = j.error; }
    } catch (e) { m.className = 'fb-msg err'; m.textContent = 'Not valid JSON, or no connection to the unit'; }
  };
  $('#cfgreboot').onclick = () => { api('reboot'); $('#cfgmsg').textContent = 'Rebooting\u2026 reconnect in about 10 seconds.'; };

  // console
  const go = async () => { const c = $('#cmd').value.trim(); if (!c) return; const r = await run(c, 1); $('#out').textContent = '> ' + c + '\n' + (r === undefined ? 'no connection to the unit' : r); };
  $('#run').onclick = go;
  $('#cmd').addEventListener('keydown', e => { if (e.key === 'Enter') go(); });

  loadCfg(false); poll(); setInterval(poll, 1000);
})();
</script>
</body>
</html>
)HTML";