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
  --bg:#0d0d0d;--card:#161616;--card2:#1e1e1e;
  --bd:#2a2a2a;--bd2:#333;
  --tx:#f0f0f0;--tx2:#999;--tx3:#555;
  --acc:#5b8fff;--accBg:rgba(91,143,255,.1);--accBd:rgba(91,143,255,.25);
  --ok:#3dba72;--okBg:rgba(61,186,114,.1);
  --err:#ff4f4f;--errBg:rgba(255,79,79,.08);--errBd:rgba(255,79,79,.2);
  --warn:#f5a623;
}
[data-theme="light"]{
  --bg:#f5f5f5;--card:#fff;--card2:#f0f0f0;
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
.card{background:var(--card);border:1px solid var(--bd);border-radius:12px;padding:16px;margin:0 16px 12px}
.card-hdr{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px}
.card-title{font-size:13px;font-weight:600;color:var(--tx);text-transform:uppercase;letter-spacing:.5px}
.card-meta{font-size:11px;color:var(--tx3)}

/* HW seg */
.hw-seg{display:flex;background:var(--card2);border:1px solid var(--bd);border-radius:9px;padding:3px;gap:2px}
.hw-btn{flex:1;padding:8px;border:none;border-radius:7px;font-size:12px;font-weight:600;
  cursor:pointer;background:transparent;color:var(--tx2);transition:all .18s;font-family:inherit}
.hw-btn.active{background:var(--card);color:var(--acc);border:1px solid var(--accBd);
  box-shadow:0 1px 4px rgba(0,0,0,.15)}
.hw-btn:hover:not(.active){background:var(--bd);color:var(--tx)}

/* Speed pills */
.pills{display:flex;gap:6px;flex-wrap:wrap}
.pill{flex:1;min-width:60px;padding:9px 8px;border:1px solid var(--bd);border-radius:9px;
  font-size:12px;font-weight:600;cursor:pointer;background:var(--bg);color:var(--tx2);
  transition:all .18s;text-align:center;font-family:inherit}
.pill.active{background:var(--accBg);border-color:var(--accBd);color:var(--acc)}
.pill:hover:not(.active){border-color:var(--bd2);color:var(--tx)}

/* Feature rows */
.feat-row{display:flex;align-items:center;justify-content:space-between;
  padding:12px 0;border-bottom:1px solid var(--bd)}
.feat-row:last-of-type{border-bottom:none;padding-bottom:0}
.feat-row:first-of-type{padding-top:0}
.feat-info{flex:1;min-width:0}
.feat-name{font-size:13px;font-weight:500;color:var(--tx)}
.feat-desc{font-size:11px;color:var(--tx3);margin-top:2px}
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
.sniff-row{display:grid;grid-template-columns:38px 56px 1fr;gap:8px;
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

/* Buttons */
.btn-row{display:flex;gap:8px;margin-top:14px}
.btn{flex:1;padding:10px;border:1px solid;border-radius:9px;background:transparent;
  font-family:inherit;font-size:12px;font-weight:600;cursor:pointer;transition:all .18s;letter-spacing:.3px}
.btn-stop{border-color:var(--errBd);color:var(--err)}
.btn-stop:hover{background:var(--errBg)}
.btn-reboot{border-color:var(--bd2);color:var(--tx2)}
.btn-reboot:hover{border-color:var(--acc);color:var(--acc)}

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
  <div class="stat"><div class="stat-lbl">AD</div><div class="stat-val" id="s-AD">Inactive</div></div>
  <div class="stat"><div class="stat-lbl">Frame rate</div><div class="stat-val v-dim" id="s-fps">0.0 Hz</div></div>
  <div class="stat"><div class="stat-lbl">RX</div><div class="stat-val v-acc" id="s-rx">0</div></div>
  <div class="stat"><div class="stat-lbl">TX</div><div class="stat-val v-acc" id="s-tx">0</div></div>
  <div class="stat"><div class="stat-lbl">TX Errors</div><div class="stat-val v-dim" id="s-txerr">0</div></div>
  <div class="stat"><div class="stat-lbl">Follow dist</div><div class="stat-val v-dim" id="s-fd">—</div></div>
  <div class="stat"><div class="stat-lbl">Profile</div><div class="stat-val v-dim" id="s-prof">—</div></div>
  <div class="stat"><div class="stat-lbl">Speed Offset</div><div class="stat-val v-dim" id="s-soff">0</div></div>
  <div class="stat"><div class="stat-lbl">Uptime</div><div class="stat-val v-dim" id="s-up">0s</div></div>
</div>

<div style="height:12px"></div>

<div class="card">
  <div class="card-hdr"><div class="card-title">Hardware</div><div class="card-meta">Autopilot generation</div></div>
  <div class="hw-seg" id="hw-seg">
    <button class="hw-btn" data-v="0" onclick="setHW(0)">Legacy</button>
    <button class="hw-btn active" data-v="1" onclick="setHW(1)">HW3</button>
    <button class="hw-btn" data-v="2" onclick="setHW(2)">HW4</button>
  </div>
</div>

<div class="card">
  <div class="card-hdr"><div class="card-title">Speed Profile</div><div class="card-meta">AD aggressiveness &bull; Auto follows stalk</div></div>
  <div class="pills" id="sp-pills"></div>
</div>

<div class="card">
  <div class="card-hdr"><div class="card-title">Features</div></div>

  <div class="feat-row">
    <div class="feat-info">
      <div class="feat-name">AD Activation</div>
      <div class="feat-desc">Requires active AD subscription</div>
    </div>
    <label class="tgl"><input type="checkbox" id="tgl-AD" checked onchange="pushFeat()">
      <div class="tgl-track"><div class="tgl-thumb"></div></div></label>
  </div>

  <div class="feat-row">
    <div class="feat-info">
      <div class="feat-name">Nag Suppression</div>
      <div class="feat-desc">Remove hands-on-wheel warning (ECE R79)</div>
    </div>
    <label class="tgl"><input type="checkbox" id="tgl-nag" checked onchange="pushFeat()">
      <div class="tgl-track"><div class="tgl-thumb"></div></div></label>
  </div>

  <div class="feat-row">
    <div class="feat-info">
      <div class="feat-name">Summon EU Unlock</div>
      <div class="feat-desc">Remove Smart Summon distance restriction</div>
    </div>
    <label class="tgl"><input type="checkbox" id="tgl-summon" checked onchange="pushFeat()">
      <div class="tgl-track"><div class="tgl-thumb"></div></div></label>
  </div>

  <div class="feat-row hw4-only" id="row-isa">
    <div class="feat-info">
      <div class="feat-name">ISA Chime Suppress</div>
      <div class="feat-desc">Disable speed limit warning chime</div>
    </div>
    <label class="tgl"><input type="checkbox" id="tgl-isa" onchange="pushFeat()">
      <div class="tgl-track"><div class="tgl-thumb"></div></div></label>
  </div>

  <div class="feat-row hw4-only" id="row-evd">
    <div class="feat-info">
      <div class="feat-name">Emergency Vehicle Detection</div>
      <div class="feat-desc">Enable approaching EV detection</div>
    </div>
    <label class="tgl"><input type="checkbox" id="tgl-evd" onchange="pushFeat()">
      <div class="tgl-track"><div class="tgl-thumb"></div></div></label>
  </div>

  <div class="feat-row">
    <div class="feat-info">
      <div class="feat-name">Nag Killer</div>
      <div class="feat-desc">Echo CAN 880 counter+1 to suppress hands-on-wheel nag</div>
    </div>
    <label class="tgl"><input type="checkbox" id="tgl-nk" onchange="pushFeat()">
      <div class="tgl-track"><div class="tgl-thumb"></div></div></label>
  </div>

  <div class="hw4-only" style="padding:12px 0;border-bottom:1px solid var(--bd)">
    <div class="feat-info" style="margin-bottom:8px">
      <div class="feat-name">Speed Offset</div>
      <div class="feat-desc">Static offset injected on mux 2</div>
    </div>
    <div id="h4o-pills" style="display:flex;flex-wrap:wrap;gap:6px"></div>
  </div>

  <div class="feat-row">
    <div class="feat-info">
      <div class="feat-name">Bypass TLSSC</div>
      <div class="feat-desc">Bypass TLSSC requirement — enables AD without steering wheel check (persists reboot)</div>
    </div>
    <label class="tgl"><input type="checkbox" id="tgl-fAD" onchange="pushFeat()">
      <div class="tgl-track"><div class="tgl-thumb"></div></div></label>
  </div>

  <div class="feat-row">
    <div class="feat-info">
      <div class="feat-name">Enable Logging</div>
      <div class="feat-desc">Toggle serial and dashboard log output</div>
    </div>
    <label class="tgl"><input type="checkbox" id="tgl-eprn" checked onchange="pushFeat()">
      <div class="tgl-track"><div class="tgl-thumb"></div></div></label>
  </div>

  <div class="btn-row">
    <button class="btn btn-stop" onclick="emergencyStop()">Emergency Stop</button>
    <button class="btn" id="btn-resume" style="display:none;background:var(--accBg);color:var(--acc);border:1px solid var(--accBd)" onclick="resumeInj()">Resume Injection</button>
    <button class="btn btn-reboot" onclick="reboot()">Reboot</button>
  </div>
</div>

<div class="card">
  <div class="card-hdr">
    <div class="card-title">CAN Sniffer</div>
    <div class="card-meta" id="sniff-count">0 frames</div>
  </div>
  <div class="sniff-ctrl">
    <input class="sniff-input" id="sniff-filter" placeholder="Filter by ID or name" oninput="renderSniffer()">
    <button class="sniff-btn" id="sniff-pause-btn" onclick="togglePause()">Pause</button>
  </div>
  <div class="sniff-box" id="sniffer">
    <div style="padding:20px;color:var(--tx3);text-align:center;font-size:12px">Waiting for CAN frames</div>
  </div>
</div>

<div class="card">
  <div class="card-hdr"><div class="card-title">CAN Recorder</div><div class="card-meta" id="rec-meta">Idle</div></div>
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

<div class="card">
  <div class="card-hdr">
    <div class="card-title">CAN Controller</div>
    <div style="display:flex;align-items:center;gap:8px">
      <div class="card-meta" id="s-mcp-raw">MCP2515</div>
      <button onclick="resetStats()" style="font-size:10px;padding:2px 8px;border:1px solid var(--bd2);border-radius:5px;background:transparent;color:var(--tx3);cursor:pointer;font-family:inherit">Reset</button>
    </div>
  </div>
  <div class="eflg-row" id="eflg-row"><span class="eflg-pill eflg-ok">OK</span></div>
  <table class="mux-tbl">
    <tr><th>Mux</th><th>RX</th><th>TX</th><th>Errors</th></tr>
    <tr><td>0</td><td id="m0rx">0</td><td id="m0tx">0</td><td id="m0err">0</td></tr>
    <tr><td>1</td><td id="m1rx">0</td><td id="m1tx">0</td><td id="m1err">0</td></tr>
    <tr><td>2</td><td id="m2rx">0</td><td id="m2tx">0</td><td id="m2err">0</td></tr>
  </table>
</div>

<div class="card">
  <div class="card-hdr"><div class="card-title">Firmware Update</div><div class="card-meta">OTA via WiFi</div></div>
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

<div class="card">
  <div class="card-hdr">
    <div class="card-title">WiFi Hotspot</div>
    <div class="card-meta" id="ap-clients">0 clients</div>
  </div>
  <div class="feat-desc" style="margin-bottom:8px">Change the WiFi hotspot name and password</div>
  <div style="display:flex;gap:6px;margin-bottom:6px">
    <input class="sniff-input" id="ap-ssid" placeholder="Hotspot Name" style="flex:1">
    <input class="sniff-input" id="ap-pass" placeholder="New Password (min 8)" type="password" style="flex:1">
  </div>
  <div style="display:flex;gap:6px;align-items:center">
    <button class="sniff-btn" onclick="saveAP()">Save</button>
    <span style="font-size:11px;color:var(--tx3)" id="ap-status"></span>
  </div>
  <div style="font-size:10px;color:var(--tx3);margin-top:6px">Changes take effect after reboot. Leave password empty to keep current.</div>
</div>

<div class="card">
  <div class="card-hdr">
    <div class="card-title">Plugins</div>
    <div class="card-meta" id="plg-count">0 installed</div>
  </div>

  <div style="margin-bottom:14px">
    <div class="feat-name" style="margin-bottom:8px">WiFi Internet</div>
    <div class="feat-desc" style="margin-bottom:8px">Connect to your home WiFi for plugin downloads</div>
    <div style="display:flex;gap:6px;margin-bottom:6px">
      <input class="sniff-input" id="wifi-ssid" placeholder="WiFi SSID" style="flex:1">
      <button class="sniff-btn" onclick="scanWifi()" id="scan-btn">Scan</button>
    </div>
    <div id="wifi-nets" style="display:none;margin-bottom:6px;max-height:140px;overflow-y:auto;border:1px solid var(--bd);border-radius:6px;background:var(--bg2)"></div>
    <div style="display:flex;gap:6px;margin-bottom:6px">
      <input class="sniff-input" id="wifi-pass" placeholder="Password" type="password" style="flex:1">
      <button class="sniff-btn" onclick="saveWifi()">Connect</button>
    </div>
    <div style="font-size:11px;color:var(--tx3);margin-bottom:6px" id="wifi-status">Not configured</div>
    <details style="margin-top:4px">
      <summary style="font-size:11px;color:var(--acc);cursor:pointer;user-select:none">Static IP (optional)</summary>
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

  <div style="padding-top:12px;border-top:1px solid var(--bd);margin-bottom:14px">
    <div class="feat-name" style="margin-bottom:8px">Install Plugin</div>
    <div style="display:flex;gap:6px;margin-bottom:8px">
      <input class="sniff-input" id="plg-url" placeholder="Plugin JSON URL (https://...)" style="flex:1">
      <button class="sniff-btn" onclick="installPlugin()">Install</button>
    </div>
    <div style="display:flex;gap:8px;align-items:center;margin-bottom:8px">
      <input type="file" id="plg-file" accept=".json" onchange="uploadPlugin(this.files[0])" style="display:none">
      <button class="sniff-btn" onclick="$('plg-file').click()">Upload .json</button>
      <span style="font-size:11px;color:var(--tx3)" id="plg-status"></span>
    </div>
    <div class="feat-name" style="margin-bottom:6px">Paste JSON (offline)</div>
    <textarea id="plg-paste" placeholder='{"name":"...","version":"1.0","rules":[...]}' style="width:100%;height:80px;resize:vertical;background:var(--bg2);color:var(--tx);border:1px solid var(--bd);border-radius:6px;padding:8px;font-family:monospace;font-size:11px;box-sizing:border-box;margin-bottom:6px"></textarea>
    <button class="sniff-btn" onclick="pastePlugin()">Install from JSON</button>
  </div>

  <div style="padding-top:12px;border-top:1px solid var(--bd)" id="plg-list">
    <div style="font-size:12px;color:var(--tx3);text-align:center;padding:12px">No plugins installed</div>
  </div>
</div>

<div class="card">
  <div class="card-hdr">
    <div class="card-title">Firmware Update</div>
    <div class="card-meta" id="fw-ver"></div>
  </div>
  <div style="margin-bottom:10px">
    <div class="feat-row">
      <div class="feat-info">
        <div class="feat-name">Beta Channel</div>
        <div class="feat-desc">Include pre-release / beta firmware versions</div>
      </div>
      <label class="tgl"><input type="checkbox" id="beta-tgl" onchange="toggleBeta()"><div class="tgl-track"><div class="tgl-thumb"></div></div></label>
    </div>
  </div>
  <div style="display:flex;gap:6px;align-items:center">
    <button class="sniff-btn" onclick="checkUpdate()" id="upd-check-btn">Check for Updates</button>
    <span style="font-size:11px;color:var(--tx3)" id="upd-status"></span>
  </div>
  <div id="upd-info" style="display:none;margin-top:10px;padding:10px;background:var(--bg2);border-radius:6px">
    <div style="display:flex;justify-content:space-between;align-items:center">
      <div>
        <div style="font-size:13px;font-weight:600" id="upd-ver"></div>
        <div style="font-size:11px;color:var(--tx3)" id="upd-detail"></div>
      </div>
      <button class="sniff-btn" onclick="installUpdate()" id="upd-install-btn" style="background:var(--ok);color:#fff;border-color:var(--ok)">Install</button>
    </div>
  </div>
</div>

<div class="card">
  <div class="card-hdr"><div class="card-title">Live Log</div></div>
  <div class="log-box" id="log">Waiting...</div>
</div>

<div class="warn-bar">CAN bus writes affect vehicle behavior. Remove device immediately if unexpected behavior occurs. Not affiliated with any vehicle manufacturer.</div>
<div class="foot" id="dash-foot">ev-open-can-tools &bull; loading...</div>

<script>
const HW=['Legacy','HW3','HW4'];
const SP3=['Chill','Normal','Hurry'];
const SP4=['Chill','Normal','Hurry','Max','Sloth'];
const $=id=>document.getElementById(id);
function spNames(){return state.hw===2?SP4:SP3;}
const H4O=[{l:'Off',v:0},{l:'+5 km/h',v:7},{l:'+7 km/h',v:10},{l:'+10 km/h',v:14},{l:'+15 km/h',v:21}];
let state={hw:1,sp:1,can:true,h4o:0,spl:false};
let sniffPaused=false,sniffFrames=[];
let otaFile=null;
let otaUser=localStorage.getItem('otaU')||'',otaPass=localStorage.getItem('otaP')||'';
let logSince=0;

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

function buildPills(){
  const ns=spNames(),c=$('sp-pills');c.innerHTML='';
  const auto=document.createElement('button');
  auto.className='pill'+(!state.spl?' active':'');
  auto.textContent='Auto';auto.onclick=()=>setSPL(false);c.appendChild(auto);
  ns.forEach((n,i)=>{
    const b=document.createElement('button');
    b.className='pill'+(state.spl&&i===state.sp?' active':'');
    b.dataset.v=i;b.textContent=n;b.onclick=()=>setSP(i);c.appendChild(b);
  });
  const hc=$('h4o-pills');if(!hc)return;hc.innerHTML='';
  H4O.forEach(o=>{
    const b=document.createElement('button');
    b.className='pill'+(o.v===state.h4o?' active':'');
    b.textContent=o.l;b.onclick=()=>{state.h4o=o.v;buildPills();pushFeat();};hc.appendChild(b);
  });
}

function updateHW4(hw){
  document.querySelectorAll('.hw4-only').forEach(el=>el.classList.toggle('hidden',hw!==2));
  ['tgl-isa','tgl-evd'].forEach(id=>{const e=$(id);if(e)e.disabled=hw!==2;});
}

function updSeg(el,v,cls){
  el.querySelectorAll('.'+cls).forEach(b=>b.classList.toggle('active',parseInt(b.dataset.v)===v));
}

function setHW(v){state.hw=v;updSeg($('hw-seg'),v,'hw-btn');buildPills();updateHW4(v);pushCfg();}
function setSP(v){state.sp=v;state.spl=true;buildPills();pushCfg();}
function setSPL(v){state.spl=v;buildPills();pushCfg();}

async function pushCfg(){
  const body='hw='+state.hw+'&sp='+state.sp+'&can='+(state.can?'1':'0')+'&spl='+(state.spl?'1':'0');
  try{await fetch('/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});}catch(e){}
}

async function pushFeat(){
  const body='AD='+($('tgl-AD').checked?'1':'0')
    +'&nag='+($('tgl-nag').checked?'1':'0')
    +'&summon='+($('tgl-summon').checked?'1':'0')
    +'&isa='+($('tgl-isa').checked?'1':'0')
    +'&evd='+($('tgl-evd').checked?'1':'0')
    +'&nk='+($('tgl-nk').checked?'1':'0')
    +'&fAD='+($('tgl-fAD').checked?'1':'0')
    +'&eprn='+($('tgl-eprn').checked?'1':'0')
    +'&h4o='+state.h4o;
  try{await fetch('/features',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});}catch(e){}
  poll();
}

async function emergencyStop(){if(!confirm('Emergency stop: disable all injection?'))return;try{await fetch('/disable',{method:'POST'});}catch(e){}poll();}
async function resumeInj(){try{await fetch('/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'hw='+state.hw+'&sp='+state.sp+'&can=1'});}catch(e){}poll();}
async function reboot(){if(!confirm('Reboot device?'))return;try{await fetch('/reboot',{method:'POST'});}catch(e){}}

function fmtUp(s){
  if(s<60)return s+'s';
  if(s<3600)return Math.floor(s/60)+'m '+String(s%60).padStart(2,'0')+'s';
  return Math.floor(s/3600)+'h '+Math.floor((s%3600)/60)+'m';
}
function toHex(n,p){return n.toString(16).toUpperCase().padStart(p,'0')}

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
  const b=$('sniff-pause-btn');
  b.textContent=sniffPaused?'Resume':'Pause';
  b.classList.toggle('paused',sniffPaused);
}

function renderSniffer(){
  if(sniffPaused)return;
  const filter=$('sniff-filter').value.trim().toLowerCase();
  const el=$('sniffer');
  let frames=sniffFrames;
  if(filter){
    const fid=parseInt(filter);
    if(!isNaN(fid))frames=frames.filter(f=>f.id===fid);
    else frames=frames.filter(f=>f.name&&f.name.toLowerCase().includes(filter));
  }
  $('sniff-count').textContent=frames.length+' frames';
  if(!frames.length){
    el.innerHTML='<div style="padding:20px;color:var(--tx3);text-align:center;font-size:12px">No frames</div>';
    return;
  }
  const ADIds=new Set([1021,1016,921]);
  el.innerHTML=frames.slice(-30).reverse().map(f=>{
    const hex=Array.from({length:f.dlc},(_,i)=>toHex(f.data[i],2)).join(' ');
    return`<div class="sniff-row${ADIds.has(f.id)?' hi':''}">
      <span class="s-ts">${(f.ts/1000).toFixed(1)}s</span>
      <span class="s-id">0x${toHex(f.id,3)}</span>
      <div><div class="s-data">${hex}</div>${f.name?`<div class="s-name">${f.name}</div>`:''}</div>
    </div>`;
  }).join('');
}

async function pollSniffer(){
  if(sniffPaused)return;
  try{const r=await fetch('/frames');const d=await r.json();sniffFrames=d.frames||[];renderSniffer();}catch(e){}
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
  try{
    const r=await fetch('/status');const d=await r.json();
    const on=d.can;
    $('s-can').textContent=on?'Active':'Offline';
    $('s-can').className='stat-val '+(on?'v-ok':'v-err');
    $('s-inj').textContent=d.ci?'Active':'BLOCKED';
    $('s-inj').className='stat-val '+(d.ci?'v-ok':'v-err');
    $('s-AD').textContent=d.AD?'Active':'Inactive';
    $('s-AD').className='stat-val '+(d.AD?'v-ok':'v-dim');
    $('s-fps').textContent=d.fps.toFixed(1)+' Hz';
    $('s-fps').className='stat-val '+(d.fps>5?'v-acc':'v-dim');
    $('s-rx').textContent=d.rx;
    $('s-tx').textContent=d.tx;
    $('s-txerr').textContent=d.txerr;
    $('s-txerr').className='stat-val '+(d.txerr>0?'v-warn':'v-dim');
    $('s-fd').textContent=d.fd||'—';
    $('s-prof').textContent=spNames()[d.sp]||'—';
    $('s-soff').textContent=d.soff||'0';
    $('s-up').textContent=fmtUp(d.up);
    $('s-mcp-raw').textContent='EFLG: 0x'+toHex(d.eflg,2);
    $('fps-fill').style.width=Math.min(d.fps/20*100,100)+'%';
    $('hw-badge').textContent=HW[d.hw]||'?';
    $('dot').className='sdot '+(d.txerr>5?'dot-warn':on?'dot-on':'dot-off');
    $('hdr-desc').textContent=on?(d.AD?'AD active — injecting':'CAN active — monitoring'):'Waiting for CAN frames';
    renderEflg(d.eflg);
    if(d.mux){for(let i=0;i<3;i++){$(('m'+i+'rx')).textContent=d.mux[i].rx;$(('m'+i+'tx')).textContent=d.mux[i].tx;const e=$(('m'+i+'err'));e.textContent=d.mux[i].err;e.style.color=d.mux[i].err>0?'var(--err)':'';}}
    state.hw=d.hw;state.sp=d.sp;state.can=d.ci;
    $('btn-resume').style.display=d.ci?'none':'';
    updSeg($('hw-seg'),d.hw,'hw-btn');buildPills();updateHW4(d.hw);
    if(d.feat){$('tgl-AD').checked=d.feat.AD;$('tgl-nag').checked=d.feat.nag;$('tgl-summon').checked=d.feat.summon;$('tgl-isa').checked=d.feat.isa;$('tgl-evd').checked=d.feat.evd;$('tgl-nk').checked=d.feat.nk;if(typeof d.feat.h4o!=='undefined'){state.h4o=d.feat.h4o;buildPills();}if(typeof d.feat.spl!=='undefined'){state.spl=d.feat.spl;}}
    if(typeof d.fAD!=='undefined')$('tgl-fAD').checked=d.fAD;
    if(typeof d.eprn!=='undefined')$('tgl-eprn').checked=d.eprn;
  }catch(e){}
}

function colorLog(l){
  if(l.includes('AD=ON')||l.includes('AD active'))return'<span class="lf">'+l+'</span>';
  if(l.match(/\[HW[34]\]|\[LEGACY\]|\[HW3\]/))return'<span class="lh">'+l+'</span>';
  if(l.includes('ERR')||l.includes('FAIL'))return'<span class="le">'+l+'</span>';
  if(l.includes('[CFG]')||l.includes('[FEAT]'))return'<span class="lc">'+l+'</span>';
  if(l.includes('[OK]')||l.includes('[BOOT]'))return'<span class="lf">'+l+'</span>';
  if(l.includes('[OTA]'))return'<span class="lo">'+l+'</span>';
  return l;
}
async function pollLog(){
  try{
    const r=await fetch('/log?since='+logSince);const d=await r.json();
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
  const ssid=$('ap-ssid').value,pass=$('ap-pass').value;
  if(!ssid){$('ap-status').textContent='Enter hotspot name';$('ap-status').style.color='var(--err)';return;}
  if(pass&&pass.length<8){$('ap-status').textContent='Password min 8 chars';$('ap-status').style.color='var(--err)';return;}
  try{const r=await fetch('/ap_config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass)});
    const d=await r.json();
    if(d.ok){$('ap-status').textContent='Saved! Reboot to apply.';$('ap-status').style.color='var(--ok)';$('ap-pass').value='';}
    else{$('ap-status').textContent=d.error||'Error';$('ap-status').style.color='var(--err)';}
  }catch(e){$('ap-status').textContent='Error';$('ap-status').style.color='var(--err)';}
}
async function loadApStatus(){
  try{const r=await fetch('/ap_status');const d=await r.json();
    if(d.ssid)$('ap-ssid').value=d.ssid;
    $('ap-clients').textContent=d.clients+' client'+(d.clients!==1?'s':'');
  }catch(e){}
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
  try{const r=await fetch('/wifi_status');const d=await r.json();
    if(d.ssid)$('wifi-ssid').value=d.ssid;
    if(d.connected){$('wifi-status').textContent='Connected: '+d.ip;$('wifi-status').style.color='var(--ok)';}
    else if(d.ssid){$('wifi-status').textContent='Connecting to '+d.ssid+'...';$('wifi-status').style.color='var(--acc)';}
    if(d.static){$('wifi-static').checked=true;toggleStaticIP();
      if(d.cfg_ip)$('wifi-ip').value=d.cfg_ip;
      if(d.cfg_gw)$('wifi-gw').value=d.cfg_gw;
      if(d.cfg_mask)$('wifi-mask').value=d.cfg_mask;
      if(d.cfg_dns)$('wifi-dns').value=d.cfg_dns;
    }
  }catch(e){}
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
  $('plg-status').textContent='Downloading...';$('plg-status').style.color='var(--acc)';
  try{const r=await fetch('/plugin_install',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'url='+encodeURIComponent(url)});
    const d=await r.json();
    if(d.ok){$('plg-url').value='';$('plg-status').textContent='Installed!';$('plg-status').style.color='var(--ok)';pollPlugins();}
    else{$('plg-status').textContent=d.error||'Error';$('plg-status').style.color='var(--err)';}
  }catch(e){$('plg-status').textContent='Connection error';$('plg-status').style.color='var(--err)';}
}
async function uploadPlugin(file){
  if(!file)return;
  $('plg-status').textContent='Uploading...';$('plg-status').style.color='var(--acc)';
  try{const text=await file.text();
    const r=await fetch('/plugin_upload',{method:'POST',headers:{'Content-Type':'application/json'},body:text});
    const d=await r.json();
    if(d.ok){$('plg-status').textContent='Installed!';$('plg-status').style.color='var(--ok)';pollPlugins();}
    else{$('plg-status').textContent=d.error||'Invalid JSON';$('plg-status').style.color='var(--err)';}
  }catch(e){$('plg-status').textContent='Error';$('plg-status').style.color='var(--err)';}
}
async function pastePlugin(){
  const text=$('plg-paste').value.trim();
  if(!text){$('plg-status').textContent='Paste JSON first';$('plg-status').style.color='var(--err)';return;}
  try{JSON.parse(text);}catch(e){$('plg-status').textContent='Invalid JSON: '+e.message;$('plg-status').style.color='var(--err)';return;}
  $('plg-status').textContent='Installing...';$('plg-status').style.color='var(--acc)';
  try{const r=await fetch('/plugin_upload',{method:'POST',headers:{'Content-Type':'application/json'},body:text});
    const d=await r.json();
    if(d.ok){$('plg-paste').value='';$('plg-status').textContent='Installed!';$('plg-status').style.color='var(--ok)';pollPlugins();}
    else{$('plg-status').textContent=d.error||'Invalid plugin';$('plg-status').style.color='var(--err)';}
  }catch(e){$('plg-status').textContent='Connection error';$('plg-status').style.color='var(--err)';}
}
async function togglePlugin(idx){
  try{await fetch('/plugin_toggle',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'idx='+idx});pollPlugins();}catch(e){}
}
async function removePlugin(idx){
  if(!confirm('Remove this plugin?'))return;
  try{await fetch('/plugin_remove',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'idx='+idx});pollPlugins();}catch(e){}
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
      if(r.conflict) hdr+=' <span style="color:var(--err);font-weight:bold" title="This CAN ID is also handled by the base firmware">&#9888; Firmware overlap</span>';
      hdr+='</div>';
      let ops=r.ops.map(o=>'<div style="padding-left:12px;color:var(--tx2)">'+fmtOp(o)+'</div>').join('');
      return hdr+ops;
    }).join('<div style="border-top:1px solid var(--bd);margin:4px 0"></div>')
    +'</div>';
}
function toggleDetails(idx){
  var el=$('plg-det-'+idx);
  if(el)el.style.display=el.style.display==='none'?'block':'none';
}
async function pollPlugins(){
  try{const r=await fetch('/plugins');const d=await r.json();
    $('plg-count').textContent=d.plugins.length+' installed';
    const el=$('plg-list');
    if(!d.plugins.length){el.innerHTML='<div style="font-size:12px;color:var(--tx3);text-align:center;padding:12px">No plugins installed</div>';}
    else{el.innerHTML=d.plugins.map((p,i)=>{
      let hasConflict=p.details&&p.details.some(r=>r.conflict);
      let row='<div style="margin-bottom:8px;padding-bottom:8px;border-bottom:1px solid var(--bd)">';
      row+='<div class="feat-row"><div class="feat-info" style="cursor:pointer" onclick="toggleDetails('+i+')">';
      row+='<div class="feat-name">'+p.name+' <span style="color:var(--tx3);font-size:11px">v'+p.version+'</span>';
      if(hasConflict) row+=' <span style="color:var(--err);font-size:11px">&#9888;</span>';
      row+='</div>';
      row+='<div class="feat-desc">'+p.rules+' rule'+(p.rules!==1?'s':'')+(p.author?' &bull; '+p.author:'')+' &bull; <span style="color:var(--acc);cursor:pointer">details</span></div>';
      row+='</div>';
      row+='<label class="tgl"><input type="checkbox" '+(p.enabled?'checked':'')+' onchange="togglePlugin('+i+')"><div class="tgl-track"><div class="tgl-thumb"></div></div></label>';
      row+='<button onclick="removePlugin('+i+')" style="margin-left:8px;padding:4px 8px;border:1px solid var(--errBd);border-radius:5px;background:transparent;color:var(--err);cursor:pointer;font-size:10px;font-family:inherit">X</button></div>';
      if(p.details){
        row+='<div id="plg-det-'+i+'" style="display:none">';
        if(hasConflict) row+='<div style="margin-top:6px;padding:6px 8px;background:var(--errBg,#3a1a1a);border:1px solid var(--errBd);border-radius:6px;font-size:11px;color:var(--err)">&#9888; Some CAN IDs overlap with base firmware. Plugin rules run <b>after</b> the original handler. Both will send modified frames.</div>';
        row+=renderPluginDetails(p.details);
        row+='</div>';
      }
      row+='</div>';
      return row;
    }).join('');}
  }catch(e){}
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
  if(!confirm('Install firmware update? The device will reboot.'))return;
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
}
function updateFoot(ver){
  var ip=location.hostname||'192.168.4.1';
  $('dash-foot').textContent='ev-open-can-tools \u2022 v'+ver+' \u2022 '+ip;
}

setInterval(poll,2000);setInterval(pollLog,3000);setInterval(pollSniffer,1000);setInterval(pollPlugins,10000);setInterval(loadWifiStatus,10000);setInterval(loadApStatus,10000);
updateHW4(1);buildPills();poll();pollLog();pollSniffer();pollRec();pollPlugins();loadWifiStatus();loadApStatus();loadUpdateInfo();
</script>
</body>
</html>
)HTML";
