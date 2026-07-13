#pragma once
#ifdef ESP_PLATFORM
#include "platform/espidf_runtime.h"
#else
#include <Arduino.h>
#endif

// Authoritative dashboard source. scripts/minify_dashboard.py generates
// mcp2515_dashboard_ui.h from this file; never edit the generated header.
static const char DASH_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en" data-theme="dark">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EV Open CAN</title>
<style>
:root{--bg:#0d0f12;--card:#171a20;--line:#2b3038;--text:#edf1f7;--muted:#8993a2;--blue:#5b8fff;--green:#3dba72;--yellow:#f5a623;--red:#ff5b5b}
[data-theme=light]{--bg:#f4f6f9;--card:#fff;--line:#d9dee7;--text:#151922;--muted:#687386}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:13px/1.45 system-ui,sans-serif}button,input,select,textarea{font:inherit}header{position:sticky;top:0;z-index:2;background:rgba(13,15,18,.94);border-bottom:1px solid var(--line);padding:12px 16px;display:flex;gap:12px;align-items:center}header h1{font-size:16px;margin:0}header .sub{color:var(--muted);font-size:11px}.dot{width:9px;height:9px;border-radius:50%;background:var(--yellow)}.dot.ok{background:var(--green)}.dot.bad{background:var(--red)}main{max-width:980px;margin:auto;padding:14px;display:grid;gap:12px}.card{background:var(--card);border:1px solid var(--line);border-radius:9px;padding:14px}.card h2{font-size:13px;margin:0 0 12px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(135px,1fr));gap:8px}.stat,.box{border:1px solid var(--line);border-radius:7px;padding:9px}.label{font-size:10px;color:var(--muted);text-transform:uppercase}.value{font-size:17px;font-weight:650;margin-top:3px}.ok{color:var(--green)}.warn{color:var(--yellow)}.bad{color:var(--red)}.muted{color:var(--muted)}.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px}.row>*{min-width:0}.field{display:flex;flex-direction:column;gap:4px;flex:1;min-width:130px}.field span{color:var(--muted);font-size:11px}input,select,textarea{background:var(--bg);color:var(--text);border:1px solid var(--line);border-radius:6px;padding:8px}textarea{width:100%;min-height:90px;resize:vertical}button,.button{border:1px solid var(--line);background:transparent;color:var(--text);border-radius:6px;padding:8px 11px;cursor:pointer;text-decoration:none}button.primary{background:var(--blue);border-color:var(--blue);color:#fff}button.danger{color:var(--red)}button:disabled,input:disabled,select:disabled,textarea:disabled{opacity:.45;cursor:not-allowed}.status{font-size:11px;color:var(--muted);min-height:16px;margin-top:7px}.section{border-top:1px solid var(--line);margin-top:14px;padding-top:12px}.section h3{font-size:12px;margin:0 0 8px}.plugin{display:flex;gap:8px;align-items:center;border-top:1px solid var(--line);padding:9px 0}.plugin:first-child{border-top:0}.plugin .name{flex:1}.plugin small{display:block;color:var(--muted)}.probe{font-family:ui-monospace,monospace;word-spacing:4px}.note{color:var(--muted);font-size:11px}.hidden{display:none!important}.footer{text-align:center;color:var(--muted);font-size:10px;padding:10px}.toast{position:fixed;right:14px;bottom:14px;background:var(--card);border:1px solid var(--line);padding:9px 12px;border-radius:7px;display:none;max-width:360px}.toast.show{display:block}.scan-item{display:flex;justify-content:space-between;border-top:1px solid var(--line);padding:6px;cursor:pointer}.scan-item:hover{color:var(--blue)}details.card>summary{cursor:pointer;font-weight:650;list-style-position:inside}.support-report{margin:12px 0 0;max-height:65vh;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;background:var(--bg);border:1px solid var(--line);border-radius:7px;padding:11px;font:11px/1.5 ui-monospace,monospace;color:var(--text)}
.onboarding{position:fixed;inset:0;z-index:20;background:rgba(0,0,0,.72);display:grid;place-items:center;padding:18px}
.onboarding-panel{width:min(680px,100%);max-height:92vh;overflow:auto;background:var(--card);border:1px solid var(--line);border-radius:12px;padding:18px;box-shadow:0 20px 70px rgba(0,0,0,.45)}
.onboarding-head{display:flex;gap:12px;align-items:flex-start}.onboarding-head h2{font-size:18px;margin:0}.onboarding-head .note{margin-top:3px}
.onboarding-progress{height:5px;background:var(--bg);border-radius:4px;overflow:hidden;margin:14px 0 18px}.onboarding-progress span{display:block;height:100%;width:0;background:var(--blue);transition:width .2s}
.onboarding-step{display:none}.onboarding-step.active{display:block}.onboarding-step h3{font-size:15px;margin:0 0 8px}.onboarding-step ul{padding-left:20px}
.onboarding-actions{display:flex;gap:8px;justify-content:flex-end;border-top:1px solid var(--line);margin-top:18px;padding-top:14px}.onboarding-actions #onboarding-skip{margin-right:auto}
.onboarding-review{white-space:pre-wrap;background:var(--bg);border:1px solid var(--line);border-radius:7px;padding:10px;font:12px/1.5 ui-monospace,monospace}
.onboarding-choice{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:10px}.onboarding-choice label{border:1px solid var(--line);border-radius:8px;padding:10px;cursor:pointer}.onboarding-choice input{margin-right:6px}
@media(max-width:560px){.onboarding-choice{grid-template-columns:1fr}.onboarding-actions{flex-wrap:wrap}.onboarding-actions #onboarding-skip{margin-right:0}.onboarding-actions button{flex:1}}
</style>
</head>
<body>
<header><span class="dot" id="dot"></span><div><h1>EV Open CAN</h1><div class="sub" id="header-status">Starting dashboard…</div></div><button style="margin-left:auto" onclick="openOnboarding(true)">Setup</button><button onclick="toggleTheme()">Theme</button></header>
<div class="onboarding hidden" id="onboarding" role="dialog" aria-modal="true" aria-labelledby="onboarding-title">
  <div class="onboarding-panel">
    <div class="onboarding-head"><div><h2 id="onboarding-title">Device setup</h2><div class="note" id="onboarding-subtitle">Step 1 of 6</div></div></div>
    <div class="onboarding-progress"><span id="onboarding-progress"></span></div>
    <section class="onboarding-step" data-step="0">
      <h3>Welcome to EV Open CAN</h3>
      <p>This guided setup configures the essentials without enabling CAN injection.</p>
      <ul><li>Select the Tesla hardware generation.</li><li>Optionally connect the board to an internet WiFi network.</li><li>Verify the CAN GPIO pins.</li><li>Review the safety gate before saving.</li></ul>
      <label><input id="onboarding-safety" type="checkbox"> I understand this is safety-critical research firmware and I remain responsible for validating every setting.</label>
    </section>
    <section class="onboarding-step" data-step="1">
      <h3>Vehicle and safety profile</h3>
      <div class="onboarding-choice"><label><input type="radio" name="onboarding-hw" value="0"> Legacy</label><label><input type="radio" name="onboarding-hw" value="1" checked> HW3</label><label><input type="radio" name="onboarding-hw" value="2"> HW4</label></div>
      <div class="row"><label class="field"><span>Speed profile</span><select id="onboarding-speed" onchange="updateOnboardingSpeedOptions()"><option value="auto">Auto</option><option value="0">Chill</option><option value="1">Normal</option><option value="2">Hurry</option><option value="3">Max (HW4)</option><option value="4">Sloth (HW4)</option></select></label></div>
      <label><input id="onboarding-ap-gate" type="checkbox"> Start injection only after the AP gate is satisfied</label>
      <p class="note">The wizard never arms injection. You must explicitly arm it from the dashboard after validating live CAN traffic.</p>
    </section>
    <section class="onboarding-step" data-step="2">
      <h3>Internet WiFi</h3>
      <p class="note">Optional. The device hotspot remains available. Credentials are saved only to the device.</p>
      <div class="row"><label class="field"><span>SSID</span><input id="onboarding-wifi-ssid"></label><label class="field"><span>Password</span><input id="onboarding-wifi-pass" type="password"></label><button id="onboarding-scan-btn" onclick="scanOnboardingWifi()">Scan</button></div>
      <div id="onboarding-wifi-scan"></div><div class="status" id="onboarding-wifi-status">Leave the SSID empty to skip internet WiFi.</div>
    </section>
    <section class="onboarding-step" data-step="3">
      <h3>CAN GPIO pins</h3>
      <div class="row"><label class="field"><span>TX GPIO</span><input id="onboarding-can-tx" type="number"></label><label class="field"><span>RX GPIO</span><input id="onboarding-can-rx" type="number"></label></div>
      <p class="note">Keep the detected defaults unless your board wiring requires different pins. Changing these values requires a reboot and an incorrect value can disable CAN communication.</p>
    </section>
    <section class="onboarding-step" data-step="4"><h3>Review setup</h3><div class="onboarding-review" id="onboarding-review"></div><div class="status" id="onboarding-save-status">Injection will remain stopped after saving.</div></section>
    <section class="onboarding-step" data-step="5"><h3>Setup complete</h3><p>Your settings were saved. CAN injection is still stopped and must be armed manually from the dashboard.</p><p class="note" id="onboarding-reboot-note"></p></section>
    <div class="onboarding-actions"><button id="onboarding-skip" onclick="skipOnboarding()">Skip setup</button><button id="onboarding-back" onclick="onboardingBack()">Back</button><button id="onboarding-next" class="primary" onclick="onboardingNext()">Next</button></div>
  </div>
</div>
<main>
<section class="card"><h2>Runtime status</h2><div class="grid">
<div class="stat"><div class="label">CAN</div><div class="value" id="s-can">—</div></div>
<div class="stat"><div class="label">Injection</div><div class="value" id="s-inj">—</div></div>
<div class="stat"><div class="label">Frames</div><div class="value" id="s-rx">0</div></div>
<div class="stat"><div class="label">Last CAN frame</div><div class="value" id="s-age">—</div></div>
<div class="stat"><div class="label">TX OK / fail</div><div class="value" id="s-tx">0 / 0</div></div>
<div class="stat"><div class="label">CAN driver state</div><div class="value" id="s-twai">—</div></div>
<div class="stat"><div class="label">Free heap</div><div class="value" id="s-heap">—</div></div>
<div class="stat"><div class="label">Uptime</div><div class="value" id="s-up">—</div></div>
</div><div class="status" id="runtime-note"></div></section>

<section class="card"><h2>Last Write Check</h2><div class="value muted" id="probe-status">No injected frame yet</div>
<div class="section"><div class="label">Sent <span id="probe-tx-meta"></span></div><div class="probe" id="probe-tx">—</div></div>
<div class="section"><div class="label">Latest matching bus frame <span id="probe-rx-meta"></span></div><div class="probe" id="probe-rx">—</div></div>
<p class="note">Compares the last write attempt with the next frame of the same ID and mux. It detects overwrites; it does not prove ECU acceptance.</p></section>

<section class="card" id="config-card"><h2>Configuration and safety</h2><div id="config-controls">
<div class="row"><label class="field"><span>Hardware</span><select id="cfg-hw" onchange="updateSpeedOptions()"><option value="0">Legacy</option><option value="1">HW3</option><option value="2">HW4</option></select></label>
<label class="field"><span>Speed profile</span><select id="cfg-speed"><option value="auto">Auto</option><option value="0">Chill</option><option value="1">Normal</option><option value="2">Hurry</option><option value="3">Max (HW4)</option><option value="4">Sloth (HW4)</option></select></label>
<label class="field"><span>Plugin replay</span><input id="plugin-replay" type="number" min="1" max="20" value="1"></label>
<label class="field"><span>LED brightness</span><input id="led-brightness" type="number" min="0" max="255" value="32"></label></div>
<div class="row"><label><input id="ap-gate-tgl" type="checkbox"> Start injection after AP gate</label><label><input id="hw3-slew" type="checkbox"> HW3/HW4 offset slew</label><label class="field" style="max-width:150px"><span>Slew rate %/s</span><input id="hw3-slew-rate" type="number" min="1" max="25" value="5"></label></div>
<div class="row"><button class="primary" onclick="saveRuntimeConfig()">Save configuration</button><button id="btn-stop" class="danger" onclick="stopInjection()">Stop injection</button><button id="btn-resume" onclick="resumeInjection()">Arm injection</button><button onclick="rebootDevice()">Reboot</button></div>
</div><div class="status" id="config-status">Loading configuration…</div></section>

<section class="card"><h2>Plugins</h2><div id="plugin-list" class="note">Loading…</div>
<div class="section"><div class="row"><label class="field"><span>HTTPS plugin URL</span><input id="plugin-url" placeholder="https://…/plugin.json"></label><button onclick="installPluginUrl()">Install URL</button></div>
<label class="field"><span>Plugin JSON</span><textarea id="plugin-json" placeholder='{"name":"example","version":"1.0","rules":[]}'></textarea></label>
<div class="row"><input id="plugin-file" type="file" accept="application/json,.json"><button class="primary" onclick="installPluginJson()">Install JSON</button></div><div class="status" id="plugin-status"></div></div></section>

<section class="card"><h2>SavvyCAN USB serial</h2><div class="grid"><div class="stat"><div class="label">Session</div><div class="value" id="gvret-session">Off</div></div><div class="stat"><div class="label">Frames / dropped</div><div class="value" id="gvret-frames">0 / 0</div></div></div>
<div class="row"><button class="primary" onclick="startGvret()">Arm GVRET</button><button onclick="stopGvret()">Stop GVRET</button></div>
<p class="note">In SavvyCAN: Connection → Add Connection → Serial Connection (GVRET), select this board’s USB serial port, 115200 baud. Once SavvyCAN connects, text logging is suppressed so binary GVRET frames remain clean. One 500 kbit/s CAN bus is exposed; the reference command set is read-only.</p><div class="status" id="gvret-status"></div></section>

<section class="card"><h2>Connectivity</h2>
<div class="section" style="border-top:0;margin-top:0;padding-top:0"><h3>WiFi hotspot</h3><div class="row"><label class="field"><span>SSID</span><input id="ap-ssid"></label><label class="field"><span>New password (8–63)</span><input id="ap-pass" type="password" maxlength="63"></label><label><input id="ap-hidden" type="checkbox"> Hidden</label><button onclick="saveAP()">Save hotspot</button></div><div class="status" id="ap-status"></div></div>
<div class="section"><h3>WiFi internet</h3><div id="wifi-nets" class="note">No saved networks</div><div class="row"><label class="field"><span>SSID</span><input id="wifi-ssid"></label><label class="field"><span>Password</span><input id="wifi-pass" type="password"></label><button id="scan-btn" onclick="scanWifi()">Scan</button><button onclick="saveWifi()">Save network</button></div>
<label><input id="wifi-static" type="checkbox" onchange="toggleStaticFields()"> Static IPv4</label><div class="row hidden" id="wifi-static-fields"><input id="wifi-ip" placeholder="IP"><input id="wifi-gw" placeholder="Gateway"><input id="wifi-mask" placeholder="Mask"><input id="wifi-dns" placeholder="DNS"></div><div id="wifi-scan"></div><div class="status" id="wifi-status">Not configured</div></div>
<div class="section"><h3>CAN pins</h3><div class="row"><label class="field"><span>TX GPIO</span><input id="can-tx" type="number"></label><label class="field"><span>RX GPIO</span><input id="can-rx" type="number"></label><button onclick="saveCanPins()">Save and reboot</button></div><div class="status" id="can-pins-status"></div></div>
<div class="section"><h3>Settings backup</h3><div class="row"><button onclick="exportSettings()">Export JSON</button><input id="settings-file" type="file" accept="application/json,.json"><button onclick="importSettings()">Restore and reboot</button></div><div class="status" id="backup-status"></div></div>
</section>

<section class="card"><h2>Firmware update <span id="fw-ver" class="muted"></span></h2><div class="row"><label><input id="beta-tgl" type="checkbox" onchange="saveUpdateFlags()"> Beta channel</label><label><input id="auto-upd-tgl" type="checkbox" onchange="saveUpdateFlags()"> Automatic update</label><button onclick="checkUpdate()">Check release</button><button id="install-update" class="primary hidden" onclick="installUpdate()">Install update</button><button id="ota-reset-btn" onclick="resetOtaCredentials()">Reset OTA credentials</button></div>
<div class="row"><input id="ota-file" type="file" accept=".bin,application/octet-stream"><button onclick="uploadFirmware()">Upload firmware</button></div><div class="status" id="update-status"></div></section>

<details class="card" id="support-card" ontoggle="supportToggle(this)"><summary>Support diagnostics <span id="support-health" class="muted">on demand</span></summary>
<p class="note">Generates one bounded report without passwords, credentials, tokens, or keys. Open only when diagnosing a problem.</p>
<div class="row"><button id="support-refresh" class="primary" onclick="loadSupport(true)">Refresh report</button><button id="support-copy" onclick="copySupport()">Copy report</button><a class="button" href="https://github.com/ev-open-can-tools/ev-open-can-tools/issues/new?template=issue.yml" target="_blank" rel="noopener">Open GitHub issue</a></div>
<div class="status" id="support-status">Expand this section to load diagnostics.</div><pre class="support-report" id="support-report">No report loaded.</pre></details>

<div class="footer">EV Open CAN · safety-critical research firmware</div>
</main><div class="toast" id="toast"></div>
<script>
const $=id=>document.getElementById(id);let config=null,plugins=[],pendingUpdateUrl='',supportReport='',gvretWatching=false,otaUser=localStorage.getItem('otaU')||'',otaPass=localStorage.getItem('otaP')||'';const locks={};
let onboardingStep=0,onboardingCanOriginal={tx:null,rx:null},onboardingNeedsReboot=false;
const onboardingKey='evOpenCanOnboardingV1';
function show(message,bad=false){const t=$('toast');t.textContent=message;t.style.color=bad?'var(--red)':'';t.classList.add('show');clearTimeout(show.timer);show.timer=setTimeout(()=>t.classList.remove('show'),2500)}
function setStatus(id,message,bad=false){const e=$(id);if(e){e.textContent=message;e.style.color=bad?'var(--red)':''}}
function toggleTheme(){const root=document.documentElement;const next=root.dataset.theme==='dark'?'light':'dark';root.dataset.theme=next;localStorage.setItem('theme',next)}document.documentElement.dataset.theme=localStorage.getItem('theme')||'dark';
async function requestJson(url,options={},timeout=4000){const c=new AbortController(),timer=setTimeout(()=>c.abort(),timeout);try{const r=await fetch(url,{...options,signal:c.signal});const text=await r.text();let data={};try{data=text?JSON.parse(text):{}}catch(e){throw new Error('Invalid response')}if(!r.ok)throw new Error(data.error||`HTTP ${r.status}`);return data}finally{clearTimeout(timer)}}
async function requestText(url,options={},timeout=8000){const c=new AbortController(),timer=setTimeout(()=>c.abort(),timeout);try{const r=await fetch(url,{...options,signal:c.signal});const text=await r.text();if(!r.ok)throw new Error(text||`HTTP ${r.status}`);return text}finally{clearTimeout(timer)}}
async function locked(name,work){if(locks[name])return;locks[name]=true;try{return await work()}finally{locks[name]=false}}
function form(data){return new URLSearchParams(data).toString()}function fmtAge(ms){if(ms===null||ms===undefined||ms>=4294967295)return'never';if(ms<1000)return ms+' ms';return(ms/1000).toFixed(ms<10000?1:0)+' s'}function fmtBytes(n){return n>1048576?(n/1048576).toFixed(1)+' MB':Math.round(n/1024)+' KB'}function hexData(data,dlc){return Array.isArray(data)?data.slice(0,dlc).map(v=>(v&255).toString(16).padStart(2,'0').toUpperCase()).join(' '):'—'}

function selectedOnboardingHw(){return Number(document.querySelector('input[name="onboarding-hw"]:checked')?.value??1)}
function updateOnboardingSpeedOptions(){
  const hw=selectedOnboardingHw();
  for(const option of $('onboarding-speed').options)option.disabled=option.value!=='auto'&&Number(option.value)>2&&hw!==2;
  if($('onboarding-speed').selectedOptions[0]?.disabled)$('onboarding-speed').value='auto';
}
document.querySelectorAll('input[name="onboarding-hw"]').forEach(element=>element.addEventListener('change',updateOnboardingSpeedOptions));
function syncOnboarding(){
  if(config){
    const hw=document.querySelector(`input[name="onboarding-hw"][value="${config.hw}"]`);
    if(hw)hw.checked=true;
    $('onboarding-speed').value=config.speedAuto?'auto':String(config.speedProfile);
    $('onboarding-ap-gate').checked=!!config.apGate;
  }
  const txText=$('can-tx').value.trim(),rxText=$('can-rx').value.trim();
  if(txText&&rxText){
    onboardingCanOriginal={tx:Number(txText),rx:Number(rxText)};
    $('onboarding-can-tx').value=txText;
    $('onboarding-can-rx').value=rxText;
  }
  updateOnboardingSpeedOptions();
  updateOnboardingReview();
}
function onboardingClearHash(){if(location.hash==='#/onboarding')history.replaceState(null,'',location.pathname+location.search)}
function openOnboarding(force=false){
  if(!force&&location.hash!=='#/onboarding'&&localStorage.getItem(onboardingKey))return;
  onboardingStep=0;onboardingNeedsReboot=false;syncOnboarding();
  $('onboarding').classList.remove('hidden');document.body.style.overflow='hidden';
  if(force&&location.hash!=='#/onboarding')history.replaceState(null,'','#/onboarding');
  renderOnboarding();
}
function closeOnboarding(){onboardingClearHash();$('onboarding').classList.add('hidden');document.body.style.overflow=''}
function skipOnboarding(){localStorage.setItem(onboardingKey,'1');closeOnboarding()}
function onboardingBack(){if(onboardingStep>0){onboardingStep--;renderOnboarding()}}
function updateOnboardingReview(){
  const hw=['Legacy','HW3','HW4'][selectedOnboardingHw()]||'HW3';
  const speed=$('onboarding-speed')?.selectedOptions[0]?.textContent||'Auto';
  const ssid=$('onboarding-wifi-ssid')?.value.trim()||'Not configured';
  const tx=$('onboarding-can-tx')?.value||'firmware default',rx=$('onboarding-can-rx')?.value||'firmware default';
  const gate=$('onboarding-ap-gate')?.checked?'Enabled':'Disabled';
  if($('onboarding-review'))$('onboarding-review').textContent=`Hardware: ${hw}\nSpeed profile: ${speed}\nAP injection gate: ${gate}\nInternet WiFi: ${ssid}\nCAN GPIO: TX ${tx}, RX ${rx}\nInjection after setup: Stopped`;
}
function renderOnboarding(){
  document.querySelectorAll('.onboarding-step').forEach((element,index)=>element.classList.toggle('active',index===onboardingStep));
  $('onboarding-subtitle').textContent=`Step ${onboardingStep+1} of 6`;
  $('onboarding-progress').style.width=`${((onboardingStep+1)/6)*100}%`;
  $('onboarding-back').classList.toggle('hidden',onboardingStep===0||onboardingStep===5);
  $('onboarding-skip').classList.toggle('hidden',onboardingStep===5);
  $('onboarding-next').textContent=onboardingStep===4?'Save setup':onboardingStep===5?'Open dashboard':'Next';
  if(onboardingStep===4)updateOnboardingReview();
}
async function onboardingNext(){
  if(onboardingStep===0&&!$('onboarding-safety').checked)return show('Confirm the safety notice before continuing',true);
  if(onboardingStep===3){
    const txText=$('onboarding-can-tx').value.trim(),rxText=$('onboarding-can-rx').value.trim();
    if(txText||rxText){
      const tx=Number(txText),rx=Number(rxText);
      if(!Number.isInteger(tx)||!Number.isInteger(rx)||tx<0||rx<0||tx===rx)return show('Enter two different valid GPIO numbers',true);
    }
  }
  if(onboardingStep===4)return saveOnboarding();
  if(onboardingStep===5){closeOnboarding();return}
  onboardingStep++;renderOnboarding();
}
async function scanOnboardingWifi(){
  const button=$('onboarding-scan-btn');button.disabled=true;setStatus('onboarding-wifi-status','Scanning…');
  try{
    const data=await requestJson('/wifi_scan',{},10000);
    $('onboarding-wifi-scan').innerHTML=(data.networks||[]).map(network=>`<div class="scan-item" data-ssid="${escapeHtml(encodeURIComponent(network.ssid))}" onclick="selectOnboardingWifi(this.dataset.ssid)"><span>${escapeHtml(network.ssid)}</span><span>${network.rssi} dBm</span></div>`).join('');
    setStatus('onboarding-wifi-status',`${(data.networks||[]).length} network(s) found`);
  }catch(error){setStatus('onboarding-wifi-status',error.message,true)}finally{button.disabled=false}
}
function selectOnboardingWifi(ssid){$('onboarding-wifi-ssid').value=decodeURIComponent(ssid)}
async function saveOnboarding(){return locked('onboarding',async()=>{
  const next=$('onboarding-next');next.disabled=true;setStatus('onboarding-save-status','Saving setup…');
  try{
    const speed=$('onboarding-speed').value;
    const data={hw:String(selectedOnboardingHw()),spa:speed==='auto'?'1':'0',sp:speed==='auto'?String(config?.speedProfile??1):speed,plgr:String(config?.pluginReplay??1),apg:$('onboarding-ap-gate').checked?'1':'0',hw3OffsetSlew:config?.hw3OffsetSlew?'1':'0',hw3SlewRate:String(config?.hw3SlewRate??5)};
    await requestJson('/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form(data)});
    const ssid=$('onboarding-wifi-ssid').value.trim();
    if(ssid)await requestJson('/wifi_config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form({ssid,pass:$('onboarding-wifi-pass').value,static:'0',ip:'',gw:'',mask:'',dns:''})},7000);
    const txText=$('onboarding-can-tx').value.trim(),rxText=$('onboarding-can-rx').value.trim();
    if(txText&&rxText){
      const tx=Number(txText),rx=Number(rxText);
      if(tx!==onboardingCanOriginal.tx||rx!==onboardingCanOriginal.rx){
        await requestJson('/can_pins',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form({tx,rx})});
        onboardingNeedsReboot=true;
      }
    }
    localStorage.setItem(onboardingKey,'1');
    await Promise.allSettled([loadConfig(),loadWifi(),loadCanPins()]);
    $('onboarding-reboot-note').textContent=onboardingNeedsReboot?'CAN pins changed. Reboot the device before validating CAN traffic.':'No reboot is required for the selected settings.';
    setStatus('onboarding-save-status','Setup saved');onboardingStep=5;renderOnboarding();
  }catch(error){setStatus('onboarding-save-status',error.message,true)}finally{next.disabled=false}
})}
async function initOnboarding(){
  for(let attempt=0;attempt<50&&!config;attempt++)await new Promise(resolve=>setTimeout(resolve,100));
  syncOnboarding();openOnboarding(false);
}
function setConfigReady(ready){$('config-controls').querySelectorAll('button,input,select').forEach(e=>e.disabled=!ready)}setConfigReady(false);
function updateSpeedOptions(){const hw=Number($('cfg-hw').value);for(const o of $('cfg-speed').options)o.disabled=o.value!=='auto'&&Number(o.value)>2&&hw!==2;if($('cfg-speed').selectedOptions[0]?.disabled)$('cfg-speed').value='auto'}
function renderConfig(c){config=c;$('cfg-hw').value=String(c.hw);$('cfg-speed').value=c.speedAuto?'auto':String(c.speedProfile);$('plugin-replay').value=c.pluginReplay;$('plugin-replay').max=c.pluginReplayMax;$('led-brightness').value=c.ledBrightness;$('ap-gate-tgl').checked=!!c.apGate;$('hw3-slew').checked=!!c.hw3OffsetSlew;$('hw3-slew-rate').value=c.hw3SlewRate;updateSpeedOptions();setConfigReady(true);setStatus('config-status','Configuration loaded')}
async function loadConfig(){for(let attempt=0;attempt<5;attempt++){try{return renderConfig(await requestJson('/config'))}catch(e){setStatus('config-status',`Configuration retry ${attempt+1}/5`,true);await new Promise(r=>setTimeout(r,400*(attempt+1)))}}setStatus('config-status','Configuration unavailable; controls remain disabled',true)}
function updateApGateControl(d){if(config&&typeof d.apGate==='boolean'){config.apGate=d.apGate;$('ap-gate-tgl').checked=d.apGate}}
async function saveApGate(){return saveRuntimeConfig()}
async function saveRuntimeConfig(){if(!config)return;const speed=$('cfg-speed').value;const data={hw:$('cfg-hw').value,spa:speed==='auto'?'1':'0',sp:speed==='auto'?String(config.speedProfile):speed,plgr:$('plugin-replay').value,apg:$('ap-gate-tgl').checked?'1':'0',hw3OffsetSlew:$('hw3-slew').checked?'1':'0',hw3SlewRate:$('hw3-slew-rate').value};try{await requestJson('/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form(data)});await requestJson('/led_brightness',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form({b:$('led-brightness').value})});await loadConfig();show('Configuration saved')}catch(e){setStatus('config-status',e.message,true)}}
async function stopInjection(){if(!confirm('Stop CAN injection and persist the stopped state?'))return;try{await fetch('/disable',{method:'POST'});await loadConfig()}catch(e){show('Stop failed',true)}}async function resumeInjection(){try{await requestJson('/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'can=1'});await loadConfig()}catch(e){show(e.message,true)}}async function rebootDevice(){if(confirm('Reboot device?'))fetch('/reboot',{method:'POST'}).catch(()=>{})}
function renderProbe(p){if(!p||!p.active){$('probe-status').textContent='No injected frame yet';$('probe-status').className='value muted';$('probe-tx').textContent=$('probe-rx').textContent='—';return}const id='0x'+(p.id&0x7ff).toString(16).padStart(3,'0').toUpperCase()+(p.mux>=0?' mux '+p.mux:'');$('probe-tx-meta').textContent=`${id}, ${fmtAge(p.txa)} ago`;$('probe-tx').textContent=hexData(p.tx,p.txdlc);$('probe-rx-meta').textContent=p.hasrx?`${id}, ${fmtAge(p.rxa)} ago`:'not seen';$('probe-rx').textContent=p.hasrx?hexData(p.rx,p.rxdlc):'—';const states={1:['Waiting for matching frame','warn'],2:['Matching frame seen','ok'],3:['Bus frame differs from write','warn'],4:['Transmit failed','bad']};const s=states[p.state]||['Waiting','muted'];$('probe-status').textContent=s[0];$('probe-status').className='value '+s[1]}
const twaiNames=['stopped','running','bus-off','recovering'];function renderRuntime(d){const rt=d.runtime||{},driver=d.driver||{};const online=!!d.can,injecting=!!d.ia;$('dot').className='dot '+(online?'ok':'bad');$('header-status').textContent=online?(injecting?'CAN online · injection active':d.ready?'CAN online · monitoring':'CAN warm-up gate active'):'Waiting for CAN';$('s-can').textContent=online?'Online':'Offline';$('s-can').className='value '+(online?'ok':'bad');$('s-inj').textContent=injecting?'Active':(d.ready?'Stopped':'Gated');$('s-inj').className='value '+(injecting?'ok':'warn');$('s-rx').textContent=rt.canFrames??d.rx??0;$('s-age').textContent=fmtAge(rt.canAgeMs);$('s-tx').textContent=`${rt.txOk??d.tx??0} / ${rt.txFail??d.txerr??0}`;const code=Number(driver.stateCode);const name=twaiNames[code]||driver.state||'unknown';$('s-twai').textContent=name;$('s-twai').className='value '+(code===1?'ok':code===2?'bad':'warn');$('s-heap').textContent=fmtBytes(rt.freeHeap||0);$('s-up').textContent=Math.floor((rt.uptimeMs||d.up*1000||0)/1000)+' s';setStatus('runtime-note',d.ready?'Injection startup gates satisfied':`Waiting: ${rt.delayRemainingMs||0} ms delay, ${rt.canFrames||0}/${(rt.frameThreshold||1000)+1} CAN frames`);renderProbe(d.probe)}
async function pollRuntime(){return locked('runtime',async()=>{try{renderRuntime(await requestJson('/status',{},3000))}catch(e){$('dot').className='dot bad';$('header-status').textContent='Dashboard connection lost'}})}
function renderPlugins(){const el=$('plugin-list');if(!plugins.length){el.textContent='No plugins installed';return}el.innerHTML=plugins.map((p,i)=>`<div class="plugin"><div class="name"><b>${escapeHtml(p.name)}</b> <span class="muted">v${escapeHtml(p.version)}</span><small>${p.rules} rules · priority ${i+1}${p.author?' · '+escapeHtml(p.author):''}</small></div><button onclick="togglePlugin(${i})">${p.enabled?'Disable':'Enable'}</button><button onclick="movePlugin(${i},${Math.max(0,i-1)})" ${i===0?'disabled':''}>↑</button><button onclick="movePlugin(${i},${Math.min(plugins.length-1,i+1)})" ${i===plugins.length-1?'disabled':''}>↓</button><button class="danger" onclick="removePlugin(${i})">Remove</button></div>`).join('')}
function escapeHtml(s){return String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}async function loadPlugins(){return locked('plugins',async()=>{try{const d=await requestJson('/plugins',{},5000);plugins=d.plugins||[];renderPlugins()}catch(e){setStatus('plugin-status',e.message,true)}})}async function pluginAction(path,data){try{await requestJson(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form(data)},6000);await loadPlugins()}catch(e){setStatus('plugin-status',e.message,true)}}function togglePlugin(i){pluginAction('/plugin_toggle',{idx:i})}function movePlugin(i,p){if(i!==p)pluginAction('/plugin_priority',{idx:i,priority:p})}function removePlugin(i){if(confirm(`Remove ${plugins[i].name}?`))pluginAction('/plugin_remove',{idx:i})}
async function installPluginUrl(){try{await requestJson('/plugin_install',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form({url:$('plugin-url').value})},20000);await loadPlugins();show('Plugin installed')}catch(e){setStatus('plugin-status',e.message,true)}}async function installPluginJson(){let text=$('plugin-json').value.trim();const file=$('plugin-file').files[0];if(file)text=await file.text();try{JSON.parse(text);await requestJson('/plugin_upload',{method:'POST',headers:{'Content-Type':'application/json'},body:text},8000);await loadPlugins();show('Plugin installed')}catch(e){setStatus('plugin-status',e.message,true)}}
async function pollGvret(){return locked('gvret',async()=>{try{const d=await requestJson('/gvret/status');gvretWatching=!!(d.enabled||d.connected);$('gvret-session').textContent=d.connected?'Connected':d.enabled?'Armed':'Off';$('gvret-session').className='value '+(d.connected?'ok':d.enabled?'warn':'muted');$('gvret-frames').textContent=`${d.frames} / ${d.dropped}`;setStatus('gvret-status',d.connected?'Binary mode owns serial; text logs are routed away.':d.enabled?'Waiting for SavvyCAN handshake.':'GVRET is stopped.')}catch(e){setStatus('gvret-status',e.message,true)}})}async function startGvret(){try{await requestJson('/gvret/start',{method:'POST'});gvretWatching=true;pollGvret()}catch(e){show(e.message,true)}}async function stopGvret(){try{await requestJson('/gvret/stop',{method:'POST'});gvretWatching=false;pollGvret()}catch(e){show(e.message,true)}}
async function loadApStatus(){try{const d=await requestJson('/ap_status');$('ap-ssid').value=d.ssid||'';$('ap-hidden').checked=!!d.hidden;setStatus('ap-status',`${d.clients||0} client(s) · ${d.ip||''}`)}catch(e){setStatus('ap-status',e.message,true)}}async function saveAP(){const ssid=$('ap-ssid').value,pass=$('ap-pass').value;if(!ssid||pass&&pass.length<8||pass.length>63)return setStatus('ap-status','SSID required; password must be 8–63 characters',true);try{await requestJson('/ap_config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form({ssid,pass,hidden:$('ap-hidden').checked?'1':'0'})});setStatus('ap-status','Saved; reboot to apply')}catch(e){setStatus('ap-status',e.message,true)}}
function toggleStaticFields(){$('wifi-static-fields').classList.toggle('hidden',!$('wifi-static').checked)}async function loadWifi(){try{const [n,s]=await Promise.all([requestJson('/wifi_networks'),requestJson('/wifi_status')]);$('wifi-nets').innerHTML=(n.networks||[]).map(x=>`<div class="plugin"><div class="name"><b>${escapeHtml(x.ssid)}</b><small>${x.static?'static '+escapeHtml(x.ip||''):'DHCP'}${x.idx===n.active?' · active':''}</small></div><button class="danger" onclick="deleteWifi(${x.idx})">Remove</button></div>`).join('')||'No saved networks';setStatus('wifi-status',s.connected?`Connected to ${s.ssid} · ${s.ip}`:s.connecting?'Connecting…':'Not connected')}catch(e){setStatus('wifi-status',e.message,true)}}async function saveWifi(){const data={ssid:$('wifi-ssid').value,pass:$('wifi-pass').value,static:$('wifi-static').checked?'1':'0',ip:$('wifi-ip').value,gw:$('wifi-gw').value,mask:$('wifi-mask').value,dns:$('wifi-dns').value};try{await requestJson('/wifi_config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form(data)},7000);await loadWifi()}catch(e){setStatus('wifi-status',e.message,true)}}async function deleteWifi(i){try{await requestJson('/wifi_delete',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form({idx:i})});loadWifi()}catch(e){show(e.message,true)}}async function scanWifi(){setStatus('wifi-status','Scanning…');try{const d=await requestJson('/wifi_scan',{},10000);$('wifi-scan').innerHTML=(d.networks||[]).map(n=>`<div class="scan-item" data-ssid="${escapeHtml(encodeURIComponent(n.ssid))}" onclick="selectWifi(this.dataset.ssid)"><span>${escapeHtml(n.ssid)}</span><span>${n.rssi} dBm</span></div>`).join('');setStatus('wifi-status',`${(d.networks||[]).length} network(s) found`)}catch(e){setStatus('wifi-status',e.message,true)}}function selectWifi(s){$('wifi-ssid').value=decodeURIComponent(s)}
async function loadCanPins(){try{const d=await requestJson('/can_pins');$('can-tx').value=d.tx;$('can-rx').value=d.rx;setStatus('can-pins-status',d.customized?'Custom pins active':'Firmware defaults')}catch(e){setStatus('can-pins-status',e.message,true)}}async function saveCanPins(){if(!confirm('Save CAN pins and reboot? Incorrect pins disable CAN.'))return;try{await requestJson('/can_pins',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form({tx:$('can-tx').value,rx:$('can-rx').value})});rebootDevice()}catch(e){setStatus('can-pins-status',e.message,true)}}
function otaAuthorization(){if(!otaUser)otaUser=prompt('OTA username')||'';if(!otaPass)otaPass=prompt('OTA password')||'';if(!otaUser||!otaPass)return'';localStorage.setItem('otaU',otaUser);localStorage.setItem('otaP',otaPass);return'Basic '+btoa(otaUser+':'+otaPass)}function resetOtaCredentials(){otaUser=otaPass='';localStorage.removeItem('otaU');localStorage.removeItem('otaP');show('OTA credentials cleared')}
async function exportSettings(){const auth=otaAuthorization();if(!auth)return;try{const r=await fetch('/settings_export',{headers:{Authorization:auth}});if(!r.ok)throw new Error(`HTTP ${r.status}`);const blob=new Blob([await r.text()],{type:'application/json'}),a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='ev-open-can-settings.json';a.click();URL.revokeObjectURL(a.href)}catch(e){setStatus('backup-status',e.message,true)}}async function importSettings(){const file=$('settings-file').files[0],auth=otaAuthorization();if(!file||!auth)return;if(!confirm('Restore settings and reboot?'))return;try{const d=await requestJson('/settings_import',{method:'POST',headers:{'Content-Type':'application/json',Authorization:auth},body:await file.text()},8000);if(d.ok)rebootDevice()}catch(e){setStatus('backup-status',e.message,true)}}
async function loadUpdateInfo(){try{const [b,a]=await Promise.all([requestJson('/update_beta'),requestJson('/auto_update')]);$('beta-tgl').checked=!!b.beta;$('auto-upd-tgl').checked=!!a.enabled;$('fw-ver').textContent='v'+(b.version||'')}catch(e){setStatus('update-status',e.message,true)}}async function saveUpdateFlags(){try{await Promise.all([requestJson('/update_beta',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form({beta:$('beta-tgl').checked?'1':'0'})}),requestJson('/auto_update',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form({enabled:$('auto-upd-tgl').checked?'1':'0'})})])}catch(e){setStatus('update-status',e.message,true)}}async function checkUpdate(){setStatus('update-status','Checking…');try{const d=await requestJson('/update_check',{},15000);pendingUpdateUrl=d.update?d.url:'';$('install-update').classList.toggle('hidden',!d.update);setStatus('update-status',d.update?`Update ${d.latest} available (${d.artifact})`:`Up to date (${d.current})`)}catch(e){setStatus('update-status',e.message,true)}}async function installUpdate(){const auth=otaAuthorization();if(!auth||!pendingUpdateUrl||!confirm('Install update and reboot?'))return;try{await requestJson('/update_install',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded',Authorization:auth},body:form({url:pendingUpdateUrl})},180000);setStatus('update-status','Installed; rebooting…')}catch(e){setStatus('update-status',e.message,true)}}async function uploadFirmware(){const file=$('ota-file').files[0],auth=otaAuthorization();if(!file||!auth||!confirm('Upload firmware and reboot?'))return;setStatus('update-status','Uploading…');try{const body=new FormData();body.append('firmware',file);const r=await fetch('/update',{method:'POST',headers:{Authorization:auth,'X-File-Name':file.name,'X-File-Size':String(file.size)},body});if(!r.ok)throw new Error(`HTTP ${r.status}`);setStatus('update-status','Installed; rebooting…')}catch(e){setStatus('update-status',e.message,true)}}
function supportToggle(details){if(details.open&&!supportReport)loadSupport(false)}
async function loadSupport(){return locked('support',async()=>{const refresh=$('support-refresh'),copy=$('support-copy');refresh.disabled=copy.disabled=true;setStatus('support-status','Collecting diagnostics…');try{supportReport=await requestText('/support');$('support-report').textContent=supportReport;const match=supportReport.match(/^Overall: (OK|WARNING|ERROR)$/m),health=match?match[1]:'UNKNOWN';$('support-health').textContent=health.toLowerCase();$('support-health').className=health==='OK'?'ok':health==='ERROR'?'bad':'warn';setStatus('support-status',`Report loaded · ${supportReport.length} bytes`)}catch(e){supportReport='';$('support-report').textContent='Support report unavailable.';$('support-health').textContent='error';$('support-health').className='bad';setStatus('support-status',e.message,true)}finally{refresh.disabled=copy.disabled=false}})}
async function copySupport(){if(!supportReport)await loadSupport(false);if(!supportReport)return;let temp=null;try{if(navigator.clipboard&&window.isSecureContext)await navigator.clipboard.writeText(supportReport);else{temp=document.createElement('textarea');temp.value=supportReport;temp.style.position='fixed';temp.style.opacity='0';document.body.appendChild(temp);temp.select();if(!document.execCommand('copy'))throw new Error('Copy unavailable')}setStatus('support-status','Copied. Paste report into GitHub issue.')}catch(e){setStatus('support-status','Copy failed; select report text manually.',true)}finally{if(temp)temp.remove()}}
async function init(){await loadConfig();await Promise.allSettled([pollRuntime(),loadPlugins(),pollGvret(),loadApStatus(),loadWifi(),loadCanPins(),loadUpdateInfo()]);setInterval(pollRuntime,3000);setInterval(()=>{if(gvretWatching)pollGvret()},3000);setInterval(loadWifi,30000)}init();
window.addEventListener('hashchange',()=>{if(location.hash==='#/onboarding')openOnboarding(false)});
window.addEventListener('keydown',event=>{if(event.key==='Escape'&&!$('onboarding').classList.contains('hidden'))closeOnboarding()});
initOnboarding();
</script>
</body>
</html>
)HTML";
