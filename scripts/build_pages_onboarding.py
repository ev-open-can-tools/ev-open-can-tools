#!/usr/bin/env python3
"""Build the standalone interactive onboarding page for GitHub Pages."""

from __future__ import annotations

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUTPUT = ROOT / "onboarding" / "index.html"

PAGE = r'''<!doctype html>
<html lang="en" data-theme="dark">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="description" content="Interactive EV Open CAN onboarding preview">
<title>EV Open CAN onboarding preview</title>
<style>
:root{--bg:#0d0f12;--card:#171a20;--line:#2b3038;--text:#edf1f7;--muted:#8993a2;--blue:#5b8fff;--green:#3dba72;--yellow:#f5a623;--red:#ff5b5b}
[data-theme=light]{--bg:#f4f6f9;--card:#fff;--line:#d9dee7;--text:#151922;--muted:#687386}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px/1.5 system-ui,sans-serif}button,input,select{font:inherit}.banner{padding:10px 16px;background:#1d4ed8;color:#fff;text-align:center;font-size:12px;font-weight:650}header{border-bottom:1px solid var(--line);padding:14px 18px;display:flex;align-items:center;gap:12px}header h1{font-size:17px;margin:0}header p{color:var(--muted);font-size:12px;margin:2px 0 0}header button{margin-left:auto}main{max-width:760px;margin:auto;padding:20px}.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:20px;box-shadow:0 18px 55px rgba(0,0,0,.2)}.progress{height:6px;background:var(--bg);border-radius:9px;overflow:hidden;margin:14px 0 22px}.progress span{display:block;height:100%;background:var(--blue);transition:width .2s}.step{display:none}.step.active{display:block}.step h2{font-size:20px;margin:0 0 6px}.step h3{font-size:15px;margin:18px 0 8px}.muted{color:var(--muted)}.notice{border:1px solid var(--yellow);border-radius:8px;padding:11px;margin:14px 0}.choices{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin:14px 0}.choice{border:1px solid var(--line);border-radius:9px;padding:12px;cursor:pointer}.choice:has(input:checked){border-color:var(--blue);box-shadow:0 0 0 1px var(--blue)}.row{display:flex;gap:10px;align-items:end;flex-wrap:wrap;margin-top:12px}.field{display:flex;flex-direction:column;gap:5px;flex:1;min-width:160px}.field span{font-size:11px;color:var(--muted);text-transform:uppercase}input,select{width:100%;background:var(--bg);color:var(--text);border:1px solid var(--line);border-radius:7px;padding:9px}button,.button{border:1px solid var(--line);background:transparent;color:var(--text);border-radius:7px;padding:9px 12px;cursor:pointer;text-decoration:none}button.primary{background:var(--blue);border-color:var(--blue);color:#fff}button:disabled{opacity:.45;cursor:not-allowed}.actions{display:flex;gap:8px;border-top:1px solid var(--line);margin-top:22px;padding-top:16px}.actions .spacer{flex:1}.network{display:flex;justify-content:space-between;border-top:1px solid var(--line);padding:9px 2px;cursor:pointer}.network:hover{color:var(--blue)}pre{white-space:pre-wrap;overflow-wrap:anywhere;background:var(--bg);border:1px solid var(--line);border-radius:8px;padding:12px;font:12px/1.55 ui-monospace,monospace}.ok{color:var(--green)}.warn{color:var(--yellow)}.footer{text-align:center;color:var(--muted);font-size:11px;margin-top:14px}@media(max-width:580px){main{padding:12px}.card{padding:15px}.choices{grid-template-columns:1fr}.actions{flex-wrap:wrap}.actions .spacer{display:none}.actions button{flex:1}}
</style>
</head>
<body>
<div class="banner">Interactive GitHub Pages preview. All values are simulated and no ESP32 can be changed from this page.</div>
<header><div><h1>EV Open CAN</h1><p>Standalone onboarding preview</p></div><button onclick="toggleTheme()">Theme</button></header>
<main>
<div class="card">
<div class="muted" id="step-label">Step 1 of 6</div>
<div class="progress"><span id="progress"></span></div>

<section class="step active" data-step="0">
<h2>Welcome</h2>
<p>This preview demonstrates a possible guided setup flow without adding onboarding code to the ESP32 dashboard.</p>
<div class="notice"><strong>Safety notice</strong><br>This project is experimental, safety-critical research firmware. Validate hardware, wiring, traffic, counters, checksums, and every enabled rule before transmitting on a vehicle CAN bus.</div>
<label><input id="safety" type="checkbox"> I understand that this preview does not validate a real installation.</label>
</section>

<section class="step" data-step="1">
<h2>Vehicle profile</h2>
<p class="muted">Choose the simulated Tesla hardware generation and desired speed profile.</p>
<div class="choices">
<label class="choice"><input type="radio" name="hardware" value="Legacy"> Legacy</label>
<label class="choice"><input type="radio" name="hardware" value="HW3" checked> HW3</label>
<label class="choice"><input type="radio" name="hardware" value="HW4"> HW4</label>
</div>
<div class="row"><label class="field"><span>Speed profile</span><select id="speed"><option>Auto</option><option>Chill</option><option>Normal</option><option>Hurry</option><option data-hw4>Max</option><option data-hw4>Sloth</option></select></label></div>
<label><input id="ap-gate" type="checkbox" checked> Require the AP injection gate before plugin transmission</label>
</section>

<section class="step" data-step="2">
<h2>Internet WiFi</h2>
<p class="muted">Optional simulated uplink. Credentials entered here remain only in this browser tab and are never submitted.</p>
<div class="row"><label class="field"><span>SSID</span><input id="ssid" autocomplete="off"></label><label class="field"><span>Password</span><input id="wifi-password" type="password" autocomplete="new-password"></label><button onclick="scanNetworks()">Simulate scan</button></div>
<div id="networks"></div>
</section>

<section class="step" data-step="3">
<h2>CAN GPIO pins</h2>
<p class="muted">These are example values only. Use the correct defaults for the actual board and transceiver.</p>
<div class="row"><label class="field"><span>TX GPIO</span><input id="can-tx" type="number" min="0" value="5"></label><label class="field"><span>RX GPIO</span><input id="can-rx" type="number" min="0" value="4"></label></div>
<div class="notice">Changing CAN pins on a real device requires a reboot. Incorrect values can disable CAN communication.</div>
</section>

<section class="step" data-step="4">
<h2>Review</h2>
<p class="muted">No settings will be sent to hardware.</p>
<pre id="review"></pre>
</section>

<section class="step" data-step="5">
<h2 class="ok">Preview complete</h2>
<p>The simulated onboarding flow is complete. CAN injection remains stopped.</p>
<p class="muted">For a real device, connect to the ESP32 hotspot and use the existing local dashboard controls directly.</p>
<button onclick="restart()">Run preview again</button>
</section>

<div class="actions"><button id="back" onclick="goBack()">Back</button><div class="spacer"></div><a class="button" href="../docs/">Documentation</a><button id="next" class="primary" onclick="goNext()">Next</button></div>
</div>
<div class="footer">EV Open CAN GitHub Pages onboarding preview</div>
</main>
<script>
window.__EV_OPEN_CAN_PAGES_PREVIEW__ = true;
const simulatedNetworks=[{ssid:'Home WiFi',rssi:-42},{ssid:'Garage',rssi:-61},{ssid:'Phone hotspot',rssi:-73}];
let currentStep=0;
const $=id=>document.getElementById(id);
function selectedHardware(){return document.querySelector('input[name="hardware"]:checked').value}
function toggleTheme(){const root=document.documentElement;root.dataset.theme=root.dataset.theme==='dark'?'light':'dark'}
function updateHardwareOptions(){const hw4=selectedHardware()==='HW4';document.querySelectorAll('#speed option[data-hw4]').forEach(option=>option.disabled=!hw4);if($('speed').selectedOptions[0].disabled)$('speed').value='Auto'}
document.querySelectorAll('input[name="hardware"]').forEach(input=>input.addEventListener('change',updateHardwareOptions));
function scanNetworks(){$('networks').innerHTML=simulatedNetworks.map(network=>`<div class="network" data-ssid="${encodeURIComponent(network.ssid)}"><span>${network.ssid}</span><span>${network.rssi} dBm</span></div>`).join('');document.querySelectorAll('.network').forEach(element=>element.addEventListener('click',()=>{$('ssid').value=decodeURIComponent(element.dataset.ssid)}))}
function updateReview(){const ssid=$('ssid').value.trim()||'Not configured';$('review').textContent=`Hardware: ${selectedHardware()}\nSpeed profile: ${$('speed').value}\nAP injection gate: ${$('ap-gate').checked?'Enabled':'Disabled'}\nInternet WiFi: ${ssid}\nCAN GPIO: TX ${$('can-tx').value}, RX ${$('can-rx').value}\nInjection after setup: Stopped\nMode: GitHub Pages simulation only`}
function render(){document.querySelectorAll('.step').forEach((step,index)=>step.classList.toggle('active',index===currentStep));$('step-label').textContent=`Step ${currentStep+1} of 6`;$('progress').style.width=`${((currentStep+1)/6)*100}%`;$('back').style.visibility=currentStep===0||currentStep===5?'hidden':'visible';$('next').style.display=currentStep===5?'none':'inline-block';$('next').textContent=currentStep===4?'Complete preview':'Next';if(currentStep===4)updateReview()}
function validatePins(){const tx=Number($('can-tx').value),rx=Number($('can-rx').value);return Number.isInteger(tx)&&Number.isInteger(rx)&&tx>=0&&rx>=0&&tx!==rx}
function goNext(){if(currentStep===0&&!$('safety').checked){alert('Confirm the safety notice before continuing.');return}if(currentStep===3&&!validatePins()){alert('Enter two different non-negative GPIO numbers.');return}if(currentStep<5){currentStep++;render()}}
function goBack(){if(currentStep>0){currentStep--;render()}}
function restart(){currentStep=0;$('safety').checked=false;$('ssid').value='';$('wifi-password').value='';$('networks').innerHTML='';location.hash='#/onboarding';render()}
if(location.hash!=='#/onboarding')history.replaceState(null,'','#/onboarding');
updateHardwareOptions();render();
</script>
</body>
</html>
'''


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    output = args.output if args.output.is_absolute() else ROOT / args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(PAGE, encoding="utf-8")
    display = output.relative_to(ROOT) if output.is_relative_to(ROOT) else output
    print(f"GitHub Pages onboarding preview: {display}")


if __name__ == "__main__":
    main()
