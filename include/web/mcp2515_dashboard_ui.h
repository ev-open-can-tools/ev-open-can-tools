#pragma once
#include <Arduino.h>

static const char DASH_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en" data-theme="dark">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>ev-open-can-tools</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
[data-theme="dark"]{
  --bg:#0d0d0d;--bg2:var(--bg);--card:#161616;--card2:#1e1e1e;
  --bd:#2a2a2a;--bd2:#333;
  --tx:#f0f0f0;--tx2:#999;--tx3:#555;
  --acc:#5b8fff;--accBg:rgba(91,143,255,.1);--accBd:rgba(91,143,255,.25);
  --ok:#3dba72;--okBg:rgba(61,186,114,.1);
  --err:#ff4f4f;--errBg:rgba(255,79,79,.08);--errBd:rgba(255,79,79,.2);
  --warn:#f5a623;
}
[data-theme="light"]{
  --bg:#f5f5f5;--bg2:var(--bg);--card:#fff;--card2:#f0f0f0;
  --bd:#e0e0e0;--bd2:#ccc;
  --tx:#111;--tx2:#555;--tx3:#999;
  --acc:#2563eb;--accBg:rgba(37,99,235,.08);--accBd:rgba(37,99,235,.2);
  --ok:#16a34a;--okBg:rgba(22,163,74,.08);
  --err:#dc2626;--errBg:rgba(220,38,38,.06);--errBd:rgba(220,38,38,.18);
  --warn:#d97706;
}
html{scroll-behavior:smooth}
body{background:var(--bg);color:var(--tx);font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
  min-height:100vh;max-width:480px;margin:0 auto;font-size:14px;line-height:1.5;
  transition:background .2s,color .2s}

/* Header */
.hdr{padding:20px 16px 0;display:flex;flex-direction:column;gap:4px}
.hdr-top{display:flex;align-items:center;justify-content:space-between}
.hdr-left{display:flex;align-items:center;gap:10px}
.hdr-title{font-size:20px;font-weight:700;color:var(--tx)}
.hw-badge{padding:3px 8px;border-radius:5px;font-size:11px;font-weight:600;
  background:var(--accBg);border:1px solid var(--accBd);color:var(--acc)}
.theme-btn{padding:6px 10px;border:1px solid var(--bd2);border-radius:8px;
  background:var(--card);color:var(--tx2);font-size:12px;cursor:pointer;
  display:flex;align-items:center;gap:4px;transition:all .2s}
.theme-btn:hover{border-color:var(--acc);color:var(--acc)}
.hdr-status{display:flex;align-items:center;gap:6px;font-size:12px;color:var(--tx2)}
.sdot{width:7px;height:7px;border-radius:50%;flex-shrink:0;transition:all .4s}
.dot-on{background:var(--ok);box-shadow:0 0 8px var(--ok)}
.dot-off{background:var(--err)}
.dot-warn{background:var(--warn)}

/* FPS bar */
.fps-bar{margin:14px 16px 0;height:3px;background:var(--bd);border-radius:2px;overflow:hidden}
.fps-fill{height:100%;background:var(--acc);border-radius:2px;transition:width .5s;width:0%}

/* Status grid */
.stat-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px;margin:14px 16px 0}
.stat{background:var(--card);border:1px solid var(--bd);border-radius:10px;padding:10px 12px}
.stat-lbl{font-size:10px;color:var(--tx3);text-transform:uppercase;letter-spacing:.8px;margin-bottom:3px}
.stat-val{font-size:14px;font-weight:600;color:var(--tx)}
.v-ok{color:var(--ok)}.v-err{color:var(--err)}.v-acc{color:var(--acc)}.v-dim{color:var(--tx3)}.v-warn{color:var(--warn)}
.stat-wide{grid-column:span 3}

/* Divider */
hr{border:none;border-top:1px solid var(--bd);margin:16px}

/* Cards */
.card{background:var(--card);border:1px solid var(--bd);border-radius:12px;padding:16px;margin:0 16px 12px;overflow:hidden}
.card-hdr{display:grid;grid-template-columns:minmax(0,1fr) auto auto;align-items:center;column-gap:8px;margin-bottom:14px}
.card-title{font-size:13px;font-weight:600;color:var(--tx);text-transform:uppercase;letter-spacing:.5px;min-width:0}
.card-meta{font-size:11px;color:var(--tx3);justify-self:end;text-align:right;min-width:0}
.card-min-btn{padding:4px 8px;font-size:10px;justify-self:end}
.card.collapsed{padding-bottom:12px}
.card.collapsed .card-hdr{margin-bottom:0}
.card.collapsed>:not(.card-hdr){display:none !important}
.subsec{margin-top:14px;padding-top:12px;border-top:1px solid var(--bd)}
.subsec:first-child{margin-top:0;padding-top:0;border-top:none}
.subsec-head{display:grid;grid-template-columns:minmax(0,1fr) auto auto;align-items:center;column-gap:8px;margin-bottom:8px}
.subsec-title{font-size:13px;font-weight:600;color:var(--tx);min-width:0}
.title-help{display:inline-flex;align-items:center;justify-content:center;width:16px;height:16px;margin-left:6px;border:1px solid var(--bd2);border-radius:50%;font-size:10px;font-weight:700;color:var(--tx3);cursor:pointer;vertical-align:middle;line-height:1;background:transparent}
.title-help:hover{border-color:var(--accBd);color:var(--acc);background:var(--accBg)}
.info-box{margin-bottom:10px;padding:10px 12px;background:var(--bg2);border:1px solid var(--bd);border-radius:9px;font-size:12px;color:var(--tx3);line-height:1.6}
.info-box a{color:var(--acc);text-decoration:none}
.inline-help-panel{display:none;margin:8px 0 0;padding:10px 12px;background:var(--bg2);border:1px solid var(--bd);border-radius:9px;font-size:12px;color:var(--tx3);line-height:1.6}
.inline-help-panel.show{display:block}
.subsec-meta{font-size:11px;color:var(--tx3);justify-self:end;text-align:right;min-width:0}
.subsec-btn{padding:4px 8px;font-size:10px;justify-self:end}
.subsec.collapsed .subsec-head{margin-bottom:0}
.subsec.collapsed .subsec-body{display:none}

/* HW seg */
.hw-seg{display:flex;background:var(--card2);border:1px solid var(--bd);border-radius:9px;padding:3px;gap:2px}
.hw-btn{flex:1;padding:8px;border:none;border-radius:7px;font-size:12px;font-weight:600;
  cursor:pointer;background:transparent;color:var(--tx2);transition:all .18s;font-family:inherit}
.hw-btn.active{background:var(--card);color:var(--acc);border:1px solid var(--accBd);
  box-shadow:0 1px 4px rgba(0,0,0,.15)}
.hw-btn:hover:not(.active){background:var(--bd);color:var(--tx)}
.profile-wrap{margin-top:12px}
.profile-label{font-size:11px;color:var(--tx3);margin-bottom:6px}
.profile-group.hidden{display:none}
.profile-note{font-size:10px;color:var(--tx3);margin-top:6px}

/* Speed pills */
.pills{display:flex;gap:6px;flex-wrap:wrap}
/* Settings rows */
.setting-row{display:flex;align-items:center;justify-content:space-between;
  padding:12px 0;border-bottom:1px solid var(--bd)}
.setting-row:last-of-type{border-bottom:none;padding-bottom:0}
.setting-row:first-of-type{padding-top:0}
.setting-info{flex:1;min-width:0}
.setting-name{font-size:13px;font-weight:500;color:var(--tx)}
.setting-desc{font-size:11px;color:var(--tx3);margin-top:2px}
.hw4-only.hidden{display:none}

/* Toggle */
.tgl{position:relative;width:44px;height:24px;flex-shrink:0;margin-left:12px}
.tgl input{opacity:0;width:0;height:0;position:absolute}
.tgl-track{position:absolute;inset:0;background:var(--bd2);border-radius:24px;cursor:pointer;transition:all .22s}
.tgl-thumb{position:absolute;top:3px;left:3px;width:18px;height:18px;background:#fff;
  border-radius:50%;transition:all .22s;box-shadow:0 1px 3px rgba(0,0,0,.3)}
.tgl input:checked~.tgl-track{background:var(--acc)}
.tgl input:checked~.tgl-track .tgl-thumb{transform:translateX(20px)}
.tgl input:disabled~.tgl-track{opacity:.35;cursor:not-allowed}

/* Sniffer */
.sniff-ctrl{display:flex;gap:6px;margin-bottom:8px}
.sniff-input{flex:1;background:var(--bg);border:1px solid var(--bd);border-radius:8px;
  padding:7px 10px;color:var(--tx);font-size:12px;font-family:inherit;transition:border .2s}
.sniff-input{width:100%;min-width:0;box-sizing:border-box;} 
.sniff-input:focus{outline:none;border-color:var(--acc)}
.sniff-input::placeholder{color:var(--tx3)}
.sniff-btn{padding:7px 12px;background:transparent;border:1px solid var(--bd);border-radius:8px;
  color:var(--tx2);font-size:11px;font-weight:600;cursor:pointer;transition:all .18s;font-family:inherit}
.sniff-btn.paused{border-color:var(--warn);color:var(--warn)}
.sniff-btn:hover:not(.paused){border-color:var(--bd2);color:var(--tx)}
.sniff-box{background:var(--bg);border:1px solid var(--bd);border-radius:9px;
  max-height:250px;overflow-y:auto;font-family:'SF Mono','Courier New',monospace}
.sniff-box::-webkit-scrollbar{width:4px}
.sniff-box::-webkit-scrollbar-thumb{background:var(--bd2);border-radius:4px}
.sniff-row{display:grid;grid-template-columns:38px 72px 1fr;gap:8px;
  padding:6px 10px;border-bottom:1px solid var(--bd);font-size:11px;align-items:start}
.sniff-row:last-child{border-bottom:none}
.sniff-row.hi{border-left:2px solid var(--acc);padding-left:8px}
.s-ts{color:var(--tx3);font-size:10px;padding-top:1px}
.s-id{color:var(--acc);font-weight:700}
.s-data{color:var(--tx2);word-break:break-all}
.s-name{color:var(--ok);font-size:10px;margin-top:2px}

/* EFLG */
.eflg-row{display:flex;flex-wrap:wrap;gap:5px;margin-top:10px}
.eflg-pill{padding:3px 8px;border-radius:5px;font-size:10px;font-weight:600;letter-spacing:.3px}
.eflg-ok{background:var(--okBg);color:var(--ok)}
.eflg-warn{background:rgba(245,166,35,.1);color:var(--warn)}
.eflg-err{background:var(--errBg);color:var(--err)}

/* Mux table */
.mux-tbl{width:100%;border-collapse:collapse;font-size:12px;margin-top:10px}
.mux-tbl th{color:var(--tx3);font-size:10px;text-transform:uppercase;letter-spacing:.8px;
  text-align:left;padding:4px 8px;border-bottom:1px solid var(--bd);font-weight:500}
.mux-tbl td{padding:5px 8px;color:var(--tx2);border-bottom:1px solid var(--bd)}
.mux-tbl tr:last-child td{border-bottom:none}
.mux-tbl td:first-child{color:var(--acc);font-weight:600}

/* Last write check */
.probe-status{font-size:13px;font-weight:600}
.probe-note{font-size:11px;color:var(--tx3);line-height:1.6;margin-top:10px}
.probe-block{margin-top:12px;padding-top:12px;border-top:1px solid var(--bd)}
.probe-meta{font-size:11px;color:var(--tx3);margin-bottom:4px}
.probe-label{font-size:10px;color:var(--tx3);text-transform:uppercase;letter-spacing:.8px;margin-bottom:6px}
.probe-hex{font-family:'SF Mono','Courier New',monospace;font-size:12px;color:var(--tx2);word-break:break-all}

/* Buttons */
.btn-row{display:flex;gap:8px;margin-top:14px}
.btn{flex:1;padding:10px;border:1px solid;border-radius:9px;background:transparent;
  font-family:inherit;font-size:12px;font-weight:600;cursor:pointer;transition:all .18s;letter-spacing:.3px}
.btn-stop{border-color:var(--errBd);color:var(--err)}
.btn-stop:hover{background:var(--errBg)}
.btn-reboot{border-color:var(--bd2);color:var(--tx2)}
.btn-reboot:hover{border-color:var(--acc);color:var(--acc)}

/* Confirm modal */
.modal-backdrop{position:fixed;inset:0;display:none;align-items:center;justify-content:center;
  padding:16px;background:rgba(0,0,0,.55);z-index:9999}
.modal-card{width:min(100%,360px);background:var(--card);border:1px solid var(--bd2);
  border-radius:12px;padding:16px;box-shadow:0 16px 40px rgba(0,0,0,.35)}
