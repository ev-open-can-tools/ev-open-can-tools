#!/usr/bin/env python3
from __future__ import annotations

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'include' / 'web' / 'mcp2515_dashboard_ui.src.h'
CHANGELOG = ROOT / 'CHANGELOG.md'
DOC = ROOT / 'docs' / 'onboarding.md'
TEST = ROOT / 'test' / 'test_dashboard_onboarding_regression.py'

CSS = r'''.onboarding{position:fixed;inset:0;z-index:20;background:rgba(0,0,0,.72);display:grid;place-items:center;padding:18px}
.onboarding-panel{width:min(680px,100%);max-height:92vh;overflow:auto;background:var(--card);border:1px solid var(--line);border-radius:12px;padding:18px;box-shadow:0 20px 70px rgba(0,0,0,.45)}
.onboarding-head{display:flex;gap:12px;align-items:flex-start}.onboarding-head h2{font-size:18px;margin:0}.onboarding-head .note{margin-top:3px}
.onboarding-progress{height:5px;background:var(--bg);border-radius:4px;overflow:hidden;margin:14px 0 18px}.onboarding-progress span{display:block;height:100%;width:0;background:var(--blue);transition:width .2s}
.onboarding-step{display:none}.onboarding-step.active{display:block}.onboarding-step h3{font-size:15px;margin:0 0 8px}.onboarding-step ul{padding-left:20px}
.onboarding-actions{display:flex;gap:8px;justify-content:flex-end;border-top:1px solid var(--line);margin-top:18px;padding-top:14px}.onboarding-actions #onboarding-skip{margin-right:auto}
.onboarding-review{white-space:pre-wrap;background:var(--bg);border:1px solid var(--line);border-radius:7px;padding:10px;font:12px/1.5 ui-monospace,monospace}
.onboarding-choice{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:10px}.onboarding-choice label{border:1px solid var(--line);border-radius:8px;padding:10px;cursor:pointer}.onboarding-choice input{margin-right:6px}
@media(max-width:560px){.onboarding-choice{grid-template-columns:1fr}.onboarding-actions{flex-wrap:wrap}.onboarding-actions #onboarding-skip{margin-right:0}.onboarding-actions button{flex:1}}'''

HEADER_OLD = '<header><span class="dot" id="dot"></span><div><h1>EV Open CAN</h1><div class="sub" id="header-status">Starting dashboard…</div></div><button style="margin-left:auto" onclick="toggleTheme()">Theme</button></header>'
HEADER_NEW = '<header><span class="dot" id="dot"></span><div><h1>EV Open CAN</h1><div class="sub" id="header-status">Starting dashboard…</div></div><button style="margin-left:auto" onclick="openOnboarding(true)">Setup</button><button onclick="toggleTheme()">Theme</button></header>'

MARKUP = r'''
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
</div>'''

STATE_ANCHOR = "const $=id=>document.getElementById(id);let config=null,plugins=[],pendingUpdateUrl='',supportReport='',gvretWatching=false,otaUser=localStorage.getItem('otaU')||'',otaPass=localStorage.getItem('otaP')||'';const locks={};"
STATE = """\nlet onboardingStep=0,onboardingCanOriginal={tx:null,rx:null},onboardingNeedsReboot=false;\nconst onboardingKey='evOpenCanOnboardingV1';"""

JS = r'''
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
'''

BOOTSTRAP = """window.addEventListener('hashchange',()=>{if(location.hash==='#/onboarding')openOnboarding(false)});\nwindow.addEventListener('keydown',event=>{if(event.key==='Escape'&&!$('onboarding').classList.contains('hidden'))closeOnboarding()});\ninitOnboarding();\n"""

DOC_CONTENT = '''# Dashboard onboarding

The dashboard includes a lightweight guided setup at `#/onboarding`. It opens automatically on the first visit in a browser and can always be reopened with the **Setup** button in the header.

The wizard configures the vehicle hardware generation, speed profile, AP injection gate, optional internet WiFi, and CAN GPIO pins. It reuses the existing dashboard endpoints and does not add a background task or additional polling.

## Safety behavior

The wizard never enables CAN injection. After setup, injection remains stopped until it is explicitly armed from the main dashboard after live CAN traffic has been validated. Changing CAN GPIO pins requires a device reboot.

The completion marker is stored in browser `localStorage`. This prevents repeated prompts in the same browser without adding another NVS write. Opening `#/onboarding` bypasses the marker. WiFi credentials are submitted directly to the device and are not stored in browser storage by the wizard.
'''

TEST_CONTENT = '''import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h").read_text(encoding="utf-8")


class DashboardOnboardingRegressionTest(unittest.TestCase):
    def test_onboarding_is_reopenable_and_first_visit_aware(self):
        self.assertIn("#/onboarding", SOURCE)
        self.assertIn("openOnboarding(true)", SOURCE)
        self.assertIn("evOpenCanOnboardingV1", SOURCE)

    def test_onboarding_reuses_existing_configuration_endpoints(self):
        for endpoint in ("/config", "/wifi_scan", "/wifi_config", "/can_pins"):
            self.assertIn(endpoint, SOURCE)

    def test_onboarding_does_not_arm_injection(self):
        match = re.search(r"async function saveOnboarding\(\).*?async function initOnboarding", SOURCE, re.DOTALL)
        self.assertIsNotNone(match)
        save_function = match.group(0)
        self.assertNotIn("can=1", save_function)
        self.assertNotIn("resumeInjection", save_function)
        self.assertIn("Injection after setup: Stopped", SOURCE)

    def test_wifi_password_is_not_written_to_browser_storage(self):
        self.assertNotRegex(SOURCE, r"localStorage\.setItem\([^,]+,\s*\$\('onboarding-wifi-pass'\)")


if __name__ == "__main__":
    unittest.main()
'''


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if text.count(old) != 1:
        raise RuntimeError(f'{label}: expected one anchor, found {text.count(old)}')
    return text.replace(old, new, 1)


def main() -> None:
    text = SOURCE.read_text(encoding='utf-8')
    if 'evOpenCanOnboardingV1' in text:
        print('onboarding already applied')
        return

    text = replace_once(text, '</style>', CSS + '\n</style>', 'style')
    text = replace_once(text, HEADER_OLD, HEADER_NEW + MARKUP, 'header')
    text = replace_once(text, STATE_ANCHOR, STATE_ANCHOR + STATE, 'state')
    text = replace_once(text, 'function setConfigReady', JS + 'function setConfigReady', 'javascript')
    text = replace_once(text, '</script>', BOOTSTRAP + '</script>', 'bootstrap')
    SOURCE.write_text(text, encoding='utf-8')

    changelog = CHANGELOG.read_text(encoding='utf-8')
    changelog = replace_once(
        changelog,
        '## [Unreleased]\n\nNo unreleased changes.',
        '## [Unreleased]\n\n### Added\n\n- Added a lightweight first-visit dashboard onboarding wizard for vehicle, safety gate, WiFi, and CAN pin setup. It reuses existing endpoints, can be reopened at `#/onboarding`, and never arms CAN injection automatically.',
        'changelog',
    )
    CHANGELOG.write_text(changelog, encoding='utf-8')

    DOC.write_text(DOC_CONTENT, encoding='utf-8')
    TEST.write_text(TEST_CONTENT, encoding='utf-8')

    subprocess.run(['python3', str(ROOT / 'scripts' / 'minify_dashboard.py')], cwd=ROOT, check=True)
    subprocess.run(['python3', '-m', 'unittest', 'discover', '-s', 'test', '-p', 'test_dashboard_onboarding_regression.py'], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
