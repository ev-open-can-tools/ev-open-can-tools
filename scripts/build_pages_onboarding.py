#!/usr/bin/env python3
"""Build an interactive GitHub Pages preview from the embedded dashboard source.

The generated page uses the exact dashboard HTML that is embedded in firmware, but
replaces device HTTP calls with a small in-browser simulation. This keeps the
public preview useful without pretending that GitHub Pages can configure an
HTTP-only ESP32 from an HTTPS origin.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h"
DEFAULT_OUTPUT = ROOT / "onboarding" / "index.html"

PREVIEW_STYLE = """
<style>
.preview-banner{position:sticky;top:0;z-index:30;padding:10px 16px;background:#1d4ed8;color:#fff;text-align:center;font:600 12px/1.4 system-ui,sans-serif}
.preview-banner a{color:#fff;text-decoration:underline}.preview-banner+header{top:37px}
</style>
"""

PREVIEW_BANNER = """
<div class="preview-banner" role="status">
  Interactive GitHub Pages preview. All device data and saved values are simulated.
  Connect to your ESP32 hotspot and open its dashboard to configure real hardware.
</div>
"""

MOCK_API = r"""
<script>
(() => {
  'use strict';
  window.__EV_OPEN_CAN_PAGES_PREVIEW__ = true;
  localStorage.removeItem('evOpenCanOnboardingV1');
  if (location.hash !== '#/onboarding') history.replaceState(null, '', '#/onboarding');

  const state = {
    config: {
      hw: 1,
      speedAuto: true,
      speedProfile: 1,
      pluginReplay: 1,
      pluginReplayMax: 20,
      ledBrightness: 32,
      apGate: true,
      hw3OffsetSlew: false,
      hw3SlewRate: 5
    },
    pins: {tx: 5, rx: 4, customized: false},
    networks: [],
    wifi: {connected: false, connecting: false, ssid: '', ip: ''}
  };

  const json = (value, status = 200) => new Response(JSON.stringify(value), {
    status,
    headers: {'Content-Type': 'application/json'}
  });
  const text = (value, status = 200) => new Response(value, {
    status,
    headers: {'Content-Type': 'text/plain; charset=utf-8'}
  });
  const fields = options => new URLSearchParams(
    typeof options?.body === 'string' ? options.body : ''
  );

  window.fetch = async (input, options = {}) => {
    const url = new URL(typeof input === 'string' ? input : input.url, location.href);
    const path = url.pathname.replace(/\/ev-open-can-tools(?=\/)/, '');
    const method = String(options.method || 'GET').toUpperCase();
    const body = fields(options);

    if (path === '/config') {
      if (method === 'POST') {
        if (body.has('hw')) state.config.hw = Number(body.get('hw'));
        if (body.has('spa')) state.config.speedAuto = body.get('spa') === '1';
        if (body.has('sp')) state.config.speedProfile = Number(body.get('sp'));
        if (body.has('plgr')) state.config.pluginReplay = Number(body.get('plgr'));
        if (body.has('apg')) state.config.apGate = body.get('apg') === '1';
        if (body.has('hw3OffsetSlew')) state.config.hw3OffsetSlew = body.get('hw3OffsetSlew') === '1';
        if (body.has('hw3SlewRate')) state.config.hw3SlewRate = Number(body.get('hw3SlewRate'));
      }
      return json({...state.config, ok: true});
    }
    if (path === '/status') return json({
      can: true,
      ia: false,
      ready: true,
      runtime: {
        canFrames: 42831,
        canAgeMs: 18,
        txOk: 0,
        txFail: 0,
        freeHeap: 184320,
        uptimeMs: 682000,
        delayRemainingMs: 0,
        frameThreshold: 1000
      },
      driver: {stateCode: 1, state: 'running'},
      probe: {active: false}
    });
    if (path === '/plugins') return json({plugins: []});
    if (path === '/gvret/status') return json({enabled: false, connected: false, frames: 0, dropped: 0});
    if (path === '/ap_status') return json({ssid: 'EV-Open-CAN', hidden: false, clients: 1, ip: '192.168.4.1'});
    if (path === '/wifi_networks') return json({networks: state.networks, active: state.networks.length ? 0 : -1});
    if (path === '/wifi_status') return json(state.wifi);
    if (path === '/wifi_scan') return json({networks: [
      {ssid: 'Home WiFi', rssi: -42},
      {ssid: 'Garage', rssi: -61},
      {ssid: 'Phone hotspot', rssi: -73}
    ]});
    if (path === '/wifi_config' && method === 'POST') {
      const ssid = body.get('ssid') || '';
      if (ssid) {
        state.networks = [{idx: 0, ssid, static: false, ip: ''}];
        state.wifi = {connected: true, connecting: false, ssid, ip: '192.168.1.73'};
      }
      return json({ok: true});
    }
    if (path === '/wifi_delete' && method === 'POST') {
      state.networks = [];
      state.wifi = {connected: false, connecting: false, ssid: '', ip: ''};
      return json({ok: true});
    }
    if (path === '/can_pins') {
      if (method === 'POST') {
        state.pins = {tx: Number(body.get('tx')), rx: Number(body.get('rx')), customized: true};
      }
      return json({...state.pins, ok: true});
    }
    if (path === '/update_beta') return json({beta: false, version: 'preview'});
    if (path === '/auto_update') return json({enabled: false});
    if (path === '/update_check') return json({update: false, current: 'preview'});
    if (path === '/support') return text(
      'EV Open CAN support report\nOverall: OK\nMode: GitHub Pages simulation\nNo real device is connected.\n'
    );
    if (path === '/settings_export') return text('{}');
    return json({ok: true, preview: true});
  };
})();
</script>
"""


def extract_dashboard(source: str) -> str:
    match = re.search(r'R"HTML\((.*)\)HTML";', source, re.DOTALL)
    if not match:
        raise ValueError("No embedded dashboard HTML payload found")
    return match.group(1)


def build_page(html: str) -> str:
    html = html.replace("<title>EV Open CAN</title>", "<title>EV Open CAN onboarding preview</title>", 1)
    html = html.replace("</head>", PREVIEW_STYLE + "\n</head>", 1)
    html = html.replace("<body>", "<body>\n" + PREVIEW_BANNER, 1)
    html = html.replace("<script>", MOCK_API + "\n<script>", 1)
    return html


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    source = SOURCE.read_text(encoding="utf-8")
    output = args.output if args.output.is_absolute() else ROOT / args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(build_page(extract_dashboard(source)), encoding="utf-8")
    print(f"GitHub Pages onboarding preview: {output.relative_to(ROOT) if output.is_relative_to(ROOT) else output}")


if __name__ == "__main__":
    main()