.modal-title{font-size:14px;font-weight:700;color:var(--tx)}
.modal-msg{margin-top:8px;font-size:12px;color:var(--tx2);line-height:1.6;white-space:pre-wrap}
.modal-actions{display:flex;justify-content:flex-end;gap:8px;margin-top:14px}
.modal-btn-primary{background:var(--accBg);border-color:var(--accBd);color:var(--acc)}
.modal-btn-primary:hover{background:var(--acc);color:#fff}

/* OTA upload */
.ota-drop{border:2px dashed var(--bd2);border-radius:10px;padding:24px 16px;
  text-align:center;cursor:pointer;transition:all .2s;background:var(--bg)}
.ota-drop:hover,.ota-drop.drag{border-color:var(--acc);background:var(--accBg)}
.ota-drop input{display:none}
.ota-icon{font-size:24px;margin-bottom:8px}
.ota-text{font-size:13px;font-weight:500;color:var(--tx2);margin-bottom:3px}
.ota-sub{font-size:11px;color:var(--tx3)}
.ota-progress{margin-top:12px;display:none}
.ota-bar{height:4px;background:var(--bd);border-radius:2px;overflow:hidden;margin-bottom:6px}
.ota-fill{height:100%;background:var(--acc);border-radius:2px;transition:width .3s;width:0%}
.ota-status{font-size:11px;color:var(--acc);text-align:center}
.ota-btn{width:100%;margin-top:10px;padding:10px;border:1px solid var(--accBd);border-radius:9px;
  background:var(--accBg);color:var(--acc);font-family:inherit;font-size:13px;font-weight:600;
  cursor:pointer;transition:all .2s;display:none}
.ota-btn:hover{background:var(--acc);color:#fff}

/* Log */
.log-box{background:var(--bg);border:1px solid var(--bd);border-radius:9px;padding:10px 12px;
  font-family:'SF Mono','Courier New',monospace;font-size:11px;color:var(--tx2);
  max-height:180px;overflow-y:auto;line-height:1.9;white-space:pre-wrap;word-break:break-all}
.log-box::-webkit-scrollbar{width:4px}
.log-box::-webkit-scrollbar-thumb{background:var(--bd2);border-radius:4px}
.lf{color:var(--ok)}.lh{color:var(--acc)}.le{color:var(--err)}.lc{color:var(--warn)}.lo{color:var(--tx2)}

/* Recorder */
.rec-bar{height:4px;background:var(--bd);border-radius:2px;overflow:hidden;margin-bottom:6px}
.rec-fill{height:100%;background:var(--err);border-radius:2px;transition:width .3s;width:0%}
.rec-info{display:flex;justify-content:space-between;font-size:11px;color:var(--tx3);margin-bottom:10px}

/* Warning */
.warn-bar{margin:0 16px 14px;padding:10px 14px;border-radius:9px;
  background:var(--errBg);border:1px solid var(--errBd);font-size:11px;color:var(--err);line-height:1.7}
.foot{text-align:center;padding:8px 16px 20px;font-size:11px;color:var(--tx3)}
</style>
</head>
<body>

<div class="hdr">
  <div class="hdr-top">
    <div class="hdr-left">
      <div class="hdr-title">ev-open-can-tools</div>
      <span class="hw-badge" id="hw-badge">HW3</span>
    </div>
    <button class="theme-btn" onclick="toggleTheme()" id="theme-btn">&#9788; Light</button>
  </div>
  <div class="hdr-status">
    <span class="sdot dot-off" id="dot"></span>
    <span id="hdr-desc">Waiting for CAN frames</span>
  </div>
</div>

<div class="fps-bar"><div class="fps-fill" id="fps-fill"></div></div>

<div class="stat-grid">
  <div class="stat"><div class="stat-lbl">CAN Bus</div><div class="stat-val" id="s-can">Offline</div></div>
  <div class="stat"><div class="stat-lbl">Injection</div><div class="stat-val v-dim" id="s-inj">—</div></div>
  <div class="stat"><div class="stat-lbl">Frame rate</div><div class="stat-val v-dim" id="s-fps">0.0 Hz</div></div>
  <div class="stat"><div class="stat-lbl">RX</div><div class="stat-val v-acc" id="s-rx">0</div></div>
  <div class="stat"><div class="stat-lbl">TX</div><div class="stat-val v-acc" id="s-tx">0</div></div>
  <div class="stat"><div class="stat-lbl">TX Errors</div><div class="stat-val v-dim" id="s-txerr">0</div></div>
  <div class="stat"><div class="stat-lbl">Follow dist</div><div class="stat-val v-dim" id="s-fd">—</div></div>
  <div class="stat"><div class="stat-lbl">Profile</div><div class="stat-val v-dim" id="s-prof">—</div></div>
  <div class="stat"><div class="stat-lbl">Speed Offset</div><div class="stat-val v-dim" id="s-soff">0</div></div>
  <div class="stat"><div class="stat-lbl">Uptime</div><div class="stat-val v-dim" id="s-up">0s</div></div>
  <button class="btn btn-stop" id="btn-stop" style="display:none" onclick="emergencyStop()">Stop Injecting</button>
  <button class="btn" id="btn-resume" style="display:none;background:var(--accBg);color:var(--acc);border:1px solid var(--accBd)" onclick="resumeInj()">Resume Injection</button>
  <button class="btn btn-reboot" onclick="reboot()">Reboot</button>
</div>

<div style="height:12px"></div>

<div class="card">
  <div class="card-hdr">
    <div class="card-title">Plugins <span class="title-help" onclick="return toggleHelp(this,event)" data-help-target="plg-info" title="Install and manage JSON plugins that modify CAN messages in real time.">?</span></div>
    <div class="card-meta" id="plg-count">0 installed</div>
  </div>

  <div id="plg-info" class="info-box" style="display:none;margin-bottom:12px">
    Plugins are JSON rules that modify CAN messages in real time. Install via URL, file upload or paste.
    <div style="margin-top:6px"><a href="https://ev-open-can-tools.github.io/ev-open-can-tools/docs/plugins.html" target="_blank" rel="noopener" style="color:var(--acc);text-decoration:none">Documentation &amp; examples &rarr;</a></div>
  </div>

  <div style="margin-bottom:14px">
    <div class="setting-name" style="margin-bottom:8px">Install Plugin <span class="title-help" onclick="return toggleHelp(this,event)" data-help-target="plg-info" title="Install a plugin from a URL or uploaded JSON file.">?</span></div>
    <div style="font-size:11px;color:var(--tx3);margin-bottom:8px" id="plg-limit">Maximum plugins: --</div>
    <div style="display:flex;gap:6px;margin-bottom:8px">
      <input class="sniff-input" id="plg-url" placeholder="Plugin JSON URL (https://...)" style="flex:1">
      <button class="sniff-btn" onclick="installPlugin()">Install</button>
    </div>
    <div style="display:flex;gap:8px;align-items:center;margin-bottom:8px">
      <input type="file" id="plg-file" accept=".json" onchange="uploadPlugin(this.files[0])" style="display:none">
      <button class="sniff-btn" onclick="$('plg-file').click()">Upload .json</button>
      <span style="font-size:11px;color:var(--tx3)" id="plg-status"></span>
    </div>
    <div class="setting-name" style="margin-bottom:6px">Paste JSON (offline) <span class="title-help" onclick="return toggleHelp(this,event)" title="Paste a full plugin JSON document directly into the editor below.">?</span></div>
    <textarea id="plg-paste" placeholder='{"name":"...","version":"1.0","rules":[...]}' style="width:100%;height:80px;resize:vertical;background:var(--bg2);color:var(--tx);border:1px solid var(--bd);border-radius:6px;padding:8px;font-family:monospace;font-size:11px;box-sizing:border-box;margin-bottom:6px"></textarea>
    <button class="sniff-btn" onclick="pastePlugin()">Install from JSON</button>
  </div>

  <div id="plg-conflicts" style="display:none;margin-bottom:10px"></div>

  <div style="padding-top:12px;border-top:1px solid var(--bd)" id="plg-list">
    <div style="font-size:12px;color:var(--tx3);text-align:center;padding:12px">No plugins installed</div>
  </div>
</div>

<div class="card">
  <div class="card-hdr">
    <div class="card-title">Plugin Editor <span class="title-help" onclick="return toggleHelp(this,event)" data-help-target="pe-info" title="Build, edit and test plugin rules without writing JSON by hand.">?</span></div>
    <div class="card-meta" id="pe-count">0 rules</div>
  </div>
  <div id="pe-info" class="info-box" style="display:none">
    Build or edit a plugin via form &mdash; no JSON writing needed. Load an installed plugin into the editor, change rules, then reinstall it. You can also send a temporary test frame for one rule before installing.
  </div>
  <div style="display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr) 90px;gap:6px;margin-bottom:10px">
    <input class="sniff-input" id="pe-name" placeholder="Plugin name" maxlength="31" oninput="peRenderPreview()">
    <input class="sniff-input" id="pe-author" placeholder="Author (optional)" oninput="peRenderPreview()">
    <input class="sniff-input" id="pe-version" placeholder="Version" value="1.0" oninput="peRenderPreview()">
  </div>
  <div class="subsec" data-subkey="plugin-editor-quick-rule">
    <div class="subsec-head">
      <div class="subsec-title">Quick Rule <span class="title-help" onclick="return toggleHelp(this,event)" title="Create one plugin rule quickly from a shorthand CAN line.">?</span></div>
      <div class="subsec-meta">Fast builder</div>
    </div>
    <div class="subsec-body">
      <div class="info-box">
        <div style="font-size:11px;color:var(--tx3);line-height:1.5;margin-bottom:8px">
          Paste a shorthand line like <span style="font-family:monospace">0x7FF mux=2 byte[5] = 0x4C</span> to create one rule directly.
        </div>
        <div style="display:flex;gap:6px;flex-wrap:wrap">
          <input class="sniff-input" id="pe-shortcut" placeholder="0x7FF mux=2 byte[5] = 0x4C (bit 2 flipped -> tier 3 SELF_DRIVING)" onkeydown="if(event.key==='Enter'){event.preventDefault();peAddRuleFromShortcut()}" style="flex:1">
          <button class="sniff-btn" onclick="peAddRuleFromShortcut()">Add Shortcut</button>
        </div>
      </div>
    </div>
  </div>
  <div id="pe-rules"></div>
  <button class="sniff-btn" onclick="peAddRule()" style="margin-top:6px">+ Add Rule</button>
  <details style="margin-top:10px">
    <summary style="font-size:11px;color:var(--acc);cursor:pointer;user-select:none">JSON Preview</summary>
    <pre id="pe-preview" style="max-height:200px;overflow:auto;background:var(--bg2);border:1px solid var(--bd);border-radius:6px;padding:8px;font-size:11px;color:var(--tx2);margin-top:6px;white-space:pre-wrap;word-break:break-all"></pre>
  </details>
  <div class="subsec" data-subkey="plugin-editor-rule-test">
    <div class="subsec-head">
      <div class="subsec-title">Rule Test <span class="title-help" onclick="return toggleHelp(this,event)" title="Waits for a live matching frame, applies one editor rule and injects the result for testing.">?</span></div>
      <div class="subsec-meta">Single rule preview</div>
    </div>
    <div class="subsec-body">
      <div style="font-size:11px;color:var(--tx3);line-height:1.5;margin-bottom:8px">
        Choose one rule from the editor. The dashboard waits for the next matching CAN frame, applies the rule to that live frame, then injects it with your count and interval.
      </div>
      <div style="display:grid;grid-template-columns:minmax(0,1fr) 90px 110px;gap:6px;margin-bottom:6px">
        <select class="sniff-input" id="pe-test-rule" onchange="peUpdateTestPreview()"></select>
        <input class="sniff-input" id="pe-test-count" type="number" min="1" max="200" value="1" onchange="peUpdateTestPreview()">
        <input class="sniff-input" id="pe-test-interval" type="number" min="10" max="5000" value="100" onchange="peUpdateTestPreview()">
      </div>
      <pre id="pe-test-preview" style="min-height:54px;overflow:auto;background:var(--bg2);border:1px solid var(--bd);border-radius:6px;padding:8px;font-size:11px;color:var(--tx2);white-space:pre-wrap;word-break:break-word">Add a rule to preview a test frame.</pre>
      <div style="display:flex;gap:6px;align-items:center;margin-top:8px;flex-wrap:wrap">
        <button class="sniff-btn" onclick="peStartTest()">Start Test</button>
        <button class="sniff-btn" onclick="peStopTest()">Stop Test</button>
        <span id="pe-test-status" style="font-size:11px;color:var(--tx3)">Idle</span>
      </div>
    </div>
  </div>
  <div style="display:flex;gap:6px;margin-top:10px">
    <button class="sniff-btn" onclick="peInstall()">Install</button>
    <button class="sniff-btn" onclick="peDownload()">Download JSON</button>
    <button class="sniff-btn" onclick="peReset()">Reset</button>
  </div>
  <div id="pe-status" style="font-size:11px;margin-top:6px;color:var(--tx3)"></div>
</div>



<div class="card">
  <div class="card-hdr">
    <div class="card-title">Configuration <span class="title-help" onclick="return toggleHelp(this,event)" title="Device settings for hardware mode, WiFi, CAN pins, logging and backup.">?</span></div>
    <div class="card-meta">Device settings</div>
  </div>

  <div class="subsec" data-subkey="config-hardware">
    <div class="subsec-head">
      <div class="subsec-title">Hardware <span class="title-help" onclick="return toggleHelp(this,event)" title="Select the autopilot hardware generation and matching speed profile set.">?</span></div>
      <div class="subsec-meta">Autopilot generation</div>
    </div>
    <div class="subsec-body">
      <div class="hw-seg" id="hw-seg">
        <button class="hw-btn" data-v="0" onclick="setHW(0)">Legacy</button>
        <button class="hw-btn active" data-v="1" onclick="setHW(1)">HW3</button>
        <button class="hw-btn" data-v="2" onclick="setHW(2)">HW4</button>
      </div>
      <div class="profile-wrap">
        <div class="profile-label">Profile</div>
        <div class="profile-group" id="sp3-group">
          <div class="hw-seg" id="sp3-seg">
            <button class="hw-btn" data-v="0" onclick="setProfile(0)">Chill</button>
            <button class="hw-btn" data-v="1" onclick="setProfile(1)">Normal</button>
            <button class="hw-btn" data-v="2" onclick="setProfile(2)">Hurry</button>
          </div>
        </div>
        <div class="profile-group hidden" id="sp4-group">
          <div class="hw-seg" id="sp4-seg">
            <button class="hw-btn" data-v="0" onclick="setProfile(0)">Chill</button>
            <button class="hw-btn" data-v="1" onclick="setProfile(1)">Normal</button>
            <button class="hw-btn" data-v="2" onclick="setProfile(2)">Hurry</button>
            <button class="hw-btn" data-v="3" onclick="setProfile(3)">Max</button>
            <button class="hw-btn" data-v="4" onclick="setProfile(4)">Sloth</button>
          </div>
        </div>
        <div class="profile-note" id="profile-note">Available profiles depend on the selected hardware.</div>
      </div>
    </div>
  </div>

  <div class="subsec" data-subkey="config-wifi-hotspot">
    <div class="subsec-head">
      <div class="subsec-title">WiFi Hotspot <span class="title-help" onclick="return toggleHelp(this,event)" data-help-target="ap-info" title="Configure the device hotspot name, password and visibility. Saved in NVS.">?</span></div>
      <div class="subsec-meta"><span id="ap-stored" style="margin-right:8px"></span><span id="ap-clients">0 clients</span></div>
    </div>
    <div class="subsec-body">
      <div id="ap-info" class="info-box" style="display:none">
        Stored in NVS (non-volatile storage). The SSID and password survive firmware updates and reboots. Only a full factory erase via USB clears them.
      </div>
      <div class="setting-desc" style="margin-bottom:8px">Change the WiFi hotspot name and password</div>
      <div style="display:flex;gap:6px;margin-bottom:6px">
        <input class="sniff-input" id="ap-ssid" placeholder="Hotspot Name" style="flex:1">
        <input class="sniff-input" id="ap-pass" placeholder="New Password (min 8)" type="password" style="flex:1">
      </div>
      <div class="setting-row" style="padding:8px 0">
        <div class="setting-info">
          <div class="setting-name">Hide SSID</div>
          <div class="setting-desc">Don't broadcast the hotspot name &mdash; clients must enter it manually</div>
        </div>
        <label class="tgl"><input type="checkbox" id="ap-hidden"><div class="tgl-track"><div class="tgl-thumb"></div></div></label>
      </div>
      <div style="display:flex;gap:6px;align-items:center">
        <button class="sniff-btn" onclick="saveAP()">Save</button>
        <span style="font-size:11px;color:var(--tx3)" id="ap-status"></span>
      </div>
      <div style="font-size:10px;color:var(--tx3);margin-top:6px">Changes take effect after reboot. Leave password empty to keep current.</div>
    </div>
  </div>

  <div class="subsec" data-subkey="config-wifi-internet">
    <div class="subsec-head">
      <div class="subsec-title">WiFi Internet <span class="title-help" onclick="return toggleHelp(this,event)" title="Connect the device to an upstream WiFi network for updates and downloads. Saved in NVS.">?</span></div>
      <div class="subsec-meta" id="wifi-status">Not configured</div>
    </div>
    <div class="subsec-body">
      <div class="setting-desc" style="margin-bottom:8px">Connect to your home WiFi. Required for firmware updates and plugin downloads. Stored in NVS &mdash; survives firmware updates.</div>
      <div style="display:flex;gap:6px;margin-bottom:6px">
        <input class="sniff-input" id="wifi-ssid" placeholder="WiFi SSID" style="flex:1">
        <button class="sniff-btn" onclick="scanWifi()" id="scan-btn">Scan</button>
      </div>
      <div id="wifi-nets" style="display:none;margin-bottom:6px;max-height:140px;overflow-y:auto;border:1px solid var(--bd);border-radius:6px;background:var(--bg2)"></div>
      <div style="display:flex;gap:6px;margin-bottom:6px">
        <input class="sniff-input" id="wifi-pass" placeholder="Password" type="password" style="flex:1">
        <button class="sniff-btn" onclick="saveWifi()">Connect</button>
      </div>
      <details style="margin-top:4px">
        <summary style="font-size:11px;color:var(--acc);cursor:pointer;user-select:none">Static IP (optional) <span class="title-help" onclick="return toggleHelp(this,event)" title="Set a fixed IP configuration instead of using DHCP.">?</span></summary>
        <div style="margin-top:6px">
          <label style="font-size:11px;color:var(--tx3);display:flex;align-items:center;gap:6px;margin-bottom:6px">
            <input type="checkbox" id="wifi-static" onchange="toggleStaticIP()"> Use static IP
          </label>
          <div id="static-fields" style="display:none">
            <div style="display:grid;grid-template-columns:1fr 1fr;gap:4px">
              <input class="sniff-input" id="wifi-ip" placeholder="IP (e.g. 192.168.1.100)">
              <input class="sniff-input" id="wifi-gw" placeholder="Gateway (e.g. 192.168.1.1)">
              <input class="sniff-input" id="wifi-mask" placeholder="Mask (255.255.255.0)" value="255.255.255.0">
              <input class="sniff-input" id="wifi-dns" placeholder="DNS (e.g. 8.8.8.8)">
            </div>
          </div>
        </div>
      </details>
    </div>
  </div>

  <div class="subsec" data-subkey="config-can-pins">
    <div class="subsec-head">
      <div class="subsec-title">CAN Pins <span class="title-help" onclick="return toggleHelp(this,event)" data-help-target="can-pins-info" title="Set the ESP32 GPIO pins used for the CAN transceiver. Wrong values can disable CAN.">?</span></div>
      <div class="subsec-meta" id="can-pins-status">default</div>
    </div>
    <div class="subsec-body">
      <div id="can-pins-info" class="info-box">
        GPIO pins for the CAN transceiver (TWAI). Persisted in NVS so they survive OTA updates. Leave empty to use the firmware&#39;s compile-time defaults. <b>Wrong pins disable CAN</b> &mdash; recovery needs a USB re-flash. On most ESP32 boards GPIO 6&ndash;11 remain reserved for SPI flash.
      </div>
      <div style="display:flex;gap:6px;align-items:center">
        <input class="sniff-input" id="can-tx" type="number" min="0" max="39" placeholder="TX GPIO" style="flex:1">
        <input class="sniff-input" id="can-rx" type="number" min="0" max="39" placeholder="RX GPIO" style="flex:1">
        <button class="sniff-btn" onclick="saveCanPins()">Save</button>
      </div>
      <div style="font-size:11px;color:var(--tx3);margin-top:6px" id="can-pins-hint">Reboot required after change</div>
    </div>
  </div>

  <div class="subsec" data-subkey="config-dashboard-log" style="margin-top:14px">
    <div class="subsec-head">
      <div class="subsec-title">Dashboard Log <span class="title-help" onclick="return toggleHelp(this,event)" title="Shows recent dashboard and firmware log lines. This is the dashboard logging output, not the CAN sniffer.">?</span></div>
      <div class="subsec-meta">Recent dashboard output</div>
    </div>
    <div class="subsec-body">
      <div class="setting-row" style="padding-top:0">
        <div class="setting-info">
          <div class="setting-name">Dashboard Logging <span class="title-help" onclick="return toggleHelp(this,event)" title="Turns dashboard log output on or off.">?</span></div>
          <div class="setting-desc">Toggle dashboard log output</div>
        </div>
        <label class="tgl"><input type="checkbox" id="tgl-eprn" checked onchange="pushLogging()">
          <div class="tgl-track"><div class="tgl-thumb"></div></div></label>
      </div>
      <div class="log-box" id="log">Waiting...</div>
    </div>
  </div>
  <div class="setting-row" style="margin-top:14px;padding-top:12px;border-top:1px solid var(--bd)">
    <div class="setting-info">
      <div class="setting-name">Settings Backup <span class="title-help" onclick="return toggleHelp(this,event)" data-help-target="backup-info" title="Export or restore saved device settings as JSON.">?</span></div>
      <div class="setting-desc">Export and import device settings</div>
    </div>
    <button class="sniff-btn" onclick="exportSettings()">Download</button>
    <button class="sniff-btn" onclick="document.getElementById('backup-file').click()">Upload &amp; Restore</button>
    <input type="file" id="backup-file" accept=".json,application/json" style="display:none" onchange="importSettings(event)">
  </div>
  <div id="backup-info" class="info-box" style="display:none">
    Exports AP credentials, WiFi Internet, CAN pins and beta channel as JSON. Useful before a full re-flash or when migrating to another device. <b>Passwords are included in clear text</b> &mdash; keep the file safe.
  </div>
</div>

<div class="card">
  <div class="card-hdr">
    <div class="card-title">Firmware Update <span class="title-help" onclick="return toggleHelp(this,event)" title="Check for updates, enable beta builds and upload firmware manually.">?</span></div>
    <div class="card-meta" id="fw-ver">Version info</div>
  </div>
  <div style="margin-bottom:10px">
    <div class="setting-row">
      <div class="setting-info">
        <div class="setting-name">Beta Channel <span class="title-help" onclick="return toggleHelp(this,event)" title="Shows pre-release firmware versions when available.">?</span></div>
        <div class="setting-desc">Include pre-release / beta firmware versions</div>
      </div>
      <label class="tgl"><input type="checkbox" id="beta-tgl" onchange="toggleBeta()"><div class="tgl-track"><div class="tgl-thumb"></div></div></label>
    </div>
    <div class="setting-row">
      <div class="setting-info">
        <div class="setting-name">Auto-Update on Boot <span class="title-help" onclick="return toggleHelp(this,event)" title="Checks for firmware updates automatically shortly after WiFi connects.">?</span></div>
        <div class="setting-desc">Check and install updates automatically ~15 s after WiFi connects</div>
      </div>
      <label class="tgl"><input type="checkbox" id="auto-upd-tgl" onchange="toggleAutoUpdate()"><div class="tgl-track"><div class="tgl-thumb"></div></div></label>
    </div>
  </div>
  <div style="display:flex;gap:6px;align-items:center">
    <button class="sniff-btn" onclick="checkUpdate()" id="upd-check-btn">Check for Updates</button>
    <span style="font-size:11px;color:var(--tx3)" id="upd-status"></span>
  </div>
  <div id="upd-info" class="info-box" style="display:none;margin-top:10px">
    <div style="display:flex;justify-content:space-between;align-items:center">
      <div>
        <div style="font-size:13px;font-weight:600" id="upd-ver"></div>
        <div style="font-size:11px;color:var(--tx3)" id="upd-detail"></div>
      </div>
      <button class="sniff-btn" onclick="installUpdate()" id="upd-install-btn" style="background:var(--ok);color:#fff;border-color:var(--ok)">Install</button>
    </div>
  </div>

  <details style="margin-top:14px;padding-top:12px;border-top:1px solid var(--bd)">
    <summary style="font-size:12px;color:var(--acc);cursor:pointer;user-select:none">Manual firmware upload (.bin) <span class="title-help" onclick="return toggleHelp(this,event)" title="Upload a local firmware .bin file directly to the device.">?</span></summary>
    <div style="margin-top:10px">
      <div class="ota-drop" id="ota-drop" onclick="$('ota-file').click()" ondragover="event.preventDefault();this.classList.add('drag')" ondragleave="this.classList.remove('drag')" ondrop="handleDrop(event)">
        <input type="file" id="ota-file" accept=".bin" onchange="fileSelected(this.files[0])">
        <div class="ota-icon">&#8679;</div>
        <div class="ota-text">Tap to select firmware .bin</div>
        <div class="ota-sub">Or drag and drop a file here</div>
      </div>
      <div class="ota-progress" id="ota-progress">
        <div class="ota-bar"><div class="ota-fill" id="ota-fill"></div></div>
        <div class="ota-status" id="ota-status">Uploading...</div>
      </div>
      <button class="ota-btn" id="ota-upload-btn" onclick="uploadFirmware()">Flash Firmware</button>
      <div style="margin-top:10px;font-size:11px;color:var(--tx3);line-height:1.7">
        Build your .bin in PlatformIO: <span style="color:var(--acc);font-family:monospace">Ctrl+Alt+B</span><br>
        File is at: <span style="color:var(--acc);font-family:monospace">.pio/build/esp32_ext_mcp2515/firmware.bin</span>
      </div>
    </div>
  </details>
</div>
<div class="card">
  <div class="card-hdr"><div class="card-title">CAN <span class="title-help" onclick="return toggleHelp(this,event)" title="Live CAN tools for sniffing, recording, controller status and checking the last injected write.">?</span></div><div class="card-meta">Sniffer, recorder and bus status</div></div>

  <div class="subsec" data-subkey="can-sniffer">
    <div class="subsec-head">
      <div class="subsec-title">CAN Sniffer <span class="title-help" onclick="return toggleHelp(this,event)" title="Shows the latest 30 CAN frames live. You can filter by ID or name, switch between wire IDs and DBC IDs, and pause the view.">?</span></div>
      <div class="subsec-meta" id="sniff-count">0 frames</div>
    </div>
    <div class="subsec-body">
      <div class="sniff-ctrl">
        <input class="sniff-input" id="sniff-filter" placeholder="Filter by ID or name" oninput="renderSniffer()">
        <button class="sniff-btn" id="sniff-id-btn" onclick="toggleSniffIdMode()">Wire IDs</button>
        <button class="sniff-btn" id="sniff-pause-btn" onclick="togglePause()">Pause</button>
      </div>
      <div class="sniff-box" id="sniffer">
        <div style="padding:20px;color:var(--tx3);text-align:center;font-size:12px">Waiting for CAN frames</div>
      </div>
    </div>
  </div>

  <div class="subsec" data-subkey="can-recorder">
    <div class="subsec-head">
      <div class="subsec-title">CAN Recorder <span class="title-help" onclick="return toggleHelp(this,event)" title="Records live CAN traffic up to the frame limit and lets you download it as a CSV file.">?</span></div>
      <div class="subsec-meta" id="rec-meta">Idle</div>
    </div>
    <div class="subsec-body">
      <div class="rec-bar"><div class="rec-fill" id="rec-fill"></div></div>
      <div class="rec-info">
        <span id="rec-count">0 / 2000 frames</span>
        <span id="rec-status">Ready</span>
      </div>
      <div class="btn-row">
        <button class="btn" id="rec-btn" onclick="toggleRec()">Start Recording</button>
        <a class="btn" id="rec-dl" href="/rec_download" download="can_recording.csv" style="display:none;text-align:center;text-decoration:none;padding:10px;border:1px solid var(--bd2);color:var(--tx2)">Download CSV</a>
      </div>
    </div>
  </div>

  <div class="subsec" data-subkey="can-controller">
    <div class="subsec-head">
      <div class="subsec-title">CAN Controller <span class="title-help" onclick="return toggleHelp(this,event)" title="Shows CAN controller health, error flags and the RX, TX and error counters per mux.">?</span></div>
      <div class="subsec-meta" style="display:flex;align-items:center;gap:8px">
        <button onclick="resetStats()" style="font-size:10px;padding:2px 8px;border:1px solid var(--bd2);border-radius:5px;background:transparent;color:var(--tx3);cursor:pointer;font-family:inherit">Reset</button>
      </div>
    </div>
    <div class="subsec-body">
      <div class="eflg-row" id="eflg-row"><span class="eflg-pill eflg-ok">OK</span></div>
      <table class="mux-tbl">
        <tr><th>Mux</th><th>RX</th><th>TX</th><th>Errors</th></tr>
        <tr><td>0</td><td id="m0rx">0</td><td id="m0tx">0</td><td id="m0err">0</td></tr>
        <tr><td>1</td><td id="m1rx">0</td><td id="m1tx">0</td><td id="m1err">0</td></tr>
        <tr><td>2</td><td id="m2rx">0</td><td id="m2tx">0</td><td id="m2err">0</td></tr>
      </table>
    </div>
  </div>

  <div class="subsec" data-subkey="can-last-write-check">
    <div class="subsec-head">
      <div class="subsec-title">Last Write Check <span class="title-help" onclick="return toggleHelp(this,event)" title="Compares the last injected frame with the latest bus frame that has the same CAN ID and mux. Helpful to spot overwrites, but not proof that a module accepted the change.">?</span></div>
    </div>
    <div class="subsec-body">
      <div class="probe-status v-dim" id="probe-status">No injected frame yet</div>
      <div class="probe-block">
        <div class="probe-label">Sent</div>
        <div class="probe-meta" id="probe-tx-meta">—</div>
        <div class="probe-hex" id="probe-tx">—</div>
      </div>
      <div class="probe-block">
        <div class="probe-label">Bus</div>
        <div class="probe-meta" id="probe-rx-meta">—</div>
        <div class="probe-hex" id="probe-rx">—</div>
      </div>
    </div>
  </div>
</div>

<div class="warn-bar">CAN bus writes affect vehicle behavior. Remove device immediately if unexpected behavior occurs. Not affiliated with any vehicle manufacturer.</div>

<div class="modal-backdrop" id="confirm-modal" onclick="dashConfirmBackdrop(event)">
  <div class="modal-card" role="dialog" aria-modal="true" aria-labelledby="confirm-title">
    <div class="modal-title" id="confirm-title">Confirm</div>
    <div class="modal-msg" id="confirm-msg"></div>
    <div class="modal-actions">
      <button class="sniff-btn" id="confirm-cancel" onclick="dashConfirmResolve(false)">Cancel</button>
      <button class="sniff-btn modal-btn-primary" id="confirm-ok" onclick="dashConfirmResolve(true)">Continue</button>
    </div>
  </div>
</div>

<div class="foot" id="dash-foot">ev-open-can-tools &bull; loading...</div>
<div class="foot" style="margin-top:4px">
  <a href="https://github.com/ev-open-can-tools/ev-open-can-tools" target="_blank" rel="noopener" style="color:var(--acc);text-decoration:none">GitHub</a>
  &bull;
  <a href="https://discord.gg/ZTQKAUTd2F" target="_blank" rel="noopener" style="color:var(--acc);text-decoration:none">Discord</a>
</div>

<script>
const HW=['Legacy','HW3','HW4'];
const SP3=['Chill','Normal','Hurry'];
const SP4=['Chill','Normal','Hurry','Max','Sloth'];
const $=id=>document.getElementById(id);
const setText=(id,value)=>{const el=$(id);if(el)el.textContent=value;};
const setClass=(id,value)=>{const el=$(id);if(el)el.className=value;};
function profileNamesForHw(hw){return hw===2?SP4:SP3;}
let state={hw:1,can:true,sp:0};
let sniffPaused=false,sniffFrames=[];
let sniffShowDbcIds=localStorage.getItem('sniffIdMode')==='dbc';
let otaFile=null;
let otaUser=localStorage.getItem('otaU')||'',otaPass=localStorage.getItem('otaP')||'';
let logSince=0;
let installedPlugins=[];
let pluginMax=0;
let peLoadedPluginName='';
let peTestPollTimer=null;
let pluginDetailOpen={};
let dashConfirmState=null;
let dashboardPollTimers=[];
let dashboardPollFailures=0;
let dashboardStatusOk=false;
let dashboardInitialLoaded=false;
let dashboardPollStopped=false;
let dashboardStaIp='';
const pollLocks={};

function stopDashboardPolling(){
  if(dashboardPollStopped)return;
  dashboardPollStopped=true;
  dashboardPollTimers.forEach(clearInterval);
  dashboardPollTimers=[];
  $('dot').className='sdot dot-off';
  $('hdr-desc').textContent='Dashboard disconnected';
  let msg='Connection to '+location.hostname+' lost. Reload after reconnecting.';
  if(dashboardStaIp&&dashboardStaIp!==location.hostname)msg='Connection to '+location.hostname+' lost. Switch to your normal WiFi and open http://'+dashboardStaIp;
  $('wifi-status').textContent=msg;
  $('wifi-status').style.color='var(--err)';
}

function noteDashboardPoll(ok){
  if(ok){dashboardPollFailures=0;dashboardStatusOk=true;return;}
  if(dashboardPollStopped)return;
  dashboardStatusOk=false;
  dashboardPollFailures++;
  $('dot').className='sdot dot-off';
  $('hdr-desc').textContent='Dashboard reconnecting';
}

async function fetchPollJson(url,timeoutMs,trackConnection){
  const ctrl=new AbortController();
  const timer=setTimeout(()=>ctrl.abort(),timeoutMs||2500);
  try{
    const r=await fetch(url,{signal:ctrl.signal});
    if(!r.ok)throw new Error('HTTP '+r.status);
    const d=await r.json();
    if(trackConnection)noteDashboardPoll(true);
    return d;
  }catch(e){
    if(trackConnection)noteDashboardPoll(false);
    throw e;
  }finally{
    clearTimeout(timer);
  }
}

async function runPoll(name,fn){
  if(dashboardPollStopped||pollLocks[name])return;
  pollLocks[name]=true;
  try{return await fn();}finally{pollLocks[name]=false;}
}

function waitMs(ms){return new Promise(resolve=>setTimeout(resolve,ms));}

function initCardMinimizers(){
  document.querySelectorAll('.card').forEach((card,i)=>{
    const hdr=card.querySelector('.card-hdr');if(!hdr||hdr.querySelector('.card-min-btn'))return;
    const title=card.querySelector('.card-title');
    const key='cardCollapse:'+i+':'+((title?title.textContent:'card').trim().toLowerCase().replace(/[^a-z0-9]+/g,'-'));
    card.dataset.collapseKey=key;
    const btn=document.createElement('button');
    btn.type='button';
    btn.className='sniff-btn card-min-btn';
    btn.onclick=()=>{
      const collapsed=!card.classList.contains('collapsed');
      card.classList.toggle('collapsed',collapsed);
      localStorage.setItem(key,collapsed?'1':'0');
      btn.textContent=collapsed?'Show':'Hide';
    };
    hdr.appendChild(btn);
    const collapsed=localStorage.getItem(key)==='1';
    card.classList.toggle('collapsed',collapsed);
    btn.textContent=collapsed?'Show':'Hide';
  });
}
function initSubsectionMinimizers(){
  document.querySelectorAll('.subsec').forEach((sec,i)=>{
    const hdr=sec.querySelector('.subsec-head');if(!hdr||hdr.querySelector('.subsec-btn'))return;
    const explicitKey=sec.dataset.subkey||'';
    const title=sec.querySelector('.subsec-title');
    const safe=((title?title.textContent:'section').trim().toLowerCase().replace(/[^a-z0-9]+/g,'-'));
    const key='subCollapse:'+(explicitKey||i+':'+safe);
    sec.dataset.collapseKey=key;
    const btn=document.createElement('button');
    btn.type='button';
    btn.className='sniff-btn subsec-btn';
    btn.onclick=()=>{
      const collapsed=!sec.classList.contains('collapsed');
      sec.classList.toggle('collapsed',collapsed);
      localStorage.setItem(key,collapsed?'1':'0');
      btn.textContent=collapsed?'Show':'Hide';
    };
    hdr.appendChild(btn);
    const collapsed=localStorage.getItem(key)==='1';
    sec.classList.toggle('collapsed',collapsed);
    btn.textContent=collapsed?'Show':'Hide';
  });
}

function syncSniffPauseButton(){
  const b=$('sniff-pause-btn');if(!b)return;
  b.textContent=sniffPaused?'Resume':'Pause';
  b.classList.toggle('paused',sniffPaused);
}

function actionErrorMessage(e,fallback){
  if(!e)return fallback;
  if(e.name==='AbortError'||e.name==='SyntaxError'||e.message==='Failed to fetch'||e.message==='Empty response')return fallback;
  return e.message||fallback;
}

async function fetchJsonWithTimeout(url,options,timeoutMs){
  const ctrl=new AbortController();
  const timer=setTimeout(()=>ctrl.abort(),timeoutMs||2500);
  try{
    const opts=Object.assign({},options||{});
    opts.signal=ctrl.signal;
    const r=await fetch(url,opts);
    const text=await r.text();
    if(!text||!text.trim())throw new Error(r.ok?'Empty response':('HTTP '+r.status));
    const d=JSON.parse(text);
    if(!r.ok)throw new Error(d.error||('HTTP '+r.status));
    return d;
  }finally{
    clearTimeout(timer);
  }
}

function dashConfirmResolve(ok){
  if(!dashConfirmState)return;
  const resolve=dashConfirmState.resolve;
  dashConfirmState=null;
  $('confirm-modal').style.display='none';
  document.body.style.overflow='';
  resolve(!!ok);
}

function dashConfirmBackdrop(ev){
  if(ev.target===$('confirm-modal'))dashConfirmResolve(false);
}

function dashConfirm(message,title,okText,cancelText){
  if(dashConfirmState)dashConfirmResolve(false);
  return new Promise(resolve=>{
    dashConfirmState={resolve};
    $('confirm-title').textContent=title||'Confirm';
    $('confirm-msg').textContent=message||'';
    $('confirm-ok').textContent=okText||'Continue';
    $('confirm-cancel').textContent=cancelText||'Cancel';
    $('confirm-modal').style.display='flex';
    document.body.style.overflow='hidden';
    setTimeout(()=>{$('confirm-ok').focus();},0);
  });
}

document.addEventListener('keydown',e=>{
  if(e.key==='Escape'){
    if(dashConfirmState)dashConfirmResolve(false);
    closeHelpPanels(document);
  }
});
document.addEventListener('click',e=>{
  if(!e.target.closest('.title-help')&&!e.target.closest('.inline-help-panel')){
    closeHelpPanels(document);
  }
});

function toggleTheme(){
  const html=document.documentElement;
  const isDark=html.getAttribute('data-theme')==='dark';
  html.setAttribute('data-theme',isDark?'light':'dark');
  $('theme-btn').innerHTML=isDark?'&#9790; Dark':'&#9788; Light';
  localStorage.setItem('theme',isDark?'light':'dark');
}
(function(){
  const t=localStorage.getItem('theme')||'dark';
  document.documentElement.setAttribute('data-theme',t);
  // will be updated after DOM ready
  window.addEventListener('DOMContentLoaded',()=>{
    $('theme-btn').innerHTML=t==='dark'?'&#9788; Light':'&#9790; Dark';
  });
})();

function updateHW4(hw){
  document.querySelectorAll('.hw4-only').forEach(el=>el.classList.toggle('hidden',hw!==2));
}

function clampProfileForHw(hw,sp){
  if(hw===2)return Math.max(0,Math.min(4,Number(sp)||0));
  if(hw===1)return Math.max(0,Math.min(2,Number(sp)||0));
  return 0;
}

function updateProfileControls(hw,sp){
  const sp3=$('sp3-group'),sp4=$('sp4-group'),note=$('profile-note');
  const safeSp=clampProfileForHw(hw,sp);
  if(sp3)sp3.classList.toggle('hidden',hw!==1);
  if(sp4)sp4.classList.toggle('hidden',hw!==2);
  const sp3Seg=$('sp3-seg'),sp4Seg=$('sp4-seg');
  if(sp3Seg)updSeg(sp3Seg,safeSp,'hw-btn');
  if(sp4Seg)updSeg(sp4Seg,safeSp,'hw-btn');
  if(note){
    if(hw===1)note.textContent='SP3 profiles are available on HW3.';
    else if(hw===2)note.textContent='SP4 profiles are available on HW4.';
    else note.textContent='Profiles are only available on HW3 and HW4.';
  }
}

function updSeg(el,v,cls){
  el.querySelectorAll('.'+cls).forEach(b=>b.classList.toggle('active',parseInt(b.dataset.v)===v));
}

function setHW(v){state.hw=v;state.sp=clampProfileForHw(v,state.sp);updSeg($('hw-seg'),v,'hw-btn');updateHW4(v);updateProfileControls(v,state.sp);updateSniffIdToggle();renderSniffer();pushCfg();}

function setProfile(v){
  state.sp=clampProfileForHw(state.hw,v);
  updateProfileControls(state.hw,state.sp);
  pushCfg();
}

function updateInjectButtons(active){
  const stop=$('btn-stop'),resume=$('btn-resume');
  if(stop)stop.style.display=active?'':'none';
  if(resume)resume.style.display=active?'none':'';
}

function sniffBusPrefix(){return state.hw===0?0x0800:0x1000;}
function sniffBusLabel(){return state.hw===0?'PARTY':'CH';}
function sniffWireId(id){return id&0x7FF;}
function sniffDbcId(id){return sniffWireId(id)|sniffBusPrefix();}
function sniffDisplayId(id){return sniffShowDbcIds?sniffDbcId(id):sniffWireId(id);}
function updateSniffIdToggle(){
  const b=$('sniff-id-btn'),bus=sniffBusLabel();
  b.textContent=sniffShowDbcIds?('DBC '+bus):'Wire IDs';
  b.title=sniffShowDbcIds?('Showing DBC JSON IDs with '+bus+' prefix'):('Showing on-wire 11-bit CAN IDs');
  $('sniff-filter').placeholder='Filter by wire/DBC ID or name';
}
function toggleSniffIdMode(){
  sniffShowDbcIds=!sniffShowDbcIds;
  localStorage.setItem('sniffIdMode',sniffShowDbcIds?'dbc':'wire');
  updateSniffIdToggle();
  renderSniffer();
}

async function pushCfg(){
  const body='hw='+state.hw+'&sp='+state.sp+'&can='+(state.can?'1':'0');
  try{await fetch('/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});}catch(e){}
}

async function pushLogging(){
  const body='eprn='+($('tgl-eprn').checked?'1':'0');
  try{await fetch('/logging',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});}catch(e){}
  if($('tgl-eprn').checked)pollLog();
  poll();
}

async function emergencyStop(){if(!await dashConfirm('Stop injecting? This remains disabled after reboot until you press Resume Injection.','Stop injection','Stop'))return;try{await fetch('/disable',{method:'POST'});}catch(e){}poll();}
async function resumeInj(){try{await fetch('/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'hw='+state.hw+'&sp='+state.sp+'&can=1'});}catch(e){}poll();}
async function reboot(){if(!await dashConfirm('Reboot device?','Reboot','Reboot'))return;try{await fetch('/reboot',{method:'POST'});}catch(e){}}

function fmtUp(s){
  if(s<60)return s+'s';
  if(s<3600)return Math.floor(s/60)+'m '+String(s%60).padStart(2,'0')+'s';
  return Math.floor(s/3600)+'h '+Math.floor((s%3600)/60)+'m';
}
function fmtAgeMs(ms){
  if(ms<1000)return ms+' ms';
  if(ms<10000)return (ms/1000).toFixed(1)+' s';
  if(ms<60000)return Math.round(ms/1000)+' s';
  return fmtUp(Math.floor(ms/1000));
}
function toHex(n,p){return n.toString(16).toUpperCase().padStart(p,'0')}
function fmtProbeData(data,dlc){
  if(!Array.isArray(data)||!dlc)return '—';
  return data.slice(0,dlc).map(v=>toHex((v||0)&255,2)).join(' ');
}
function renderWriteProbe(p){
  const status=$('probe-status');
  if(!p||!p.active){
    status.textContent='No injected frame yet';
    status.className='probe-status v-dim';
    $('probe-tx-meta').textContent='—';
    $('probe-tx').textContent='—';
    $('probe-rx-meta').textContent='—';
    $('probe-rx').textContent='—';
    return;
  }
  const id='CAN 0x'+toHex((p.id||0)&0x7FF,3)+(p.mux>=0?' · mux '+p.mux:'');
  $('probe-tx-meta').textContent=id+' · '+fmtAgeMs(p.txa||0)+' ago';
  $('probe-tx').textContent=fmtProbeData(p.tx,p.txdlc);
  if(p.hasrx){
    $('probe-rx-meta').textContent=id+' · '+fmtAgeMs(p.rxa||0)+' ago';
    $('probe-rx').textContent=fmtProbeData(p.rx,p.rxdlc);
  }else{
    $('probe-rx-meta').textContent='No matching RX frame seen yet';
    $('probe-rx').textContent='—';
  }
  let text='Waiting for next matching bus frame';
  let cls='probe-status v-acc';
  if(p.state===2){text='Matching frame seen on bus';cls='probe-status v-ok';}
  else if(p.state===3){text='Latest bus frame differs from injected frame';cls='probe-status v-warn';}
  else if(p.state===4){text='Driver transmit failed';cls='probe-status v-err';}
  status.textContent=text;
  status.className=cls;
}

function renderEflg(e){
  const el=$('eflg-row');
  if(!e){el.innerHTML='<span class="eflg-pill eflg-ok">OK</span>';return;}
  let h='';
  if(e&0x20)h+='<span class="eflg-pill eflg-err">Bus-Off</span>';
  if(e&0x10)h+='<span class="eflg-pill eflg-warn">TX Passive</span>';
  if(e&0x08)h+='<span class="eflg-pill eflg-warn">RX Passive</span>';
  if(e&0x04)h+='<span class="eflg-pill eflg-warn">TX Warn</span>';
  if(e&0x02)h+='<span class="eflg-pill eflg-warn">RX Warn</span>';
  if(e&0xC0)h+='<span class="eflg-pill eflg-err">RX Overflow</span>';
  el.innerHTML=h||'<span class="eflg-pill eflg-ok">OK</span>';
}

function togglePause(){
  sniffPaused=!sniffPaused;
  syncSniffPauseButton();
  renderSniffer();
}

function renderSniffer(){
  updateSniffIdToggle();
  const filter=$('sniff-filter').value.trim().toLowerCase();
  const el=$('sniffer');
  let frames=sniffFrames;
  if(filter){
    const fid=parseInt(filter);
    if(!isNaN(fid))frames=frames.filter(f=>sniffWireId(f.id)===fid||sniffDbcId(f.id)===fid);
    else frames=frames.filter(f=>f.name&&f.name.toLowerCase().includes(filter));
  }
  $('sniff-count').textContent=frames.length+' frames';
  if(!frames.length){
    el.innerHTML='<div style="padding:20px;color:var(--tx3);text-align:center;font-size:12px">'+(sniffPaused?'Sniffer paused':'No frames')+'</div>';
    return;
  }
  const ADIds=new Set([1021,1016,921]);
  el.innerHTML=frames.slice(-30).reverse().map(f=>{
    const hex=Array.from({length:f.dlc},(_,i)=>toHex(f.data[i],2)).join(' ');
    const wireId=sniffWireId(f.id),dbcId=sniffDbcId(f.id),displayId=sniffDisplayId(f.id);
    const altId=sniffShowDbcIds?('Wire 0x'+toHex(wireId,3)):('DBC '+sniffBusLabel()+' 0x'+toHex(dbcId,3));
    return`<div class="sniff-row${ADIds.has(f.id)?' hi':''}">
      <span class="s-ts">${(f.ts/1000).toFixed(1)}s</span>
      <span class="s-id" title="${altId}">0x${toHex(displayId,3)}</span>
      <div><div class="s-data">${hex}</div>${f.name?`<div class="s-name">${f.name}</div>`:''}</div>
    </div>`;
  }).join('');
}

async function pollSniffer(){
  return runPoll('frames',async()=>{
    if(sniffPaused||!dashboardStatusOk)return;
    try{const d=await fetchPollJson('/frames',2500);sniffFrames=d.frames||[];renderSniffer();}catch(e){}
  });
}

// OTA upload
function fileSelected(file){
  if(!file)return;
  otaFile=file;
  const drop=$('ota-drop');
  drop.querySelector('.ota-text').textContent=file.name;
  drop.querySelector('.ota-sub').textContent=(file.size/1024).toFixed(0)+' KB';
  $('ota-upload-btn').style.display='block';
}

function handleDrop(e){
  e.preventDefault();
  $('ota-drop').classList.remove('drag');
  const file=e.dataTransfer.files[0];
  if(file&&file.name.endsWith('.bin'))fileSelected(file);
}

async function uploadFirmware(){
  if(!otaFile)return;
  if(!otaUser){otaUser=prompt('OTA Username:')||'';localStorage.setItem('otaU',otaUser);}
  if(!otaPass){otaPass=prompt('OTA Password:')||'';localStorage.setItem('otaP',otaPass);}
  if(!otaUser||!otaPass)return;
  const prog=$('ota-progress');
  const fill=$('ota-fill');
  const status=$('ota-status');
  prog.style.display='block';
  $('ota-upload-btn').disabled=true;
  $('ota-upload-btn').textContent='Flashing...';

  const xhr=new XMLHttpRequest();
  xhr.upload.onprogress=e=>{
    if(e.lengthComputable){
      const pct=Math.round(e.loaded/e.total*100);
      fill.style.width=pct+'%';
      status.textContent='Uploading... '+pct+'%';
    }
  };
  xhr.onload=()=>{
    if(xhr.status===200){
      status.textContent='Done! Device is rebooting...';
      fill.style.width='100%';
      setTimeout(()=>window.location.reload(),5000);
    } else {
      status.textContent='Upload failed: '+xhr.status;
      status.style.color='var(--err)';
    }
    $('ota-upload-btn').disabled=false;
    $('ota-upload-btn').textContent='Flash Firmware';
  };
  xhr.onerror=()=>{
    status.textContent='Connection error';
    status.style.color='var(--err)';
    $('ota-upload-btn').disabled=false;
  };
  xhr.open('POST','/update',true,otaUser,otaPass);
  xhr.setRequestHeader('X-File-Name',otaFile.name);
  xhr.setRequestHeader('X-File-Size',otaFile.size);
  const form=new FormData();
  form.append('firmware',otaFile);
  xhr.send(form);
}

async function poll(){
  return runPoll('status',async()=>{
    try{
      const d=await fetchPollJson('/status',5000,true);
    const on=!!d.can,injecting=!!d.ci,fpsVal=Number(d.fps||0);
    state.hw=d.hw;state.sp=clampProfileForHw(d.hw,d.sp);state.can=injecting;
    setClass('dot','sdot '+(d.txerr>5?'dot-warn':on?'dot-on':'dot-off'));
    setText('hdr-desc',on?(d.AD?'AD active — injecting':'CAN active — monitoring'):'Waiting for CAN frames');
    updateInjectButtons(injecting);

    setText('s-can',on?'Active':'Offline');
    setClass('s-can','stat-val '+(on?'v-ok':'v-err'));
    setText('s-inj',injecting?'Active':'BLOCKED');
    setClass('s-inj','stat-val '+(injecting?'v-ok':'v-err'));
    setText('s-AD',d.AD?'Active':'Inactive');
    setClass('s-AD','stat-val '+(d.AD?'v-ok':'v-dim'));
    setText('s-fps',fpsVal.toFixed(1)+' Hz');
    setClass('s-fps','stat-val '+(fpsVal>5?'v-acc':'v-dim'));
    setText('s-rx',d.rx);
    setText('s-tx',d.tx);
    setText('s-txerr',d.txerr);
    setClass('s-txerr','stat-val '+(d.txerr>0?'v-warn':'v-dim'));
    setText('s-fd',d.fd||'—');
    setText('s-prof',profileNamesForHw(d.hw)[d.sp]||'—');
    setText('s-soff',d.soff||'0');
    setText('s-up',fmtUp(d.up));
    setText('s-mcp-raw','EFLG: 0x'+toHex(d.eflg,2));
    const fpsFill=$('fps-fill');if(fpsFill)fpsFill.style.width=Math.min(fpsVal/20*100,100)+'%';
    setText('hw-badge',HW[d.hw]||'?');
    try{renderEflg(d.eflg);}catch(e){}
    try{renderWriteProbe(d.probe);}catch(e){}
    if(d.mux){for(let i=0;i<3;i++){setText('m'+i+'rx',d.mux[i].rx);setText('m'+i+'tx',d.mux[i].tx);const e=$('m'+i+'err');if(e){e.textContent=d.mux[i].err;e.style.color=d.mux[i].err>0?'var(--err)':'';}}}
    updateSniffIdToggle();
    const hwSeg=$('hw-seg');if(hwSeg)updSeg(hwSeg,d.hw,'hw-btn');updateHW4(d.hw);updateProfileControls(d.hw,state.sp);
    const eprn=$('tgl-eprn');if(eprn&&typeof d.eprn!=='undefined')eprn.checked=d.eprn;
    if(!dashboardInitialLoaded){
      dashboardInitialLoaded=true;
      pollLog();pollSniffer();pollPlugins();loadWifiStatus();loadApStatus();loadUpdateInfo();loadCanPins();
    }
    }catch(e){}
  });
}

function colorLog(l){
  if(l.includes('AD=ON')||l.includes('AD active'))return'<span class="lf">'+l+'</span>';
  if(l.match(/\[HW[34]\]|\[LEGACY\]|\[HW3\]/))return'<span class="lh">'+l+'</span>';
  if(l.includes('ERR')||l.includes('FAIL'))return'<span class="le">'+l+'</span>';
  if(l.includes('[CFG]'))return'<span class="lc">'+l+'</span>';
  if(l.includes('[OK]')||l.includes('[BOOT]'))return'<span class="lf">'+l+'</span>';
  if(l.includes('[OTA]'))return'<span class="lo">'+l+'</span>';
  return l;
}
async function pollLog(){
  return runPoll('log',async()=>{
    if(!$('tgl-eprn').checked||!dashboardStatusOk)return;
    try{
      const d=await fetchPollJson('/log?since='+logSince,2000);
    if(d.seq)logSince=d.seq;
    if(!d.lines.length)return;
    const el=$('log');
    const newHtml=d.lines.map(colorLog).join('\n');
    if(el.textContent==='Waiting...')el.innerHTML=newHtml;
    else el.innerHTML+='\n'+newHtml;
    // trim to 100 lines
    const lines=el.innerHTML.split('\n');
    if(lines.length>100)el.innerHTML=lines.slice(-100).join('\n');
    el.scrollTop=el.scrollHeight;
    }catch(e){}
  });
}

async function resetStats(){try{await fetch('/reset_stats',{method:'POST'});}catch(e){}poll();}

let recIsActive=false,recInterval=null;
async function toggleRec(){recIsActive?await stopRec():await startRec();}
async function startRec(){
  try{
    await fetch('/rec_start',{method:'POST'});
    recIsActive=true;
    const b=$('rec-btn');
    b.textContent='Stop Recording';
    b.style.borderColor='var(--err)';b.style.color='var(--err)';
    $('rec-dl').style.display='none';
    recInterval=setInterval(pollRec,800);
  }catch(e){}
}
async function stopRec(){
  clearInterval(recInterval);recIsActive=false;
  try{await fetch('/rec_stop',{method:'POST'});}catch(e){}
  const b=$('rec-btn');
  b.textContent='Start Recording';b.style.borderColor='';b.style.color='';
  await pollRec();
}
async function pollRec(){
  try{
    const d=await(await fetch('/rec_status')).json();
    const pct=Math.min(d.count/d.cap*100,100);
    $('rec-fill').style.width=pct+'%';
    $('rec-count').textContent=d.count+' / '+d.cap+' frames';
    if(d.active){
      $('rec-status').textContent='Recording...';$('rec-status').style.color='var(--err)';
      $('rec-meta').textContent='Recording...';
    } else {
      $('rec-meta').textContent=d.saved?d.count+' frames saved':'Idle';
      $('rec-status').textContent=d.saved?'Saved':'Ready';
      $('rec-status').style.color=d.saved?'var(--ok)':'';
      $('rec-dl').style.display=d.saved?'':'none';
      if(recIsActive){recIsActive=false;clearInterval(recInterval);const b=$('rec-btn');b.textContent='Start Recording';b.style.borderColor='';b.style.color='';}
    }
  }catch(e){}
}

// ── AP Hotspot management ──
async function saveAP(){
  const ssid=$('ap-ssid').value,pass=$('ap-pass').value,hidden=$('ap-hidden').checked?'1':'0';
  if(!ssid){$('ap-status').textContent='Enter hotspot name';$('ap-status').style.color='var(--err)';return;}
  if(pass&&pass.length<8){$('ap-status').textContent='Password min 8 chars';$('ap-status').style.color='var(--err)';return;}
  try{const r=await fetch('/ap_config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass)+'&hidden='+hidden});
    const d=await r.json();
    if(d.ok){$('ap-status').textContent='Saved! Reboot to apply.';$('ap-status').style.color='var(--ok)';$('ap-pass').value='';}
    else{$('ap-status').textContent=d.error||'Error';$('ap-status').style.color='var(--err)';}
  }catch(e){$('ap-status').textContent='Error';$('ap-status').style.color='var(--err)';}
}
async function loadApStatus(){
  return runPoll('ap_status',async()=>{
    if(!dashboardStatusOk)return;
    try{const d=await fetchPollJson('/ap_status',2000);
    if(d.ssid)$('ap-ssid').value=d.ssid;
    $('ap-clients').textContent=d.clients+' client'+(d.clients!==1?'s':'');
    if(typeof d.hidden!=='undefined')$('ap-hidden').checked=!!d.hidden;
    if(d.stored){$('ap-stored').textContent='saved';$('ap-stored').style.color='var(--ok)';}
    else{$('ap-stored').textContent='firmware default';$('ap-stored').style.color='var(--tx3)';}
    }catch(e){}
  });
}
// ── WiFi management ──
function toggleStaticIP(){
  $('static-fields').style.display=$('wifi-static').checked?'block':'none';
}
function rssiIcon(r){
  if(r>=-50) return '\u2587\u2587\u2587\u2587';
  if(r>=-60) return '\u2587\u2587\u2587\u2581';
  if(r>=-70) return '\u2587\u2587\u2581\u2581';
  return '\u2587\u2581\u2581\u2581';
}
async function scanWifi(){
  $('scan-btn').textContent='Scanning...';$('scan-btn').disabled=true;
  try{
    const r=await fetch('/wifi_scan');const d=await r.json();
    const el=$('wifi-nets');
    if(!d.networks.length){el.innerHTML='<div style="padding:8px;font-size:11px;color:var(--tx3);text-align:center">No networks found</div>';el.style.display='block';}
    else{el.innerHTML=d.networks.map(n=>'<div onclick="pickWifi(\''+n.ssid.replace(/'/g,"\\'")+'\')" style="padding:6px 10px;cursor:pointer;display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid var(--bd);font-size:12px" onmouseover="this.style.background=\'var(--bg)\'" onmouseout="this.style.background=\'\'"><span>'+(n.enc?'\uD83D\uDD12 ':'')+n.ssid+'</span><span style="color:var(--tx3);font-size:10px">'+rssiIcon(n.rssi)+' '+n.rssi+'dBm CH'+n.ch+'</span></div>').join('');el.style.display='block';}
  }catch(e){$('wifi-status').textContent='Scan failed';$('wifi-status').style.color='var(--err)';}
  $('scan-btn').textContent='Scan';$('scan-btn').disabled=false;
}
function pickWifi(ssid){
  $('wifi-ssid').value=ssid;$('wifi-nets').style.display='none';$('wifi-pass').focus();
}
async function loadWifiStatus(){
  return runPoll('wifi_status',async()=>{
    if(!dashboardStatusOk)return;
    try{const d=await fetchPollJson('/wifi_status',2000);
    dashboardStaIp=d.connected&&d.ip?d.ip:'';
    if(d.ssid)$('wifi-ssid').value=d.ssid;
    if(d.stored){$('wifi-stored').textContent='\u2022 saved';$('wifi-stored').style.color='var(--ok)';}
    else{$('wifi-stored').textContent='';}
    if(d.connected){
      $('wifi-status').textContent=(d.ip&&d.ip!==location.hostname)?('Connected: '+d.ip+' \u2022 switch to that WiFi and open this IP'):('Connected: '+d.ip);
      $('wifi-status').style.color='var(--ok)';
    }
    else if(d.ssid){$('wifi-status').textContent='Connecting to '+d.ssid+'...';$('wifi-status').style.color='var(--acc)';}
    if(d.static){$('wifi-static').checked=true;toggleStaticIP();
      if(d.cfg_ip)$('wifi-ip').value=d.cfg_ip;
      if(d.cfg_gw)$('wifi-gw').value=d.cfg_gw;
      if(d.cfg_mask)$('wifi-mask').value=d.cfg_mask;
      if(d.cfg_dns)$('wifi-dns').value=d.cfg_dns;
    }
    }catch(e){}
  });
}
async function saveWifi(){
  const ssid=$('wifi-ssid').value,pass=$('wifi-pass').value;
  if(!ssid){$('wifi-status').textContent='Enter SSID';return;}
  let body='ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass);
  if($('wifi-static').checked){
    body+='&static=1&ip='+encodeURIComponent($('wifi-ip').value)+'&gw='+encodeURIComponent($('wifi-gw').value)+'&mask='+encodeURIComponent($('wifi-mask').value)+'&dns='+encodeURIComponent($('wifi-dns').value);
  }
  try{await fetch('/wifi_config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    $('wifi-status').textContent='Connecting to '+ssid+'...';$('wifi-status').style.color='var(--acc)';
  }catch(e){$('wifi-status').textContent='Error';$('wifi-status').style.color='var(--err)';}
}
// ── Plugin management ──
async function installPlugin(){
  const url=$('plg-url').value;
  if(!url){$('plg-status').textContent='Enter URL';return;}
  const beforeSig=pluginStateSignature(installedPlugins);
  $('plg-status').textContent='Downloading...';$('plg-status').style.color='var(--acc)';
  try{
    await fetchJsonWithTimeout('/plugin_install',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'url='+encodeURIComponent(url)},20000);
    $('plg-url').value='';
    try{await refreshPluginsNow();}catch(e){await refreshPluginsAfterAction(beforeSig);}
    $('plg-status').textContent='Installed';$('plg-status').style.color='var(--ok)';
  }catch(e){
    if(await refreshPluginsAfterAction(beforeSig)){
      $('plg-url').value='';
      $('plg-status').textContent='Installed';$('plg-status').style.color='var(--ok)';
    }else{$('plg-status').textContent=actionErrorMessage(e,'Connection error');$('plg-status').style.color='var(--err)';}
  }
}
async function uploadPlugin(file){
  if(!file)return;
  const beforeSig=pluginStateSignature(installedPlugins);
  $('plg-status').textContent='Uploading...';$('plg-status').style.color='var(--acc)';
  try{
    const text=await file.text();
    await fetchJsonWithTimeout('/plugin_upload',{method:'POST',headers:{'Content-Type':'application/json'},body:text},5000);
    try{await refreshPluginsNow();}catch(e){await refreshPluginsAfterAction(beforeSig);}
    $('plg-status').textContent='Installed';$('plg-status').style.color='var(--ok)';
  }catch(e){
    if(await refreshPluginsAfterAction(beforeSig)){
      $('plg-status').textContent='Installed';$('plg-status').style.color='var(--ok)';
    }else{$('plg-status').textContent=actionErrorMessage(e,'Error');$('plg-status').style.color='var(--err)';}
  }
}
async function pastePlugin(){
  const text=$('plg-paste').value.trim();
  if(!text){$('plg-status').textContent='Paste JSON first';$('plg-status').style.color='var(--err)';return;}
  try{JSON.parse(text);}catch(e){$('plg-status').textContent='Invalid JSON: '+e.message;$('plg-status').style.color='var(--err)';return;}
  const beforeSig=pluginStateSignature(installedPlugins);
  $('plg-status').textContent='Installing...';$('plg-status').style.color='var(--acc)';
  try{
    await fetchJsonWithTimeout('/plugin_upload',{method:'POST',headers:{'Content-Type':'application/json'},body:text},5000);
    $('plg-paste').value='';
    try{await refreshPluginsNow();}catch(e){await refreshPluginsAfterAction(beforeSig);}
    $('plg-status').textContent='Installed';$('plg-status').style.color='var(--ok)';
  }catch(e){
    if(await refreshPluginsAfterAction(beforeSig)){
      $('plg-paste').value='';
      $('plg-status').textContent='Installed';$('plg-status').style.color='var(--ok)';
    }else{$('plg-status').textContent=actionErrorMessage(e,'Connection error');$('plg-status').style.color='var(--err)';}
  }
}
async function togglePlugin(idx){
  const beforeSig=pluginStateSignature(installedPlugins);
  try{
    const d=await fetchJsonWithTimeout('/plugin_toggle',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'idx='+idx},4000);
    if(installedPlugins[idx]&&typeof d.enabled==='boolean'){
      installedPlugins[idx].enabled=d.enabled;
      renderPluginsState({plugins:installedPlugins,maxPlugins:pluginMax});
    }else{
      try{await refreshPluginsNow();}catch(e){await refreshPluginsAfterAction(beforeSig);}
    }
  }catch(e){await refreshPluginsAfterAction(beforeSig);}
}
async function removePlugin(idx){
  if(!await dashConfirm('Remove this plugin?','Remove plugin','Remove'))return;
  const beforeSig=pluginStateSignature(installedPlugins);
  try{
    await fetchJsonWithTimeout('/plugin_remove',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'idx='+idx},4000);
    try{await refreshPluginsNow();}catch(e){await refreshPluginsAfterAction(beforeSig);}
  }catch(e){await refreshPluginsAfterAction(beforeSig);}
}
async function setPluginPriority(idx,value){
  const to=parseInt(value,10)-1;
  if(isNaN(to)||to===idx){renderPluginsState({plugins:installedPlugins,maxPlugins:pluginMax});return;}
  const beforeSig=pluginStateSignature(installedPlugins);
  try{
    await fetchJsonWithTimeout('/plugin_priority',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'idx='+idx+'&priority='+to},4000);
    try{await refreshPluginsNow();}catch(e){await refreshPluginsAfterAction(beforeSig);}
  }catch(e){await refreshPluginsAfterAction(beforeSig);}
}
function pluginByteBits(byte,mask){
  byte=parseInt(byte,10);mask=(parseInt(mask,10)||0)&255;
  const bits=[];
  if(isNaN(byte)||byte<0||byte>7)return bits;
  for(let b=0;b<8;b++)if(mask&(1<<b))bits.push(byte*8+b);
  return bits;
}
function pluginOpBits(o){
  if(!o)return[];
  if(o.type==='set_bit'){
    const bit=parseInt(o.bit,10);
    return !isNaN(bit)&&bit>=0&&bit<64?[bit]:[];
  }
  if(o.type==='set_byte')return pluginByteBits(o.byte,o.mask===undefined?255:o.mask);
  if(o.type==='or_byte')return pluginByteBits(o.byte,o.val||0);
  if(o.type==='and_byte')return pluginByteBits(o.byte,(~(o.val===undefined?255:o.val))&255);
  if(o.type==='checksum')return pluginByteBits(7,255);
  return[];
}
function pluginMuxesOverlap(a,b){return a<0||b<0||a===b;}
function pluginFormatBits(bits){
  bits=Array.from(new Set(bits)).sort((a,b)=>a-b);
  if(bits.length>6)return bits.slice(0,6).join(', ')+', +'+(bits.length-6)+' more';
  return bits.join(', ');
}
function pluginConflictGroups(conflicts){
  const grouped={};
  (conflicts||[]).forEach(c=>{
    const key=c.winner+'|'+c.winnerPriority;
    if(!grouped[key])grouped[key]={winner:c.winner,winnerPriority:c.winnerPriority,bits:[]};
    grouped[key].bits.push(c.bit);
  });
  return Object.keys(grouped).map(k=>grouped[k]);
}
function pluginAnalyzePriority(list){
  const owners=[];
  (list||[]).forEach((p,i)=>{
    p.priority=i+1;p.hasConflict=false;
    (p.details||[]).forEach(r=>{r.pluginConflict=false;r.conflicts=[];});
  });
  (list||[]).forEach((p,i)=>{
    if(!p.enabled)return;
    (p.details||[]).forEach(r=>{
      if(r.send===false)return;
      const conflicts=[];
      (r.ops||[]).forEach(o=>{
        pluginOpBits(o).forEach(bit=>{
          const owner=owners.find(x=>x.id===r.id&&pluginMuxesOverlap(x.mux,r.mux)&&x.bit===bit);
          if(owner){
            if(owner.plugin!==p.name)conflicts.push({bit:bit,winner:owner.plugin,winnerPriority:owner.priority});
          }else owners.push({id:r.id,mux:r.mux,bit:bit,plugin:p.name,priority:i+1});
        });
      });
      if(conflicts.length){r.pluginConflict=true;r.conflicts=conflicts;p.hasConflict=true;}
    });
  });
}
function renderPluginConflictPanel(list){
  const el=$('plg-conflicts');
  if(!el)return;
  if(!list.length){el.style.display='none';el.innerHTML='';return;}
  const first=(list||[]).find(p=>p.enabled);
  const rows=[];
  (list||[]).forEach(p=>(p.details||[]).forEach(r=>{
    if(r.pluginConflict)pluginConflictGroups(r.conflicts).forEach(g=>{
      rows.push('<div>CAN '+r.hex+(r.mux>=0?' mux '+r.mux:' any mux')+': '+p.name+' ignores bit '+pluginFormatBits(g.bits)+'; '+g.winner+' (#'+g.winnerPriority+') wins</div>');
    });
  }));
  let h='<div style="padding:8px;background:var(--bg2);border:1px solid var(--bd);border-radius:6px;font-size:11px;color:var(--tx3);line-height:1.5">';
  h+='<div style="color:var(--tx2);font-weight:600;margin-bottom:3px">Injection priority</div>';
  h+=first?('#'+first.priority+' '+first.name+' is the first enabled plugin; one merged frame is injected after plugin bits are resolved.'):
    'No enabled plugins are injecting frames.';
  if(rows.length){
    h+='<div style="margin-top:6px;color:var(--warn)">'+rows.slice(0,5).join('')+(rows.length>5?'<div>+'+(rows.length-5)+' more conflicts</div>':'')+'</div>';
  }
  el.innerHTML=h+'</div>';
  el.style.display='block';
}
function pluginPrioritySelect(idx,total){
  if(total<2)return'';
  let h='<select class="sniff-input" title="Set injection priority" onchange="setPluginPriority('+idx+',this.value)" style="flex:0 0 58px;width:58px;margin-left:8px;padding:4px;font-size:10px">';
  for(let i=1;i<=total;i++)h+='<option value="'+i+'" '+(i===idx+1?'selected':'')+'>#'+i+'</option>';
  return h+'</select>';
}
function fmtOp(o){
  if(o.type==='set_bit') return 'set_bit('+o.bit+', '+(o.val?'true':'false')+')';
  if(o.type==='checksum') return 'checksum(byte 7)';
  if(o.type==='set_byte') return 'set_byte('+o.byte+', 0x'+o.val.toString(16)+', mask=0x'+o.mask.toString(16)+')';
  if(o.type==='or_byte') return 'or_byte('+o.byte+', 0x'+o.val.toString(16)+')';
  if(o.type==='and_byte') return 'and_byte('+o.byte+', 0x'+o.val.toString(16)+')';
  return o.type;
}
function renderPluginDetails(details){
  return '<div style="margin-top:6px;padding:8px;background:var(--bg2);border-radius:6px;font-size:11px;font-family:monospace">'
    +details.map(r=>{
      let hdr='<div style="margin-bottom:4px"><b>CAN '+r.hex+' ('+r.id+')</b>';
      if(r.mux>=0) hdr+=' <span style="color:var(--acc)">mux='+r.mux+'</span>';
      if(r.pluginConflict) hdr+=' <span style="color:var(--warn);font-weight:bold" title="Lower priority bits are ignored">&#9888; Priority overlap</span>';
      hdr+='</div>';
      let ops=r.ops.map(o=>'<div style="padding-left:12px;color:var(--tx2)">'+fmtOp(o)+'</div>').join('');
      let notes='';
      if(r.pluginConflict){
        notes='<div style="margin-top:4px;padding-left:12px;color:var(--warn)">'+pluginConflictGroups(r.conflicts).map(g=>'bit '+pluginFormatBits(g.bits)+' ignored; '+g.winner+' (#'+g.winnerPriority+') wins').join('<br>')+'</div>';
      }
      return hdr+ops+notes;
    }).join('<div style="border-top:1px solid var(--bd);margin:4px 0"></div>')
    +'</div>';
}
function toggleDetails(idx){
  var p=installedPlugins[idx];
  if(!p||!p.name)return;
  pluginDetailOpen[p.name]=!pluginDetailOpen[p.name];
  var el=$('plg-det-'+idx);
  if(el)el.style.display=pluginDetailOpen[p.name]?'block':'none';
}
function toggleInfo(id){
  var el=$(id);
  if(el)el.style.display=el.style.display==='none'?'block':'none';
}

function openHelpParent(btn){
  var sec=btn.closest('.subsec');
  if(sec&&sec.classList.contains('collapsed')){
    sec.classList.remove('collapsed');
    if(sec.dataset.collapseKey)localStorage.setItem(sec.dataset.collapseKey,'0');
    var secBtn=sec.querySelector('.subsec-btn');
    if(secBtn)secBtn.textContent='Hide';
  }
  var card=btn.closest('.card');
  if(card&&card.classList.contains('collapsed')){
    card.classList.remove('collapsed');
    if(card.dataset.collapseKey)localStorage.setItem(card.dataset.collapseKey,'0');
    var cardBtn=card.querySelector('.card-min-btn');
    if(cardBtn)cardBtn.textContent='Hide';
  }
}

function closeHelpPanels(scope,exceptId){
  (scope||document).querySelectorAll('.inline-help-panel').forEach(function(panel){
    if(!exceptId||panel.id!==exceptId)panel.classList.remove('show');
  });
}

function toggleHelp(btn,ev){
  if(ev){
    ev.preventDefault();
    ev.stopPropagation();
  }

  openHelpParent(btn);

  var targetId=btn.getAttribute('data-help-target');
  var panelId=btn.getAttribute('data-inline-help-id');
  var panel=panelId?document.getElementById(panelId):null;

  if(!panel){
    panel=document.createElement('div');
    panel.className='inline-help-panel';
    panelId='inline-help-'+Math.random().toString(36).slice(2,10);
    panel.id=panelId;
    btn.setAttribute('data-inline-help-id',panelId);

    var anchor=btn.closest('.subsec-head, .card-hdr, .setting-name, summary');
    if(anchor&&anchor.parentNode){
      anchor.insertAdjacentElement('afterend',panel);
    }else if(btn.parentNode){
      btn.parentNode.insertAdjacentElement('afterend',panel);
    }
  }

  if(targetId){
    var target=document.getElementById(targetId);
    panel.innerHTML=target?target.innerHTML:'More information is not available yet.';
  }else{
    panel.textContent=btn.getAttribute('data-help')||btn.getAttribute('title')||'More information is not available yet.';
  }

  var scope=btn.closest('.subsec, .card')||document;
  var willShow=!panel.classList.contains('show');
  closeHelpPanels(scope,panelId);
  panel.classList.toggle('show',willShow);
  return false;
}

function pluginStateSignature(list){
  return JSON.stringify((list||[]).map(p=>[p&&p.name||'',p&&p.version||'',!!(p&&p.enabled),p&&p.rules||0,p&&p.author||'']));
}

function renderPluginsState(d){
  installedPlugins=d.plugins||[];
  pluginAnalyzePriority(installedPlugins);
  const nextOpen={};
  installedPlugins.forEach(p=>{if(p&&p.name&&pluginDetailOpen[p.name])nextOpen[p.name]=true;});
  pluginDetailOpen=nextOpen;
  pluginMax=d.maxPlugins||pluginMax||0;
  const max=pluginMax;
  $('plg-count').textContent=max?installedPlugins.length+' / '+max+' installed':installedPlugins.length+' installed';
  if($('plg-limit')){
    const full=max&&installedPlugins.length>=max;
    $('plg-limit').textContent=max?(full?'Maximum '+max+' plugins reached. Remove one before installing another.':'Maximum '+max+' plugins total. Remove one before installing another.'):'Maximum plugins: --';
    $('plg-limit').style.color=full?'var(--err)':'var(--tx3)';
  }
  const el=$('plg-list');
  if(!installedPlugins.length){
    renderPluginConflictPanel(installedPlugins);
    el.innerHTML='<div style="font-size:12px;color:var(--tx3);text-align:center;padding:12px">No plugins installed</div>';
    return;
  }
  renderPluginConflictPanel(installedPlugins);
  el.innerHTML=installedPlugins.map((p,i)=>{
    let detailsOpen=!!pluginDetailOpen[p.name];
    let row='<div style="margin-bottom:8px;padding-bottom:8px;border-bottom:1px solid var(--bd)">';
    row+='<div class="setting-row"><div class="setting-info" style="cursor:pointer" onclick="toggleDetails('+i+')">';
    row+='<div class="setting-name">'+p.name+' <span style="color:var(--tx3);font-size:11px">v'+p.version+'</span>';
    row+='</div>';
    row+='<div class="setting-desc">Priority #'+(i+1)+(i===0?' first':'')+' &bull; '+p.rules+' rule'+(p.rules!==1?'s':'')+(p.author?' &bull; '+p.author:'')+(p.hasConflict?' &bull; <span style="color:var(--warn)">overlap ignored</span>':'')+' &bull; <span style="color:var(--acc);cursor:pointer">details</span></div>';
    row+='</div>';
    row+=pluginPrioritySelect(i,installedPlugins.length);
    row+='<label class="tgl"><input type="checkbox" '+(p.enabled?'checked':'')+' onchange="togglePlugin('+i+')"><div class="tgl-track"><div class="tgl-thumb"></div></div></label>';
    row+='<button onclick="peLoadInstalledPlugin('+i+')" style="margin-left:8px;padding:4px 8px;border:1px solid var(--bd);border-radius:5px;background:transparent;color:var(--acc);cursor:pointer;font-size:10px;font-family:inherit">Edit</button>';
    row+='<button onclick="removePlugin('+i+')" style="margin-left:8px;padding:4px 8px;border:1px solid var(--errBd);border-radius:5px;background:transparent;color:var(--err);cursor:pointer;font-size:10px;font-family:inherit">X</button></div>';
    if(p.details){
      row+='<div id="plg-det-'+i+'" style="display:'+(detailsOpen?'block':'none')+'">';
      row+=renderPluginDetails(p.details);
      row+='</div>';
    }
    row+='</div>';
    return row;
  }).join('');
}

async function refreshPluginsNow(){
  const d=await fetchJsonWithTimeout('/plugins',null,2500);
  renderPluginsState(d);
  return d;
}

async function refreshPluginsAfterAction(beforeSig){
  for(let i=0;i<4;i++){
    if(i)await waitMs(250);
    try{
      await refreshPluginsNow();
      if(pluginStateSignature(installedPlugins)!==beforeSig)return true;
    }catch(e){}
  }
  return false;
}

async function pollPlugins(){
  return runPoll('plugins',async()=>{
    if(!dashboardStatusOk)return;
    try{renderPluginsState(await fetchPollJson('/plugins',2000));}catch(e){}
  });
}

// ── Firmware update ──
var pendingUpdateUrl='';
async function toggleBeta(){
  const beta=$('beta-tgl').checked?'1':'0';
  try{await fetch('/update_beta',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'beta='+beta});}catch(e){}
  $('upd-info').style.display='none';$('upd-status').textContent='';
}
async function checkUpdate(){
  $('upd-check-btn').disabled=true;$('upd-status').textContent='Checking...';$('upd-status').style.color='var(--acc)';
  $('upd-info').style.display='none';pendingUpdateUrl='';
  try{const r=await fetch('/update_check');const d=await r.json();
    if(!d.ok){$('upd-status').textContent=d.error||'Error';$('upd-status').style.color='var(--err)';$('upd-check-btn').disabled=false;return;}
    $('fw-ver').textContent='v'+d.current;
    if(d.update){
      $('upd-status').textContent='Update available!';$('upd-status').style.color='var(--ok)';
      $('upd-ver').textContent='v'+d.latest+(d.prerelease?' (beta)':'');
      $('upd-detail').textContent=d.artifact+' \u2022 '+d.tag;
      pendingUpdateUrl=d.url;
      $('upd-info').style.display='block';
    }else{
      $('upd-status').textContent='Up to date (v'+d.current+')';$('upd-status').style.color='var(--ok)';
    }
  }catch(e){$('upd-status').textContent='Connection error';$('upd-status').style.color='var(--err)';}
  $('upd-check-btn').disabled=false;
}
async function installUpdate(){
  if(!pendingUpdateUrl){$('upd-status').textContent='No update URL';return;}
  if(!await dashConfirm('Install firmware update? The device will reboot.','Install update','Install'))return;
  $('upd-install-btn').disabled=true;$('upd-status').textContent='Downloading & installing...';$('upd-status').style.color='var(--acc)';
  try{await fetch('/update_install',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'url='+encodeURIComponent(pendingUpdateUrl)});
    $('upd-status').textContent='Update installed! Rebooting...';$('upd-status').style.color='var(--ok)';
    setTimeout(()=>location.reload(),15000);
  }catch(e){$('upd-status').textContent='Update failed';$('upd-status').style.color='var(--err)';$('upd-install-btn').disabled=false;}
}
async function loadUpdateInfo(){
  try{const r=await fetch('/update_beta',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'noop=1'});
    const d=await r.json();
    if(d.version){$('fw-ver').textContent='v'+d.version;updateFoot(d.version);}
    $('beta-tgl').checked=!!d.beta;
  }catch(e){}
  try{const r=await fetch('/auto_update');const d=await r.json();$('auto-upd-tgl').checked=!!d.enabled;}catch(e){}
}
async function toggleAutoUpdate(){
  const en=$('auto-upd-tgl').checked?'1':'0';
  try{await fetch('/auto_update',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'enabled='+en});}catch(e){}
}
function updateFoot(ver){
  var ip=location.hostname||'192.168.4.1';
  $('dash-foot').textContent='ev-open-can-tools \u2022 v'+ver+' \u2022 '+ip;
}

async function loadCanPins(){
  try{const r=await fetch('/can_pins');const d=await r.json();
    if(d.tx>=0)$('can-tx').value=d.tx;
    if(d.rx>=0)$('can-rx').value=d.rx;
    $('can-pins-status').textContent=d.customized?('custom TX='+d.tx+' RX='+d.rx):('firmware default TX='+d.tx+' RX='+d.rx);
  }catch(e){}
}
async function saveCanPins(){
  var tx=parseInt($('can-tx').value,10),rx=parseInt($('can-rx').value,10);
  if(isNaN(tx)||isNaN(rx)){$('can-pins-hint').textContent='Enter both TX and RX';$('can-pins-hint').style.color='var(--err)';return;}
  if(!await dashConfirm('Save CAN pins TX='+tx+' RX='+rx+' and reboot? Wrong pins disable CAN.','Save CAN pins','Save'))return;
  try{const r=await fetch('/can_pins',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'tx='+tx+'&rx='+rx});
    const d=await r.json();
    if(d.ok){
      $('can-pins-hint').textContent='Saved. Rebooting...';$('can-pins-hint').style.color='var(--ok)';
      await fetch('/reboot',{method:'POST'});
      setTimeout(()=>location.reload(),8000);
    }else{
      $('can-pins-hint').textContent=d.error||'Save failed';$('can-pins-hint').style.color='var(--err)';
    }
  }catch(e){$('can-pins-hint').textContent='Connection error';$('can-pins-hint').style.color='var(--err)';}
}

async function exportSettings(){
  $('backup-status').textContent='Preparing...';$('backup-status').style.color='var(--tx3)';
  try{const r=await fetch('/settings_export');
    if(!r.ok){throw new Error('HTTP '+r.status);}
    const text=await r.text();
    const blob=new Blob([text],{type:'application/json'});
    const url=URL.createObjectURL(blob);
    const a=document.createElement('a');a.href=url;a.download='evtools-backup.json';document.body.appendChild(a);a.click();document.body.removeChild(a);URL.revokeObjectURL(url);
    $('backup-status').textContent='Downloaded';$('backup-status').style.color='var(--ok)';
  }catch(e){$('backup-status').textContent='Export failed';$('backup-status').style.color='var(--err)';}
}
async function importSettings(ev){
  const f=ev.target.files[0];if(!f)return;
  const text=await f.text();
  try{JSON.parse(text);}catch(e){$('backup-status').textContent='Invalid JSON';$('backup-status').style.color='var(--err)';return;}
  if(!await dashConfirm('Restore settings from '+f.name+' and reboot?','Restore settings','Restore'))return;
  $('backup-status').textContent='Uploading...';$('backup-status').style.color='var(--acc)';
  try{const r=await fetch('/settings_import',{method:'POST',headers:{'Content-Type':'application/json'},body:text});
    const d=await r.json();
    if(d.ok){
      $('backup-status').textContent='Restored. Rebooting...';$('backup-status').style.color='var(--ok)';
      await fetch('/reboot',{method:'POST'});
      setTimeout(()=>location.reload(),8000);
    }else{
      $('backup-status').textContent=d.error||'Import failed';$('backup-status').style.color='var(--err)';
    }
  }catch(e){$('backup-status').textContent='Upload failed';$('backup-status').style.color='var(--err)';}
  ev.target.value='';
}

// ── Plugin Editor ────────────────────────────────────────────────
let peState={rules:[]};
function peGetMeta(){return{name:($('pe-name').value||'').trim(),version:($('pe-version').value||'1.0').trim(),author:($('pe-author').value||'').trim()};}
function peParseInt(s,def){if(typeof s==='number')return s;if(s===''||s==null)return def;s=String(s).trim();let n=s.toLowerCase().startsWith('0x')?parseInt(s,16):parseInt(s,10);return isNaN(n)?def:n;}
function peSetStatus(msg,kind){const el=$('pe-status');el.textContent=msg;el.style.color=kind==='ok'?'var(--ok)':kind==='err'?'var(--err)':kind==='acc'?'var(--acc)':'var(--tx3)';}
function peSetTestStatus(msg,kind){const el=$('pe-test-status');el.textContent=msg;el.style.color=kind==='ok'?'var(--ok)':kind==='err'?'var(--err)':kind==='acc'?'var(--acc)':'var(--tx3)';}
function peHasContent(){const meta=peGetMeta();return !!(meta.name||meta.author||meta.version!=='1.0'||peState.rules.length);}
function peRuleLabel(r,i){return 'Rule '+(i+1)+' · CAN 0x'+toHex((r.id||0)&0x7FF,3)+(r.mux>=0?' · mux '+r.mux:'');}
function peParseShortcutLine(line){
  const raw=(line||'').trim();
  if(!raw)return{error:'Shortcut line required'};
  const m=raw.match(/^\s*(0x[0-9a-fA-F]+|\d+)(?:\s+mux\s*=\s*(-?\d+))?\s+byte\[(\d+)\]\s*=\s*(0x[0-9a-fA-F]+|\d+)(?:\s+mask\s*=\s*(0x[0-9a-fA-F]+|\d+))?(?:\s*\((.*)\))?\s*$/);
  if(!m)return{error:'Use format like 0x7FF mux=2 byte[5] = 0x4C'};
  const canId=peParseInt(m[1],NaN),mux=m[2]===undefined?-1:peParseInt(m[2],NaN);
  const byte=peParseInt(m[3],NaN),val=peParseInt(m[4],NaN),mask=m[5]===undefined?255:peParseInt(m[5],NaN);
  if(isNaN(canId)||canId<1||canId>0x7FF)return{error:'CAN ID must be 1-0x7FF'};
  if(isNaN(mux)||mux<-1||mux>7)return{error:'mux must be -1..7'};
  if(isNaN(byte)||byte<0||byte>7)return{error:'byte must be 0-7'};
  if(isNaN(val)||val<0||val>255)return{error:'value must be 0-255'};
  if(isNaN(mask)||mask<0||mask>255)return{error:'mask must be 0-255'};
  return{rule:{id:canId|0,mux:mux|0,send:true,ops:[{type:'set_byte',byte:byte|0,val:val|0,mask:mask|0}]},note:raw};
}
function peUpdateRuleOptions(){
  const sel=$('pe-test-rule');if(!sel)return;
  const prev=parseInt(sel.value,10);
  if(!peState.rules.length){sel.disabled=true;sel.innerHTML='<option value="">No rules</option>';return;}
  sel.disabled=false;
  sel.innerHTML=peState.rules.map((r,i)=>'<option value="'+i+'">'+peRuleLabel(r,i)+'</option>').join('');
  sel.value=String(!isNaN(prev)&&prev>=0&&prev<peState.rules.length?prev:0);
}
function peAddRule(){if(peState.rules.length>=16){peSetStatus('Max 16 rules per plugin','err');return;}peState.rules.push({id:0,mux:-1,send:true,ops:[]});peRender();}
function peAddRuleFromShortcut(){
  if(peState.rules.length>=16){peSetStatus('Max 16 rules per plugin','err');return;}
  const input=$('pe-shortcut');const parsed=peParseShortcutLine(input.value);
  if(parsed.error){peSetStatus(parsed.error,'err');return;}
  peState.rules.push(parsed.rule);
  input.value='';
  peRender();
  peSetStatus('Shortcut added','ok');
}
function peRemoveRule(i){peState.rules.splice(i,1);peRender();}
function peAddOp(i,type){const r=peState.rules[i];if(!r)return;if(r.ops.length>=8){peSetStatus('Max 8 ops per rule','err');return;}
  const op={type:type};
  if(type==='set_bit'){op.bit=0;op.val=1;}
  else if(type==='set_byte'){op.byte=0;op.val=0;op.mask=255;}
  else if(type==='or_byte'){op.byte=0;op.val=0;}
  else if(type==='and_byte'){op.byte=0;op.val=255;}
  r.ops.push(op);peRender();}
function peRemoveOp(i,j){peState.rules[i].ops.splice(j,1);peRender();}
function peUpdateField(i,j,field,value){
  if(j<0){const r=peState.rules[i];if(!r)return;
    if(field==='id')r.id=peParseInt(value,0);
    else if(field==='mux'){r.mux=value===''?-1:peParseInt(value,-1);}
    else if(field==='send')r.send=!!value;
    peRender();return;
  }
  const op=peState.rules[i].ops[j];if(!op)return;
  if(field==='type'){const nt=value;Object.keys(op).forEach(k=>{if(k!=='type')delete op[k];});op.type=nt;
    if(nt==='set_bit'){op.bit=0;op.val=1;}
    else if(nt==='set_byte'){op.byte=0;op.val=0;op.mask=255;}
    else if(nt==='or_byte'){op.byte=0;op.val=0;}
    else if(nt==='and_byte'){op.byte=0;op.val=255;}
    peRender();return;
  }
  if(field==='bit')op.bit=Math.max(0,Math.min(63,peParseInt(value,0)));
  else if(field==='byte')op.byte=Math.max(0,Math.min(7,peParseInt(value,0)));
  else if(field==='val')op.val=Math.max(0,Math.min(op.type==='set_bit'?1:255,peParseInt(value,0)));
  else if(field==='mask')op.mask=Math.max(0,Math.min(255,peParseInt(value,255)));
  peRenderPreview();peUpdateTestPreview();
}
function peOpRow(i,j,op){
  const sel='<select class="sniff-input" style="width:90px" onchange="peUpdateField('+i+','+j+',\'type\',this.value)">'+
    ['set_bit','set_byte','or_byte','and_byte','checksum'].map(t=>'<option value="'+t+'"'+(op.type===t?' selected':'')+'>'+t+'</option>').join('')+'</select>';
  let fields='';
  if(op.type==='set_bit'){
    fields='<input class="sniff-input" style="width:55px" type="number" min="0" max="63" value="'+op.bit+'" title="bit (0-63)" onchange="peUpdateField('+i+','+j+',\'bit\',this.value)">'+
      '<select class="sniff-input" style="width:80px" onchange="peUpdateField('+i+','+j+',\'val\',this.value)"><option value="1"'+(op.val?' selected':'')+'>set (1)</option><option value="0"'+(!op.val?' selected':'')+'>clear (0)</option></select>';
  }else if(op.type==='set_byte'){
    fields='<input class="sniff-input" style="width:48px" type="number" min="0" max="7" value="'+op.byte+'" title="byte (0-7)" onchange="peUpdateField('+i+','+j+',\'byte\',this.value)">'+
      '<input class="sniff-input" style="width:70px" value="0x'+((op.val||0)&255).toString(16)+'" title="val (0-255, hex or dec)" onchange="peUpdateField('+i+','+j+',\'val\',this.value)">'+
      '<input class="sniff-input" style="width:70px" value="0x'+(op.mask===undefined?255:op.mask).toString(16)+'" title="mask (0-255)" onchange="peUpdateField('+i+','+j+',\'mask\',this.value)">';
  }else if(op.type==='or_byte'||op.type==='and_byte'){
    fields='<input class="sniff-input" style="width:48px" type="number" min="0" max="7" value="'+op.byte+'" title="byte (0-7)" onchange="peUpdateField('+i+','+j+',\'byte\',this.value)">'+
      '<input class="sniff-input" style="width:70px" value="0x'+((op.val||0)&255).toString(16)+'" title="val (0-255)" onchange="peUpdateField('+i+','+j+',\'val\',this.value)">';
  }else{
    fields='<span style="font-size:11px;color:var(--tx3);align-self:center;padding:0 4px">recalc byte 7 checksum</span>';
  }
  return '<div style="display:flex;gap:4px;align-items:center;margin-bottom:4px;flex-wrap:wrap">'+sel+fields+'<button class="sniff-btn" style="margin-left:auto;padding:2px 8px" onclick="peRemoveOp('+i+','+j+')" title="Remove op">&times;</button></div>';
}
function peRuleBlock(i,r){
  const ops=r.ops.length?r.ops.map((op,j)=>peOpRow(i,j,op)).join(''):'<div style="font-size:11px;color:var(--tx3);padding:4px 0">No ops &mdash; add one below</div>';
  const hex=r.id?'0x'+r.id.toString(16).toUpperCase():'?';
  return '<details open style="margin-bottom:10px;border:1px solid var(--bd);border-radius:6px;padding:8px;background:var(--bg2)">'+
    '<summary style="cursor:pointer;font-size:12px;color:var(--tx);user-select:none">Rule '+(i+1)+' &mdash; CAN '+hex+(r.id?' ('+r.id+')':'')+(r.mux>=0?' mux='+r.mux:'')+' &middot; '+r.ops.length+' op'+(r.ops.length===1?'':'s')+'</summary>'+
    '<div style="display:flex;gap:6px;margin:8px 0;flex-wrap:wrap">'+
      '<input class="sniff-input" style="width:110px" value="'+(r.id?('0x'+(r.id|0).toString(16).toUpperCase()):'')+'" placeholder="CAN / 0x7FF" onchange="peUpdateField('+i+',-1,\'id\',this.value)">'+
      '<input class="sniff-input" style="width:100px" type="number" min="-1" max="7" value="'+r.mux+'" placeholder="mux (-1=any)" onchange="peUpdateField('+i+',-1,\'mux\',this.value)">'+
      '<label style="font-size:11px;color:var(--tx3);display:flex;align-items:center;gap:4px"><input type="checkbox"'+(r.send?' checked':'')+' onchange="peUpdateField('+i+',-1,\'send\',this.checked)"> send</label>'+
      '<button class="sniff-btn" style="margin-left:auto" onclick="peRemoveRule('+i+')">Remove Rule</button>'+
    '</div>'+
    ops+
    '<div style="margin-top:6px;display:flex;gap:4px;flex-wrap:wrap">'+
      '<button class="sniff-btn" onclick="peAddOp('+i+',\'set_bit\')">+ set_bit</button>'+
      '<button class="sniff-btn" onclick="peAddOp('+i+',\'set_byte\')">+ set_byte</button>'+
      '<button class="sniff-btn" onclick="peAddOp('+i+',\'or_byte\')">+ or_byte</button>'+
      '<button class="sniff-btn" onclick="peAddOp('+i+',\'and_byte\')">+ and_byte</button>'+
      '<button class="sniff-btn" onclick="peAddOp('+i+',\'checksum\')">+ checksum</button>'+
    '</div>'+
  '</details>';
}
function peRender(){
  const el=$('pe-rules');
  if(!peState.rules.length){el.innerHTML='<div style="font-size:12px;color:var(--tx3);text-align:center;padding:12px;border:1px dashed var(--bd);border-radius:6px">No rules yet. Click &ldquo;+ Add Rule&rdquo; below.</div>';}
  else{el.innerHTML=peState.rules.map((r,i)=>peRuleBlock(i,r)).join('');}
  $('pe-count').textContent=peState.rules.length+' rule'+(peState.rules.length===1?'':'s');
  peUpdateRuleOptions();
  peRenderPreview();
  peUpdateTestPreview();
}
function peBuildObj(){
  const meta=peGetMeta();
  const obj={name:meta.name||'Untitled',version:meta.version||'1.0'};
  if(meta.author)obj.author=meta.author;
  obj.rules=peState.rules.map(r=>{
    const out={id:r.id|0};
    if(r.mux>=0)out.mux=r.mux|0;
    if(r.send===false)out.send=false;
    out.ops=r.ops.map(op=>{
      const o={type:op.type};
      if(op.type==='set_bit'){o.bit=op.bit|0;o.val=op.val?1:0;}
      else if(op.type==='set_byte'){o.byte=op.byte|0;o.val=(op.val|0)&255;if(op.mask!==undefined&&op.mask!==255)o.mask=op.mask|0;}
      else if(op.type==='or_byte'||op.type==='and_byte'){o.byte=op.byte|0;o.val=(op.val|0)&255;}
      return o;
    });
    return out;
  });
  return obj;
}
function peRenderPreview(){$('pe-preview').textContent=JSON.stringify(peBuildObj(),null,2);}
function peFormatBytes(bytes){return(bytes||[]).slice(0,8).map(b=>toHex((b||0)&255,2)).join(' ');}
function peUpdateTestPreview(){
  const el=$('pe-test-preview');if(!el)return;
  if(!peState.rules.length){el.textContent='Add a rule to preview a test frame.';peSetTestStatus('Idle','');return;}
  const idx=parseInt($('pe-test-rule').value,10);
  if(isNaN(idx)||idx<0||idx>=peState.rules.length){el.textContent='Select a rule to test.';return;}
  const count=parseInt($('pe-test-count').value,10),interval=parseInt($('pe-test-interval').value,10);
  if(isNaN(count)||count<1||count>200){el.textContent='Count must be 1-200.';return;}
  if(isNaN(interval)||interval<10||interval>5000){el.textContent='Interval must be 10-5000 ms.';return;}
  const rule=peState.rules[idx];
  el.textContent='Preview '+peRuleLabel(rule,idx)+'\nWait for next matching bus frame, then apply this rule.\nInject '+count+'x every '+interval+' ms';
}
function peStopTestPoll(){if(peTestPollTimer){clearInterval(peTestPollTimer);peTestPollTimer=null;}}
async function pePollTestStatus(){
  try{
    const r=await fetch('/plugin_test_status');const d=await r.json();
    if(d.waiting){
      $('pe-test-preview').textContent='Waiting for CAN 0x'+toHex((d.targetId||d.id||0)&0x7FF,3)+'\nThe next matching bus frame will be modified and injected.\nProgress: 0/'+(d.total||0);
    }else if(d.id||d.data){
      $('pe-test-preview').textContent='Test CAN 0x'+toHex((d.id||0)&0x7FF,3)+'\nFrame: '+peFormatBytes(d.data||[])+(d.total?'\nProgress: '+d.sent+'/'+d.total:'');
    }
    if(d.active){
      peSetTestStatus(d.waiting?('Waiting for CAN 0x'+toHex((d.targetId||d.id||0)&0x7FF,3)):('Running '+d.sent+'/'+d.total+' · every '+d.interval+' ms'),'acc');
    }else{
      peSetTestStatus(d.total?(d.sent<d.total?'Stopped '+d.sent+'/'+d.total:'Done '+d.sent+'/'+d.total):'Idle',d.total&&d.sent>=d.total?'ok':'');
      peStopTestPoll();
    }
  }catch(e){}
}
async function peLoadInstalledPlugin(idx){
  const p=installedPlugins[idx];if(!p)return;
  if(peHasContent()&&!await dashConfirm('Load installed plugin into the editor? Current editor contents will be replaced.','Load plugin','Load'))return;
  $('pe-name').value=p.name||'';$('pe-author').value=p.author||'';$('pe-version').value=p.version||'1.0';
  peState={rules:(p.details||[]).map(r=>({id:r.id|0,mux:typeof r.mux==='number'?r.mux:-1,send:r.send!==false,ops:(r.ops||[]).map(op=>{const out={type:op.type};if(op.type==='set_bit'){out.bit=op.bit|0;out.val=op.val?1:0;}else if(op.type==='set_byte'){out.byte=op.byte|0;out.val=(op.val|0)&255;out.mask=((typeof op.mask==='number'?op.mask:255)|0)&255;}else if(op.type==='or_byte'||op.type==='and_byte'){out.byte=op.byte|0;out.val=(op.val|0)&255;}return out;})}))};
  peLoadedPluginName=p.name||'';peStopTestPoll();peSetTestStatus('Idle','');peRender();peSetStatus('Loaded "'+p.name+'" into editor','ok');
  $('pe-name').scrollIntoView({behavior:'smooth',block:'center'});
}
function peValidate(){
  const meta=peGetMeta();
  if(!meta.name)return 'Plugin name required';
  if(meta.name.length>31)return 'Name too long (max 31)';
  if(!peState.rules.length)return 'Add at least one rule';
  for(let i=0;i<peState.rules.length;i++){
    const r=peState.rules[i];
    if(!r.id||r.id<1||r.id>2047)return 'Rule '+(i+1)+': CAN ID must be 1-2047';
    if(r.mux<-1||r.mux>7)return 'Rule '+(i+1)+': mux must be -1..7';
    if(!r.ops.length)return 'Rule '+(i+1)+': add at least one op';
    for(let j=0;j<r.ops.length;j++){
      const op=r.ops[j];
      if(op.type==='set_bit'){if(op.bit<0||op.bit>63)return 'Rule '+(i+1)+' op '+(j+1)+': bit must be 0-63';}
      else if(op.type==='set_byte'||op.type==='or_byte'||op.type==='and_byte'){
        if(op.byte<0||op.byte>7)return 'Rule '+(i+1)+' op '+(j+1)+': byte must be 0-7';
        if(op.val<0||op.val>255)return 'Rule '+(i+1)+' op '+(j+1)+': val must be 0-255';
      }
    }
  }
  return null;
}
async function peInstall(){
  const err=peValidate();
  if(err){peSetStatus(err,'err');return;}
  const obj=peBuildObj();
  try{const r=await fetch('/plugins');const d=await r.json();
    if(d.plugins&&d.plugins.some(p=>p.name===obj.name)&&obj.name!==peLoadedPluginName){
      if(!await dashConfirm('A plugin named "'+obj.name+'" already exists. Overwrite?','Overwrite plugin','Overwrite'))return;
    }
  }catch(e){}
  const beforeSig=pluginStateSignature(installedPlugins);
  peSetStatus('Installing...','acc');
  try{
    await fetchJsonWithTimeout('/plugin_upload',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(obj)},5000);
    peLoadedPluginName=obj.name;
    try{await refreshPluginsNow();}catch(e){await refreshPluginsAfterAction(beforeSig);}
    peSetStatus('Installed','ok');
  }catch(e){
    if(await refreshPluginsAfterAction(beforeSig)){
      peLoadedPluginName=obj.name;
      peSetStatus('Installed','ok');
    }else{peSetStatus(actionErrorMessage(e,'Connection error'),'err');}
  }
}
async function peStartTest(){
  if(!peState.rules.length){peSetTestStatus('Add a rule first','err');return;}
  const idx=parseInt($('pe-test-rule').value,10);
  if(isNaN(idx)||idx<0||idx>=peState.rules.length){peSetTestStatus('Select a valid rule','err');return;}
  const count=parseInt($('pe-test-count').value,10),interval=parseInt($('pe-test-interval').value,10);
  if(isNaN(count)||count<1||count>200){peSetTestStatus('Count must be 1-200','err');return;}
  if(isNaN(interval)||interval<10||interval>5000){peSetTestStatus('Interval must be 10-5000 ms','err');return;}
  peSetTestStatus('Starting...','acc');
  try{
    const r=await fetch('/plugin_test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({plugin:peBuildObj(),rule:idx,count:count,interval:interval})});
    const d=await r.json();
    if(d.ok){
      $('pe-test-preview').textContent='Waiting for CAN 0x'+toHex((d.targetId||d.id||0)&0x7FF,3)+'\nThe next matching bus frame will be modified and injected.\nProgress: 0/'+(d.total||0);
      peSetTestStatus(d.active?('Waiting for CAN 0x'+toHex((d.targetId||d.id||0)&0x7FF,3)):'Done','acc');
      peStopTestPoll();peTestPollTimer=setInterval(pePollTestStatus,500);pePollTestStatus();
    }else{
      peSetTestStatus(d.error||'Test failed','err');
    }
  }catch(e){peSetTestStatus('Connection error','err');}
}
async function peStopTest(){
  try{
    const r=await fetch('/plugin_test_stop',{method:'POST'});const d=await r.json();
    peStopTestPoll();
    peSetTestStatus(d.total?(d.sent<d.total?'Stopped '+d.sent+'/'+d.total:'Done '+d.sent+'/'+d.total):'Idle',d.total&&d.sent>=d.total?'ok':'');
  }catch(e){peSetTestStatus('Connection error','err');}
}
function peDownload(){
  const err=peValidate();
  if(err){peSetStatus(err,'err');return;}
  const obj=peBuildObj();
  const blob=new Blob([JSON.stringify(obj,null,2)],{type:'application/json'});
  const url=URL.createObjectURL(blob);
  const a=document.createElement('a');a.href=url;a.download=(obj.name||'plugin').replace(/[^A-Za-z0-9_-]/g,'_').toLowerCase()+'.json';document.body.appendChild(a);a.click();document.body.removeChild(a);URL.revokeObjectURL(url);
  peSetStatus('Downloaded','ok');
}
async function peReset(){
  if(peState.rules.length&&!await dashConfirm('Discard current editor contents?','Discard changes','Discard'))return;
  peState={rules:[]};peLoadedPluginName='';peStopTestPoll();
  $('pe-name').value='';$('pe-author').value='';$('pe-version').value='1.0';
  peRender();peSetStatus('','');peSetTestStatus('Idle','');
}

dashboardPollTimers.push(setInterval(poll,2000));dashboardPollTimers.push(setInterval(pollLog,5000));dashboardPollTimers.push(setInterval(pollSniffer,1000));dashboardPollTimers.push(setInterval(pollPlugins,10000));dashboardPollTimers.push(setInterval(loadWifiStatus,10000));dashboardPollTimers.push(setInterval(loadApStatus,10000));
initCardMinimizers();initSubsectionMinimizers();updateHW4(1);updateProfileControls(1,0);updateSniffIdToggle();poll();pollRec();peRender();
</script>
</body>
</html>
)HTML";
