#pragma once

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD)

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include "handlers.h"
#include "can_helpers.h"
#include "plugin_engine.h"
#if defined(DRIVER_ESP32_EXT_MCP2515)
#include "drivers/esp32_mcp2515_driver.h"
#endif
#include "web/mcp2515_dashboard_ui.h"

#ifndef DASH_SSID
#error "Define -DDASH_SSID in build_flags (e.g. -DDASH_SSID=\\\"ADUnlock-1234\\\")"
#endif
#ifndef DASH_PASS
#error "Define -DDASH_PASS in build_flags (min 8 chars)"
#endif
#ifndef DASH_OTA_PASS
#error "Define -DDASH_OTA_PASS in build_flags"
#endif
#ifndef DASH_OTA_USER
#error "Define -DDASH_OTA_USER in build_flags"
#endif

#ifndef DASH_DEFAULT_HW
#define DASH_DEFAULT_HW 1
#endif

#if DASH_DEFAULT_HW < 0 || DASH_DEFAULT_HW > 2
#error "DASH_DEFAULT_HW must be 0 (LEGACY), 1 (HW3), or 2 (HW4)"
#endif

#define PREFS_NS "ADunlock"

static Preferences prefs;

struct Features
{
    bool ADEnabled = true;
    bool nagSuppress = true;
    bool summonUnlock = true;
    bool isaSuppress = false;
    bool evDetection = false;
    uint8_t hw4Offset = 0;
};

static Features feat;

static CarManagerBase *dashHandler = nullptr;
static CanDriver *dashDriver = nullptr;
#if defined(DRIVER_ESP32_EXT_MCP2515)
static MCP2515 *dashMcp = nullptr;
#endif

static unsigned long rxCount = 0;
static unsigned long txCount = 0;
static unsigned long txErrCount = 0;
static unsigned long lastFrameMs = 0;
static unsigned long startMs = 0;
static bool canOnline = false;
static uint8_t followDist = 0;

static unsigned long fpsFrames = 0;
static unsigned long fpsLastMs = 0;
static float fps = 0.0f;

static unsigned long muxRx[4] = {};
static unsigned long muxTx[4] = {};
static unsigned long muxErr[4] = {};

#if defined(DRIVER_ESP32_EXT_MCP2515)
static uint8_t mcpEflg = 0;
#else
static const uint8_t mcpEflg = 0;
#endif

static uint8_t hwMode = DASH_DEFAULT_HW;
static bool canActive = true;
static bool bypassTlssc = false;

// WiFi AP (hotspot) — overridable at runtime
static char apSSID[33] = "";
static char apPass[65] = "";
static bool apHidden = false; // when true, SSID is not broadcast (hidden AP)

// WiFi STA (client) mode for internet access
static char staSSID[33] = "";
static char staPass[65] = "";
static bool staConnected = false;
static bool staStaticIP = false;
static bool updateBetaChannel = false;
static bool autoUpdateEnabled = false;
static bool autoUpdateDone = false;          // one-shot per boot
static unsigned long autoUpdateEligibleAt = 0; // millis() at which auto-check may fire
static IPAddress staIP(0, 0, 0, 0);
static IPAddress staGW(0, 0, 0, 0);
static IPAddress staMask(255, 255, 255, 0);
static IPAddress staDNS(0, 0, 0, 0);

static void dashSwapHandler(uint8_t mode);
static void dashApplyFilters();
static void dashReapplyFiltersWithPlugins();

// CAN recorder
#define REC_CAP 2000
struct RecFrame
{
    unsigned long ts;
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
};
static RecFrame recBuf[REC_CAP];
static volatile bool recActive = false;
static volatile int recCount = 0;
static bool recSaved = false;

// CAN sniffer ring buffer
#define SNIFFER_CAP 30
struct SniffFrame
{
    unsigned long ts;
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
};
static SniffFrame sniffBuf[SNIFFER_CAP];
static int sniffHead = 0;
static int sniffCount = 0;

static const char *decodeCanId(uint32_t id)
{
    switch (id)
    {
    case 0x045:
        return "STW_ACTN_RQ";
    case 0x129:
        return "Steering angle";
    case 0x175:
        return "Speed";
    case 0x186:
        return "Gear/Drive state";
    case 0x257:
        return "State of charge";
    case 0x293:
        return "DAS control";
    case 0x321:
        return "Autopilot state";
    case 0x329:
        return "UI_autopilot";
    case 0x399:
        return "DAS_status";
    case 0x3E8:
        return "UI_driverAssistControl";
    case 0x3FD:
        return "UI_autopilotControl";
    default:
        return "";
    }
}

static void sniffPush(const CanFrame &f)
{
    sniffBuf[sniffHead] = {millis(), f.id, f.dlc, {}};
    memcpy(sniffBuf[sniffHead].data, f.data, f.dlc);
    sniffHead = (sniffHead + 1) % SNIFFER_CAP;
    if (sniffCount < SNIFFER_CAP)
        sniffCount++;
}

#define LOG_CAP 80
struct LogEntry
{
    String msg;
    unsigned long seq;
};
static LogEntry logBuf[LOG_CAP];
static int logHead = 0;
static int logCount = 0;
static unsigned long logSeq = 0;

static void dashLog(const String &s)
{
    logBuf[logHead] = {String(millis() / 1000) + "s " + s, ++logSeq};
    logHead = (logHead + 1) % LOG_CAP;
    if (logCount < LOG_CAP)
        logCount++;
    Serial.println(s);
}

// Public hooks
static void mcpDashOnFrame(const CanFrame &f)
{
    rxCount++;
    lastFrameMs = millis();
    canOnline = true;
    fpsFrames++;
    sniffPush(f);
    if (f.id == 1021)
    {
        uint8_t m = f.data[0] & 0x07;
        if (m < 4)
            muxRx[m]++;
    }
    if (f.id == 1016)
        followDist = (f.data[5] & 0xE0) >> 5;
    if (recActive)
    {
        int idx = recCount;
        if (idx < REC_CAP)
        {
            recBuf[idx].ts = millis();
            recBuf[idx].id = f.id;
            recBuf[idx].dlc = f.dlc;
            memcpy(recBuf[idx].data, f.data, f.dlc);
            recCount = idx + 1;
            if (recCount >= REC_CAP)
                recActive = false;
        }
    }
}

static void mcpDashOnSend(uint8_t mux, bool ok)
{
    txCount++;
    if (!ok)
    {
        txErrCount++;
        if (mux < 4)
            muxErr[mux]++;
    }
    else
    {
        if (mux < 4)
            muxTx[mux]++;
    }
}

// JSON escape for log strings
static String jsonEscape(const String &s)
{
    String out;
    out.reserve(s.length() + 8);
    for (unsigned int i = 0; i < s.length(); i++)
    {
        char c = s.charAt(i);
        if (c == '"')
            out += "\\\"";
        else if (c == '\\')
            out += "\\\\";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            out += "\\r";
        else if (c < 0x20)
            out += ' ';
        else
            out += c;
    }
    return out;
}

// Store config
static void dashSavePrefs()
{
    prefs.begin(PREFS_NS, false);
    prefs.putUChar("hw", hwMode);
    prefs.putUChar("sp", dashHandler ? (int)dashHandler->speedProfile : 1);
    prefs.putBool("can", canActive);
    prefs.putBool("fAD", bypassTlssc);
    prefs.putBool("eprn", dashHandler ? (bool)dashHandler->enablePrint : true);
    prefs.putBool("f_AD", feat.ADEnabled);
    prefs.putBool("f_nag", feat.nagSuppress);
    prefs.putBool("f_sum", feat.summonUnlock);
    prefs.putBool("f_isa", feat.isaSuppress);
    prefs.putBool("f_evd", feat.evDetection);
    prefs.putUChar("f_h4o", feat.hw4Offset);
    prefs.putBool("sp_lock", (bool)speedProfileLocked);
    prefs.end();
}

static void dashLoadPrefs()
{
    prefs.begin(PREFS_NS, false);
    hwMode = prefs.getUChar("hw", DASH_DEFAULT_HW);
    bool canActiveLoaded = prefs.getBool("can", true);
    bypassTlssc = prefs.getBool("fAD", false);
    feat.ADEnabled = prefs.getBool("f_AD", true);
    feat.nagSuppress = prefs.getBool("f_nag", true);
    feat.summonUnlock = prefs.getBool("f_sum", true);
    enhancedAutopilotRuntime = feat.nagSuppress || feat.summonUnlock;
    feat.isaSuppress = prefs.getBool("f_isa", false);
    feat.evDetection = prefs.getBool("f_evd", false);
    feat.hw4Offset = prefs.getUChar("f_h4o", 0);
    speedProfileLocked = prefs.getBool("sp_lock", false);
    uint8_t sp = prefs.getUChar("sp", 1);
    bool ep = prefs.getBool("eprn", true);
    String wifiSsid = prefs.getString("wifi_ssid", "");
    String wifiPass = prefs.getString("wifi_pass", "");
    prefs.end();

    canActive = true;
    if (!canActiveLoaded)
        dashLog("[WARN] canActive was OFF (prior emergency stop) -- auto-reset to ON");

    bypassTlsscRequirementRuntime = bypassTlssc;
    emergencyVehicleDetectionRuntime = feat.evDetection;
    isaSpeedChimeSuppressRuntime = feat.isaSuppress;
    hw4OffsetRuntime = feat.hw4Offset;
    if (dashHandler)
    {
        dashHandler->speedProfile = sp;
        dashHandler->enablePrint = ep;
    }
    // Load WiFi AP overrides (hotspot name/password)
    String apSsidPref = prefs.getString("ap_ssid", "");
    String apPassPref = prefs.getString("ap_pass", "");
    if (apSsidPref.length() > 0)
        strlcpy(apSSID, apSsidPref.c_str(), sizeof(apSSID));
    else
        strlcpy(apSSID, DASH_SSID, sizeof(apSSID));
    if (apPassPref.length() > 0)
        strlcpy(apPass, apPassPref.c_str(), sizeof(apPass));
    else
        strlcpy(apPass, DASH_PASS, sizeof(apPass));
    apHidden = prefs.getBool("ap_hidden", false);

    // Load WiFi STA credentials
    String wifiSsid = prefs.getString("wifi_ssid", "");
    String wifiPass = prefs.getString("wifi_pass", "");
    strlcpy(staSSID, wifiSsid.c_str(), sizeof(staSSID));
    strlcpy(staPass, wifiPass.c_str(), sizeof(staPass));
    staStaticIP = prefs.getBool("wifi_static", false);
    if (staStaticIP)
    {
        staIP.fromString(prefs.getString("wifi_ip", "0.0.0.0"));
        staGW.fromString(prefs.getString("wifi_gw", "0.0.0.0"));
        staMask.fromString(prefs.getString("wifi_mask", "255.255.255.0"));
        staDNS.fromString(prefs.getString("wifi_dns", "0.0.0.0"));
    }

    updateBetaChannel = prefs.getBool("update_beta", false);
    autoUpdateEnabled = prefs.getBool("auto_upd", false);

    dashLog("[BOOT] Prefs loaded HW=" + String(hwMode) + " SP=" + String(sp));
    dashLog("[BOOT] canActive=YES bypassTlssc=" + String(bypassTlssc ? "YES" : "NO"));
    dashLog("[BOOT] feat: AD=" + String(feat.ADEnabled ? "ON" : "OFF") +
            " nag=" + String(feat.nagSuppress ? "ON" : "OFF") +
            " summon=" + String(feat.summonUnlock ? "ON" : "OFF") +
            " isa=" + String(feat.isaSuppress ? "ON" : "OFF") +
            " evd=" + String(feat.evDetection ? "ON" : "OFF"));
}

// MCP2515-only: fine-grained filter register reload on HW mode switch.
// Other builds use dashDriver->setFilters() in dashSwapHandler instead.
static void dashApplyFilters()
{
#if defined(DRIVER_ESP32_EXT_MCP2515)
    if (!dashMcp)
        return;
    dashMcp->setConfigMode();
    if (hwMode == 0)
    {
        dashMcp->setFilterMask(MCP2515::MASK0, false, 0x7FF);
        dashMcp->setFilter(MCP2515::RXF0, false, 69);
        dashMcp->setFilter(MCP2515::RXF1, false, 1006);
        dashMcp->setFilterMask(MCP2515::MASK1, false, 0x7FF);
        dashMcp->setFilter(MCP2515::RXF2, false, 69);
        dashMcp->setFilter(MCP2515::RXF3, false, 1006);
        dashMcp->setFilter(MCP2515::RXF4, false, 69);
        dashMcp->setFilter(MCP2515::RXF5, false, 1006);
    }
    else if (hwMode == 2)
    {
        dashMcp->setFilterMask(MCP2515::MASK0, false, 0x7FF);
        dashMcp->setFilter(MCP2515::RXF0, false, 921);
        dashMcp->setFilter(MCP2515::RXF1, false, 1021);
        dashMcp->setFilterMask(MCP2515::MASK1, false, 0x7FF);
        dashMcp->setFilter(MCP2515::RXF2, false, 1016);
        dashMcp->setFilter(MCP2515::RXF3, false, 1021);
        dashMcp->setFilter(MCP2515::RXF4, false, 1016);
        dashMcp->setFilter(MCP2515::RXF5, false, 921);
    }
    else
    {
        dashMcp->setFilterMask(MCP2515::MASK0, false, 0x7FF);
        dashMcp->setFilter(MCP2515::RXF0, false, 1016);
        dashMcp->setFilter(MCP2515::RXF1, false, 1021);
        dashMcp->setFilterMask(MCP2515::MASK1, false, 0x7FF);
        dashMcp->setFilter(MCP2515::RXF2, false, 1016);
        dashMcp->setFilter(MCP2515::RXF3, false, 1021);
        dashMcp->setFilter(MCP2515::RXF4, false, 1016);
        dashMcp->setFilter(MCP2515::RXF5, false, 1021);
    }
    dashMcp->setNormalMode();
    dashLog("[CFG] Filters set for " + String(hwMode == 0 ? "LEGACY" : hwMode == 1 ? "HW3"
                                                                                   : "HW4"));
#endif
}

// Bus-off recovery (MCP2515 only — TWAI driver handles its own bus-off internally)
#if defined(DRIVER_ESP32_EXT_MCP2515)
static unsigned long lastEflgCheckMs = 0;
static void dashCheckBusHealth()
{
    if (!dashMcp)
        return;
    if (millis() - lastEflgCheckMs < 5000)
        return;
    lastEflgCheckMs = millis();
    uint8_t eflg = dashMcp->getErrorFlags();
    mcpEflg = eflg;
    if (eflg & 0x20)
    {
        dashLog("[ERR] MCP2515 BUS-OFF -- recovering");
        dashMcp->reset();
        delay(10);
        dashMcp->setBitrate(CAN_500KBPS, MCP_CRYSTAL_FREQ);
        dashApplyFilters();
        dashLog("[OK] MCP2515 recovered");
    }
}
#else
static void dashCheckBusHealth()
{
}
#endif
static WebServer server(80);

static void handleRoot()
{
    server.send_P(200, "text/html", DASH_HTML);
}

static void handleStatus()
{
    if (canOnline && millis() - lastFrameMs > 10000)
    {
        canOnline = false;
        dashLog("[CAN] Bus OFFLINE (timeout)");
    }
    unsigned long now = millis();
    if (now - fpsLastMs >= 2000)
    {
        fps = fpsFrames * 1000.0f / max(1UL, now - fpsLastMs);
        fpsFrames = 0;
        fpsLastMs = now;
    }

    bool ADActive = dashHandler ? (bool)dashHandler->ADEnabled : false;
    int sp = dashHandler ? (int)dashHandler->speedProfile : 0;
    int soff = dashHandler ? (int)dashHandler->speedOffset : 0;
    int gtwAp = dashHandler ? (int)dashHandler->gatewayAutopilot : -1;
    bool ep = dashHandler ? (bool)dashHandler->enablePrint : true;

    String j = "{\"hw\":";
    j += hwMode;
    j += ",\"sp\":";
    j += sp;
    j += ",\"soff\":";
    j += soff;
    j += ",\"gtwap\":";
    j += gtwAp;
    j += ",\"AD\":";
    j += ADActive ? "true" : "false";
    j += ",\"fAD\":";
    j += bypassTlssc ? "true" : "false";
    j += ",\"eprn\":";
    j += ep ? "true" : "false";
    j += ",\"can\":";
    j += canOnline ? "true" : "false";
    j += ",\"ci\":";
    j += canActive ? "true" : "false";
    j += ",\"rx\":";
    j += rxCount;
    j += ",\"tx\":";
    j += txCount;
    j += ",\"txerr\":";
    j += txErrCount;
    j += ",\"fd\":";
    j += followDist;
    j += ",\"fps\":";
    j += String(fps, 1);
    j += ",\"eflg\":";
    j += mcpEflg;
    j += ",\"up\":";
    j += (millis() - startMs) / 1000;
    j += ",\"feat\":{\"AD\":";
    j += feat.ADEnabled ? "true" : "false";
    j += ",\"nag\":";
    j += feat.nagSuppress ? "true" : "false";
    j += ",\"summon\":";
    j += feat.summonUnlock ? "true" : "false";
    j += ",\"isa\":";
    j += feat.isaSuppress ? "true" : "false";
    j += ",\"evd\":";
    j += feat.evDetection ? "true" : "false";
    j += ",\"h4o\":";
    j += feat.hw4Offset;
    j += ",\"spl\":";
    j += (bool)speedProfileLocked ? "true" : "false";
    j += "},\"mux\":[";
    for (int i = 0; i < 3; i++)
    {
        if (i)
            j += ",";
        j += "{\"rx\":" + String(muxRx[i]) +
             ",\"tx\":" + String(muxTx[i]) +
             ",\"err\":" + String(muxErr[i]) + "}";
    }
    j += "]}";
    server.send(200, "application/json", j);
}

static void handleConfig()
{
    bool hwChanged = false;
    if (server.hasArg("hw"))
    {
        uint8_t v = server.arg("hw").toInt();
        if (v <= 2 && v != hwMode)
        {
            hwMode = v;
            hwChanged = true;
            dashLog("[CFG] HW=" + String(v == 0 ? "LEGACY" : v == 1 ? "HW3"
                                                                    : "HW4"));
        }
    }
    if (server.hasArg("sp") && dashHandler)
    {
        uint8_t v = server.arg("sp").toInt();
        if (v <= 4)
        {
            dashHandler->speedProfile = v;
            dashLog("[CFG] Profile=" + String(v));
        }
    }
    if (server.hasArg("spl"))
    {
        speedProfileLocked = server.arg("spl") == "1";
        dashLog("[CFG] Profile lock " + String((bool)speedProfileLocked ? "ON" : "OFF"));
    }
    if (server.hasArg("can"))
        canActive = server.arg("can") == "1";
    if (hwChanged)
    {
        dashSwapHandler(hwMode);
        dashApplyFilters();
    }
    dashSavePrefs();
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleFeatures()
{
    if (server.hasArg("AD"))
    {
        feat.ADEnabled = server.arg("AD") == "1";
        dashLog("[FEAT] AD " + String(feat.ADEnabled ? "ON" : "OFF"));
    }
    if (server.hasArg("nag"))
    {
        feat.nagSuppress = server.arg("nag") == "1";
        enhancedAutopilotRuntime = feat.nagSuppress || feat.summonUnlock;
        dashLog("[FEAT] Nag suppress " + String(feat.nagSuppress ? "ON" : "OFF"));
    }
    if (server.hasArg("summon"))
    {
        feat.summonUnlock = server.arg("summon") == "1";
        enhancedAutopilotRuntime = feat.nagSuppress || feat.summonUnlock;
        dashLog("[FEAT] Summon unlock " + String(feat.summonUnlock ? "ON" : "OFF"));
    }
    if (server.hasArg("isa"))
    {
        feat.isaSuppress = server.arg("isa") == "1";
        isaSpeedChimeSuppressRuntime = feat.isaSuppress;
        dashLog("[FEAT] ISA suppress " + String(feat.isaSuppress ? "ON" : "OFF"));
    }
    if (server.hasArg("evd"))
    {
        feat.evDetection = server.arg("evd") == "1";
        emergencyVehicleDetectionRuntime = feat.evDetection;
        dashLog("[FEAT] EV detection " + String(feat.evDetection ? "ON" : "OFF"));
    }
    if (server.hasArg("h4o"))
    {
        uint8_t v = (uint8_t)constrain(server.arg("h4o").toInt(), 0, 63);
        feat.hw4Offset = v;
        hw4OffsetRuntime = v;
        dashLog("[FEAT] HW4 offset raw=" + String(v) + (v == 0 ? " (off)" : ""));
    }
    if (server.hasArg("fAD"))
    {
        bypassTlssc = server.arg("fAD") == "1";
        bypassTlsscRequirementRuntime = bypassTlssc;
        dashLog("[FEAT] Bypass TLSSC " + String(bypassTlssc ? "ON" : "OFF"));
    }
    if (server.hasArg("eprn") && dashHandler)
    {
        bool ep = server.arg("eprn") == "1";
        dashHandler->enablePrint = ep;
        dashLog("[FEAT] Logging " + String(ep ? "ON" : "OFF"));
    }
    dashSavePrefs();
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleFrames()
{
    String j = "{\"frames\":[";
    int start = (sniffCount < SNIFFER_CAP) ? 0 : sniffHead;
    int count = min(sniffCount, SNIFFER_CAP);
    for (int i = 0; i < count; i++)
    {
        int idx = (start + i) % SNIFFER_CAP;
        SniffFrame &f = sniffBuf[idx];
        if (i)
            j += ",";
        j += "{\"ts\":" + String(f.ts) +
             ",\"id\":" + String(f.id) +
             ",\"dlc\":" + String(f.dlc) +
             ",\"data\":[";
        for (int b = 0; b < f.dlc; b++)
        {
            if (b)
                j += ",";
            j += String(f.data[b]);
        }
        j += "],\"name\":\"" + jsonEscape(decodeCanId(f.id)) + "\"}";
    }
    j += "]}";
    server.send(200, "application/json", j);
}

static void handleLog()
{
    unsigned long since = 0;
    if (server.hasArg("since"))
        since = strtoul(server.arg("since").c_str(), nullptr, 10);
    String j = "{\"seq\":";
    j += logSeq;
    j += ",\"lines\":[";
    int start = (logCount < LOG_CAP) ? 0 : logHead;
    int count = min(logCount, LOG_CAP);
    bool first = true;
    for (int i = 0; i < count; i++)
    {
        int idx = (start + i) % LOG_CAP;
        if (logBuf[idx].seq <= since)
            continue;
        if (!first)
            j += ",";
        first = false;
        j += "\"" + jsonEscape(logBuf[idx].msg) + "\"";
    }
    j += "]}";
    server.send(200, "application/json", j);
}

static void handleResetStats()
{
    rxCount = 0;
    txCount = 0;
    txErrCount = 0;
    memset(muxRx, 0, sizeof(muxRx));
    memset(muxTx, 0, sizeof(muxTx));
    memset(muxErr, 0, sizeof(muxErr));
    dashLog("[CFG] Stats reset");
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleRecStart()
{
    recCount = 0;
    recSaved = false;
    recActive = true;
    dashLog("[REC] Recording started");
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleRecStop()
{
    recActive = false;
    int n = recCount;
    File f = SPIFFS.open("/rec.csv", "w");
    if (f)
    {
        f.println("ts_ms,id,dlc,b0,b1,b2,b3,b4,b5,b6,b7");
        for (int i = 0; i < n; i++)
        {
            f.print(recBuf[i].ts);
            f.print(',');
            f.print(recBuf[i].id);
            f.print(',');
            f.print(recBuf[i].dlc);
            for (int b = 0; b < 8; b++)
            {
                f.print(',');
                f.print(recBuf[i].data[b]);
            }
            f.println();
        }
        f.close();
        recSaved = true;
        dashLog("[REC] Saved " + String(n) + " frames to SPIFFS");
    }
    else
    {
        dashLog("[REC] SPIFFS write failed");
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleRecStatus()
{
    String j = "{\"active\":";
    j += recActive ? "true" : "false";
    j += ",\"count\":";
    j += recCount;
    j += ",\"cap\":";
    j += REC_CAP;
    j += ",\"saved\":";
    j += recSaved ? "true" : "false";
    j += "}";
    server.send(200, "application/json", j);
}

static void handleRecDownload()
{
    if (!SPIFFS.exists("/rec.csv"))
    {
        server.send(404, "text/plain", "No recording saved yet");
        return;
    }
    File f = SPIFFS.open("/rec.csv", "r");
    server.sendHeader("Content-Disposition", "attachment; filename=\"can_recording.csv\"");
    server.streamFile(f, "text/csv");
    f.close();
}

static void handleDisable()
{
    feat.ADEnabled = false;
    feat.nagSuppress = false;
    feat.summonUnlock = false;
    feat.isaSuppress = false;
    feat.evDetection = false;
    canActive = false;
    if (dashHandler)
        dashHandler->ADEnabled = false;
    dashSavePrefs();
    dashLog("[CFG] EMERGENCY STOP -- all features disabled");
    server.send(200, "text/plain", "All injection disabled.");
}

static void handleReboot()
{
    server.send(200, "text/plain", "Rebooting...");
    delay(200);
    ESP.restart();
}

static void handleOtaResult()
{
    if (!server.authenticate(DASH_OTA_USER, DASH_OTA_PASS))
    {
        server.requestAuthentication();
        return;
    }
    bool ok = !Update.hasError();
    server.sendHeader("Connection", "close");
    server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "FAIL");
    if (ok)
    {
        dashLog("[OTA] Upload complete -- rebooting");
        delay(300);
        ESP.restart();
    }
    else
    {
        dashLog("[OTA] Upload FAILED");
    }
}

static void handleOtaUpload()
{
    if (!server.authenticate(DASH_OTA_USER, DASH_OTA_PASS))
        return;
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        dashLog("[OTA] Receiving: " + String(upload.filename.c_str()));
        esp_task_wdt_deinit();
        if (!Update.begin(UPDATE_SIZE_UNKNOWN))
            dashLog("[OTA] Begin failed");
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            dashLog("[OTA] Write error");
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (Update.end(true))
            dashLog("[OTA] Done: " + String(upload.totalSize) + " bytes");
        else
            dashLog("[OTA] End failed");
    }
}

// ── PLUGIN MANAGEMENT ───────────────────────────────────────────

static void dashReapplyFiltersWithPlugins()
{
    if (!dashHandler || !dashDriver)
        return;
    // Merge handler + plugin filter IDs
    uint32_t mergedIds[32];
    uint8_t count = 0;
    const uint32_t *hIds = dashHandler->filterIds();
    uint8_t hCount = dashHandler->filterIdCount();
    for (uint8_t i = 0; i < hCount && count < 32; i++)
        mergedIds[count++] = hIds[i];
    count += pluginGetFilterIds(mergedIds + count, 32 - count);
    dashDriver->setFilters(mergedIds, count);
}

static const char *pluginOpName(PluginOpType t)
{
    switch (t)
    {
    case OP_SET_BIT:
        return "set_bit";
    case OP_SET_BYTE:
        return "set_byte";
    case OP_OR_BYTE:
        return "or_byte";
    case OP_AND_BYTE:
        return "and_byte";
    case OP_CHECKSUM:
        return "checksum";
    default:
        return "?";
    }
}

static bool isHandlerCanId(uint32_t id)
{
    if (!appActiveHandler)
        return false;
    const uint32_t *hIds = appActiveHandler->filterIds();
    uint8_t hCount = appActiveHandler->filterIdCount();
    for (uint8_t i = 0; i < hCount; i++)
    {
        if (hIds[i] == id)
            return true;
    }
    return false;
}

static void handlePluginList()
{
    String j = "{\"maxPlugins\":" + String(PLUGIN_MAX) + ",\"plugins\":[";
    for (uint8_t i = 0; i < pluginCount; i++)
    {
        if (i)
            j += ",";
        j += "{\"name\":\"" + jsonEscape(pluginStore[i].name) + "\"";
        j += ",\"version\":\"" + jsonEscape(pluginStore[i].version) + "\"";
        j += ",\"author\":\"" + jsonEscape(pluginStore[i].author) + "\"";
        j += ",\"rules\":" + String(pluginStore[i].ruleCount);
        j += ",\"enabled\":" + String(pluginStore[i].enabled ? "true" : "false");

        // Rule details
        j += ",\"details\":[";
        for (uint8_t r = 0; r < pluginStore[i].ruleCount; r++)
        {
            const PluginRule &rule = pluginStore[i].rules[r];
            if (r)
                j += ",";
            j += "{\"id\":" + String(rule.canId);
            j += ",\"hex\":\"0x" + String(rule.canId, HEX) + "\"";
            j += ",\"mux\":" + String(rule.mux);
            j += ",\"send\":" + String(rule.sendAfter ? "true" : "false");
            j += ",\"conflict\":" + String(isHandlerCanId(rule.canId) ? "true" : "false");
            j += ",\"ops\":[";
            for (uint8_t o = 0; o < rule.opCount; o++)
            {
                const PluginOp &op = rule.ops[o];
                if (o)
                    j += ",";
                j += "{\"type\":\"" + String(pluginOpName(op.type)) + "\"";
                if (op.type == OP_SET_BIT)
                    j += ",\"bit\":" + String(op.index) + ",\"val\":" + String(op.value);
                else if (op.type == OP_CHECKSUM)
                    j += "";
                else
                {
                    j += ",\"byte\":" + String(op.index) + ",\"val\":" + String(op.value);
                    if (op.type == OP_SET_BYTE)
                        j += ",\"mask\":" + String(op.mask);
                }
                j += "}";
            }
            j += "]}";
        }
        j += "]}";
    }
    j += "],\"wifi\":{\"connected\":";
    j += staConnected ? "true" : "false";
    j += ",\"ssid\":\"" + jsonEscape(staSSID) + "\"";
    if (staConnected)
        j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
    j += "}}";
    server.send(200, "application/json", j);
}

static bool pluginInstallJson(const String &json, const String &url)
{
    if (pluginCount >= PLUGIN_MAX)
        return false;

    PluginData temp;
    if (!pluginParseJson(json, temp))
        return false;

    // Check for duplicate name
    int existing = pluginFindByName(temp.name);
    if (existing >= 0)
        pluginRemove(existing);

    // Generate filename from name
    String fname = String(temp.name);
    fname.replace(" ", "_");
    fname.toLowerCase();
    fname += ".json";
    strlcpy(temp.filename, fname.c_str(), sizeof(temp.filename));
    strlcpy(temp.sourceUrl, url.c_str(), sizeof(temp.sourceUrl));

    if (!pluginSaveToSpiffs(json, temp.filename))
        return false;

    pluginStore[pluginCount] = temp;
    pluginCount++;

    dashReapplyFiltersWithPlugins();
    dashLog("[PLG] Installed: " + String(temp.name) + " (" + String(temp.ruleCount) + " rules)");
    return true;
}

static void handlePluginUpload()
{
    String json = server.arg("plain");
    if (json.length() == 0)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"empty body\"}");
        return;
    }
    if (pluginInstallJson(json, ""))
        server.send(200, "application/json", "{\"ok\":true}");
    else
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid plugin JSON or max " + String(PLUGIN_MAX) + " plugins reached\"}");
}

static void handlePluginInstallFromUrl()
{
    String url = server.arg("url");
    if (url.length() == 0)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"no url\"}");
        return;
    }
    if (!staConnected)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"WiFi not connected. Configure WiFi first.\"}");
        return;
    }

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure(); // skip cert verification for simplicity
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);
    http.begin(client, url);
    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        String err = "HTTP " + String(code);
        http.end();
        dashLog("[PLG] Download failed: " + err);
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"" + err + "\"}");
        return;
    }
    String json = http.getString();
    http.end();

    if (pluginInstallJson(json, url))
        server.send(200, "application/json", "{\"ok\":true}");
    else
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid plugin JSON or max " + String(PLUGIN_MAX) + " plugins reached\"}");
}

static void handlePluginToggle()
{
    if (!server.hasArg("idx"))
    {
        server.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    uint8_t idx = server.arg("idx").toInt();
    if (idx < pluginCount)
    {
        pluginStore[idx].enabled = !pluginStore[idx].enabled;
        dashReapplyFiltersWithPlugins();
        dashLog("[PLG] " + String(pluginStore[idx].name) + " " +
                String(pluginStore[idx].enabled ? "enabled" : "disabled"));
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handlePluginRemove()
{
    if (!server.hasArg("idx"))
    {
        server.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    uint8_t idx = server.arg("idx").toInt();
    if (idx < pluginCount)
    {
        String name = pluginStore[idx].name;
        pluginRemove(idx);
        dashReapplyFiltersWithPlugins();
        dashLog("[PLG] Removed: " + name);
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

// ── WIFI STA ────────────────────────────────────────────────────

static void dashConnectSTA()
{
    if (strlen(staSSID) == 0)
        return;
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID, apPass, 1, apHidden ? 1 : 0, 4);
    if (staStaticIP && (uint32_t)staIP != 0)
    {
        WiFi.config(staIP, staGW, staMask, staDNS);
        dashLog("[WIFI] Static IP: " + staIP.toString());
    }
    else
    {
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }
    WiFi.begin(staSSID, staPass);
    dashLog("[WIFI] Connecting to " + String(staSSID) + "...");
}

static void performAutoUpdate(); // forward decl, defined below

static void dashCheckWifi()
{
    static unsigned long lastCheck = 0;
    if (strlen(staSSID) == 0)
        return;
    if (millis() - lastCheck < 5000)
        return;
    lastCheck = millis();

    bool connected = WiFi.status() == WL_CONNECTED;
    if (connected != staConnected)
    {
        staConnected = connected;
        if (connected)
        {
            dashLog("[WIFI] Connected to " + String(staSSID) + " IP: " + WiFi.localIP().toString());
            // Schedule auto-update check 15 s after STA comes up (grace period for other boot work)
            if (autoUpdateEnabled && !autoUpdateDone)
                autoUpdateEligibleAt = millis() + 15000;
        }
        else
            dashLog("[WIFI] Disconnected from " + String(staSSID));
    }

    // Fire one-shot auto-update check once eligible
    if (autoUpdateEnabled && !autoUpdateDone && staConnected && autoUpdateEligibleAt > 0 && millis() >= autoUpdateEligibleAt)
    {
        autoUpdateDone = true;
        performAutoUpdate();
    }
}

static void handleWifiScan()
{
    int n = WiFi.scanNetworks(false, false, false, 300);
    String j = "{\"networks\":[";
    for (int i = 0; i < n && i < 20; i++)
    {
        if (i)
            j += ",";
        j += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i).c_str()) + "\"";
        j += ",\"rssi\":" + String(WiFi.RSSI(i));
        j += ",\"enc\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
        j += ",\"ch\":" + String(WiFi.channel(i));
        j += "}";
    }
    j += "]}";
    WiFi.scanDelete();
    server.send(200, "application/json", j);
}

static void handleWifiConfig()
{
    if (server.hasArg("ssid"))
    {
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");
        strlcpy(staSSID, ssid.c_str(), sizeof(staSSID));
        strlcpy(staPass, pass.c_str(), sizeof(staPass));

        prefs.begin(PREFS_NS, false);
        prefs.putString("wifi_ssid", ssid);
        prefs.putString("wifi_pass", pass);

        // Static IP config
        if (server.hasArg("static") && server.arg("static") == "1")
        {
            staStaticIP = true;
            staIP.fromString(server.arg("ip"));
            staGW.fromString(server.arg("gw"));
            staMask.fromString(server.arg("mask"));
            staDNS.fromString(server.arg("dns"));
            prefs.putBool("wifi_static", true);
            prefs.putString("wifi_ip", server.arg("ip"));
            prefs.putString("wifi_gw", server.arg("gw"));
            prefs.putString("wifi_mask", server.arg("mask"));
            prefs.putString("wifi_dns", server.arg("dns"));
        }
        else
        {
            staStaticIP = false;
            prefs.putBool("wifi_static", false);
        }
        prefs.end();

        staConnected = false;
        dashConnectSTA();
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleWifiStatus()
{
    Preferences p;
    bool stored = false;
    if (p.begin(PREFS_NS, true))
    {
        stored = p.getString("wifi_ssid", "").length() > 0;
        p.end();
    }
    String j = "{\"connected\":";
    j += staConnected ? "true" : "false";
    j += ",\"ssid\":\"" + jsonEscape(staSSID) + "\"";
    j += ",\"stored\":" + String(stored ? "true" : "false");
    if (staConnected)
        j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
    j += ",\"static\":" + String(staStaticIP ? "true" : "false");
    if (staStaticIP)
    {
        j += ",\"cfg_ip\":\"" + staIP.toString() + "\"";
        j += ",\"cfg_gw\":\"" + staGW.toString() + "\"";
        j += ",\"cfg_mask\":\"" + staMask.toString() + "\"";
        j += ",\"cfg_dns\":\"" + staDNS.toString() + "\"";
    }
    j += "}";
    server.send(200, "application/json", j);
}

// ── AP Config (hotspot name/password) ───────────────────────────

static void handleCanPins()
{
    Preferences canPrefs;
    int tx = -1, rx = -1;
    if (canPrefs.begin("can", true))
    {
        tx = canPrefs.getChar("tx", -1);
        rx = canPrefs.getChar("rx", -1);
        canPrefs.end();
    }
    String j = "{\"tx\":" + String(tx);
    j += ",\"rx\":" + String(rx);
    j += ",\"customized\":" + String((tx >= 0 || rx >= 0) ? "true" : "false");
    j += "}";
    server.send(200, "application/json", j);
}

static void handleCanPinsSave()
{
    int tx = server.arg("tx").toInt();
    int rx = server.arg("rx").toInt();

    if (tx < 0 || tx > 39 || rx < 0 || rx > 39)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Pin must be 0-39\"}");
        return;
    }
    if (tx == rx)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"TX and RX must differ\"}");
        return;
    }
    // GPIO 6-11 are reserved for SPI flash on most ESP32 modules
    if ((tx >= 6 && tx <= 11) || (rx >= 6 && rx <= 11))
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"GPIO 6-11 reserved for flash\"}");
        return;
    }

    Preferences canPrefs;
    if (!canPrefs.begin("can", false))
    {
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS open failed\"}");
        return;
    }
    canPrefs.putChar("tx", (int8_t)tx);
    canPrefs.putChar("rx", (int8_t)rx);
    canPrefs.end();

    dashLog("[CAN] Pins saved: TX=" + String(tx) + " RX=" + String(rx) + " (reboot required)");
    server.send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
}

// ── Settings Backup / Restore ───────────────────────────────────

static void handleSettingsExport()
{
    Preferences p;
    String apSsid = "", apPass = "", wSsid = "", wPass = "";
    String wIp = "", wGw = "", wMask = "", wDns = "";
    bool wStatic = false, beta = false, apHid = false;
    int canTx = -1, canRx = -1;

    if (p.begin(PREFS_NS, true))
    {
        apSsid = p.getString("ap_ssid", "");
        apPass = p.getString("ap_pass", "");
        apHid = p.getBool("ap_hidden", false);
        wSsid = p.getString("wifi_ssid", "");
        wPass = p.getString("wifi_pass", "");
        wStatic = p.getBool("wifi_static", false);
        wIp = p.getString("wifi_ip", "");
        wGw = p.getString("wifi_gw", "");
        wMask = p.getString("wifi_mask", "");
        wDns = p.getString("wifi_dns", "");
        beta = p.getBool("upd_beta", false);
        p.end();
    }
    Preferences cp;
    if (cp.begin("can", true))
    {
        canTx = cp.getChar("tx", -1);
        canRx = cp.getChar("rx", -1);
        cp.end();
    }

    String j = "{\"version\":\"" FIRMWARE_VERSION "\"";
    j += ",\"ap\":{\"ssid\":\"" + jsonEscape(apSsid) + "\",\"pass\":\"" + jsonEscape(apPass) + "\",\"hidden\":" + String(apHid ? "true" : "false") + "}";
    j += ",\"wifi\":{\"ssid\":\"" + jsonEscape(wSsid) + "\",\"pass\":\"" + jsonEscape(wPass) + "\"";
    j += ",\"static\":" + String(wStatic ? "true" : "false");
    j += ",\"ip\":\"" + jsonEscape(wIp) + "\",\"gw\":\"" + jsonEscape(wGw) + "\"";
    j += ",\"mask\":\"" + jsonEscape(wMask) + "\",\"dns\":\"" + jsonEscape(wDns) + "\"}";
    j += ",\"can\":{\"tx\":" + String(canTx) + ",\"rx\":" + String(canRx) + "}";
    j += ",\"beta\":" + String(beta ? "true" : "false");
    j += "}";

    server.sendHeader("Content-Disposition", "attachment; filename=\"evtools-backup.json\"");
    server.send(200, "application/json", j);
}

static void handleSettingsImport()
{
    String body = server.arg("plain");
    if (body.length() == 0)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Empty body\"}");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
        return;
    }

    Preferences p;
    if (!p.begin(PREFS_NS, false))
    {
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS open failed\"}");
        return;
    }

    if (doc["ap"].is<JsonObject>())
    {
        const char *s = doc["ap"]["ssid"] | "";
        const char *pw = doc["ap"]["pass"] | "";
        if (strlen(s) > 0) p.putString("ap_ssid", s);
        if (strlen(pw) >= 8) p.putString("ap_pass", pw);
        if (doc["ap"]["hidden"].is<bool>()) p.putBool("ap_hidden", doc["ap"]["hidden"].as<bool>());
    }
    if (doc["wifi"].is<JsonObject>())
    {
        const char *s = doc["wifi"]["ssid"] | "";
        const char *pw = doc["wifi"]["pass"] | "";
        p.putString("wifi_ssid", s);
        p.putString("wifi_pass", pw);
        p.putBool("wifi_static", doc["wifi"]["static"] | false);
        p.putString("wifi_ip", (const char *)(doc["wifi"]["ip"] | ""));
        p.putString("wifi_gw", (const char *)(doc["wifi"]["gw"] | ""));
        p.putString("wifi_mask", (const char *)(doc["wifi"]["mask"] | ""));
        p.putString("wifi_dns", (const char *)(doc["wifi"]["dns"] | ""));
    }
    if (doc["beta"].is<bool>())
        p.putBool("upd_beta", doc["beta"].as<bool>());
    p.end();

    if (doc["can"].is<JsonObject>())
    {
        int tx = doc["can"]["tx"] | -1;
        int rx = doc["can"]["rx"] | -1;
        Preferences cp;
        if (cp.begin("can", false))
        {
            if (tx >= 0 && tx <= 39 && rx >= 0 && rx <= 39 && tx != rx &&
                !((tx >= 6 && tx <= 11) || (rx >= 6 && rx <= 11)))
            {
                cp.putChar("tx", (int8_t)tx);
                cp.putChar("rx", (int8_t)rx);
            }
            cp.end();
        }
    }

    dashLog("[BACKUP] Settings imported (reboot required)");
    server.send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
}

static void handleApConfig()
{
    String newSsid = server.arg("ssid");
    String newPass = server.arg("pass");
    bool hasHidden = server.hasArg("hidden");
    bool newHidden = hasHidden && (server.arg("hidden") == "1" || server.arg("hidden") == "true");

    if (newSsid.length() == 0)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"SSID required\"}");
        return;
    }
    if (newPass.length() > 0 && newPass.length() < 8)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Password must be at least 8 characters\"}");
        return;
    }

    strlcpy(apSSID, newSsid.c_str(), sizeof(apSSID));
    if (newPass.length() > 0)
        strlcpy(apPass, newPass.c_str(), sizeof(apPass));
    if (hasHidden)
        apHidden = newHidden;

    prefs.begin(PREFS_NS, false);
    prefs.putString("ap_ssid", newSsid);
    if (newPass.length() > 0)
        prefs.putString("ap_pass", newPass);
    if (hasHidden)
        prefs.putBool("ap_hidden", newHidden);
    prefs.end();

    dashLog("[WIFI] AP config updated: SSID=" + newSsid + (apHidden ? " (hidden)" : ""));
    server.send(200, "application/json", "{\"ok\":true,\"msg\":\"Saved. Reboot to apply new AP settings.\"}");
}

static void handleApStatus()
{
    Preferences p;
    bool stored = false;
    if (p.begin(PREFS_NS, true))
    {
        stored = p.getString("ap_ssid", "").length() > 0;
        p.end();
    }
    String j = "{\"ssid\":\"" + jsonEscape(apSSID) + "\"";
    j += ",\"ip\":\"" + WiFi.softAPIP().toString() + "\"";
    j += ",\"clients\":" + String(WiFi.softAPgetStationNum());
    j += ",\"stored\":" + String(stored ? "true" : "false");
    j += ",\"hidden\":" + String(apHidden ? "true" : "false");
    j += "}";
    server.send(200, "application/json", j);
}

// ── OTA GitHub Update ───────────────────────────────────────────

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "unknown"
#endif

static const char *GITHUB_REPO = "ev-open-can-tools/ev-open-can-tools";

// Map driver type to release artifact filename
static const char *getFirmwareArtifact()
{
#if defined(DRIVER_ESP32_EXT_MCP2515)
    return "firmware-esp32-ext-mcp2515.bin";
#else
    return "firmware-esp32.bin";
#endif
}

// Parse a semver-ish version string into (major, minor, patch, preRank, preNum).
// Pre-release rank: 0 = stable (no suffix, sorts highest among same M.m.p),
//                  1 = -alpha.N, 2 = -beta.N, 3 = -rc.N (higher rank = closer to stable).
// Unknown suffix → treated as stable (rank 0).
static void parseVersion(const String &v, int &maj, int &min, int &pat, int &preRank, int &preNum)
{
    maj = min = pat = 0;
    preRank = 0;
    preNum = 0;
    int i = 0;
    int len = v.length();
    auto readInt = [&](int &out) {
        int val = 0;
        bool any = false;
        while (i < len && v[i] >= '0' && v[i] <= '9')
        {
            val = val * 10 + (v[i] - '0');
            i++;
            any = true;
        }
        if (any)
            out = val;
    };
    readInt(maj);
    if (i < len && v[i] == '.')
    {
        i++;
        readInt(min);
    }
    if (i < len && v[i] == '.')
    {
        i++;
        readInt(pat);
    }
    if (i < len && v[i] == '-')
    {
        i++;
        String tail = v.substring(i);
        tail.toLowerCase();
        if (tail.startsWith("alpha"))
            preRank = 1;
        else if (tail.startsWith("beta"))
            preRank = 2;
        else if (tail.startsWith("rc"))
            preRank = 3;
        else
            preRank = 0; // unknown → treat as stable
        int dot = tail.indexOf('.');
        if (dot >= 0)
            preNum = tail.substring(dot + 1).toInt();
    }
}

// Returns true iff `candidate` is strictly newer than `current`.
static bool isVersionNewer(const String &candidate, const String &current)
{
    int cM, cm, cp, cR, cN;
    int uM, um, up, uR, uN;
    parseVersion(candidate, cM, cm, cp, cR, cN);
    parseVersion(current, uM, um, up, uR, uN);
    if (cM != uM)
        return cM > uM;
    if (cm != um)
        return cm > um;
    if (cp != up)
        return cp > up;
    // Same M.m.p — stable (rank 0) beats any prerelease (rank 1-3)
    // For two prereleases: higher rank beats lower (rc > beta > alpha)
    int cEff = (cR == 0) ? 1000 : cR; // stable → very high
    int uEff = (uR == 0) ? 1000 : uR;
    if (cEff != uEff)
        return cEff > uEff;
    return cN > uN;
}

static void handleUpdateCheck()
{
    if (!staConnected)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"WiFi not connected\"}");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    String url;
    if (updateBetaChannel)
        url = "https://api.github.com/repos/" + String(GITHUB_REPO) + "/releases?per_page=1";
    else
        url = "https://api.github.com/repos/" + String(GITHUB_REPO) + "/releases/latest";

    http.begin(client, url);
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("User-Agent", "ESP32-OTA");
    int code = http.GET();

    if (code != 200)
    {
        http.end();
        server.send(502, "application/json", "{\"ok\":false,\"error\":\"GitHub API error " + String(code) + "\"}");
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
    {
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"JSON parse error\"}");
        return;
    }

    // Find the right release
    JsonObject release;
    if (updateBetaChannel)
    {
        JsonArray arr = doc.as<JsonArray>();
        for (JsonObject r : arr)
        {
            release = r;
            break; // first (newest) release
        }
    }
    else
    {
        release = doc.as<JsonObject>();
    }

    if (release.isNull())
    {
        server.send(404, "application/json", "{\"ok\":false,\"error\":\"No release found\"}");
        return;
    }

    String tagName = release["tag_name"] | "";
    bool prerelease = release["prerelease"] | false;
    String version = tagName;
    if (version.startsWith("v"))
        version = version.substring(1);

    // Find the matching firmware asset
    String downloadUrl = "";
    const char *artifact = getFirmwareArtifact();
    JsonArray assets = release["assets"];
    for (JsonObject asset : assets)
    {
        String name = asset["name"] | "";
        if (name == artifact)
        {
            downloadUrl = asset["browser_download_url"].as<String>();
            break;
        }
    }

    String j = "{\"ok\":true";
    j += ",\"current\":\"" + jsonEscape(FIRMWARE_VERSION) + "\"";
    j += ",\"latest\":\"" + jsonEscape(version.c_str()) + "\"";
    j += ",\"tag\":\"" + jsonEscape(tagName.c_str()) + "\"";
    j += ",\"prerelease\":" + String(prerelease ? "true" : "false");
    j += ",\"artifact\":\"" + jsonEscape(artifact) + "\"";
    j += ",\"url\":\"" + jsonEscape(downloadUrl.c_str()) + "\"";
    bool isNewer = isVersionNewer(version, String(FIRMWARE_VERSION));
    j += ",\"update\":" + String(isNewer && downloadUrl.length() > 0 ? "true" : "false");
    j += ",\"beta\":" + String(updateBetaChannel ? "true" : "false");
    j += "}";
    server.send(200, "application/json", j);
}

static void handleUpdateInstall()
{
    if (!staConnected)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"WiFi not connected\"}");
        return;
    }

    String url = server.arg("url");
    if (url.length() == 0)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"No URL provided\"}");
        return;
    }

    dashLog("[OTA] Starting GitHub update from: " + url);
    server.send(200, "application/json", "{\"ok\":true,\"msg\":\"Downloading and installing... Device will reboot.\"}");
    delay(500);

    WiFiClientSecure client;
    client.setInsecure();

    // Follow redirects — GitHub release assets redirect to S3
    HTTPClient http;
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.begin(client, url);
    http.addHeader("Accept", "application/octet-stream");
    int code = http.GET();

    if (code != 200)
    {
        dashLog("[OTA] Download failed: HTTP " + String(code));
        http.end();
        return;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0)
    {
        dashLog("[OTA] Invalid content length");
        http.end();
        return;
    }

    if (!Update.begin(contentLength))
    {
        dashLog("[OTA] Not enough space for update");
        http.end();
        return;
    }

    WiFiClient *stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);
    http.end();

    if (written != (size_t)contentLength)
    {
        dashLog("[OTA] Written " + String(written) + " of " + String(contentLength) + " bytes");
        Update.abort();
        return;
    }

    if (!Update.end())
    {
        dashLog("[OTA] Update finalize failed");
        return;
    }

    dashLog("[OTA] Update successful! Rebooting...");
    delay(1000);
    ESP.restart();
}

// Check GitHub for a newer release and, if found, download + install it.
// Blocking; on success calls ESP.restart() and never returns.
static void performAutoUpdate()
{
    if (!staConnected)
        return;

    dashLog("[AUTO-OTA] Checking for updates...");

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    String url;
    if (updateBetaChannel)
        url = "https://api.github.com/repos/" + String(GITHUB_REPO) + "/releases?per_page=1";
    else
        url = "https://api.github.com/repos/" + String(GITHUB_REPO) + "/releases/latest";

    http.begin(client, url);
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("User-Agent", "ESP32-OTA");
    int code = http.GET();
    if (code != 200)
    {
        dashLog("[AUTO-OTA] GitHub API error " + String(code));
        http.end();
        return;
    }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload))
    {
        dashLog("[AUTO-OTA] JSON parse error");
        return;
    }

    JsonObject release;
    if (updateBetaChannel)
    {
        JsonArray arr = doc.as<JsonArray>();
        for (JsonObject r : arr)
        {
            release = r;
            break;
        }
    }
    else
    {
        release = doc.as<JsonObject>();
    }
    if (release.isNull())
    {
        dashLog("[AUTO-OTA] No release found");
        return;
    }

    String tagName = release["tag_name"] | "";
    String version = tagName;
    if (version.startsWith("v"))
        version = version.substring(1);
    if (!isVersionNewer(version, String(FIRMWARE_VERSION)))
    {
        dashLog("[AUTO-OTA] No newer release (latest=" + version + ", current=" FIRMWARE_VERSION ")");
        return;
    }

    const char *artifact = getFirmwareArtifact();
    String downloadUrl = "";
    for (JsonObject asset : release["assets"].as<JsonArray>())
    {
        String name = asset["name"] | "";
        if (name == artifact)
        {
            downloadUrl = asset["browser_download_url"].as<String>();
            break;
        }
    }
    if (!downloadUrl.length())
    {
        dashLog("[AUTO-OTA] No matching artifact for this build");
        return;
    }

    dashLog("[AUTO-OTA] Update " + version + " available. Installing...");

    HTTPClient http2;
    http2.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http2.begin(client, downloadUrl);
    http2.addHeader("Accept", "application/octet-stream");
    int code2 = http2.GET();
    if (code2 != 200)
    {
        dashLog("[AUTO-OTA] Download failed: HTTP " + String(code2));
        http2.end();
        return;
    }
    int len = http2.getSize();
    if (len <= 0)
    {
        dashLog("[AUTO-OTA] Invalid content length");
        http2.end();
        return;
    }
    if (!Update.begin(len))
    {
        dashLog("[AUTO-OTA] Not enough space for update");
        http2.end();
        return;
    }
    WiFiClient *stream = http2.getStreamPtr();
    size_t written = Update.writeStream(*stream);
    http2.end();
    if (written != (size_t)len)
    {
        dashLog("[AUTO-OTA] Written " + String(written) + "/" + String(len) + " bytes");
        Update.abort();
        return;
    }
    if (!Update.end())
    {
        dashLog("[AUTO-OTA] Finalize failed");
        return;
    }
    dashLog("[AUTO-OTA] Update successful! Rebooting...");
    delay(1000);
    ESP.restart();
}

static void handleAutoUpdate()
{
    if (server.hasArg("enabled"))
    {
        autoUpdateEnabled = server.arg("enabled") == "1";
        prefs.begin(PREFS_NS, false);
        prefs.putBool("auto_upd", autoUpdateEnabled);
        prefs.end();
        dashLog("[AUTO-OTA] " + String(autoUpdateEnabled ? "enabled" : "disabled"));
    }
    String j = "{\"ok\":true,\"enabled\":";
    j += autoUpdateEnabled ? "true" : "false";
    j += "}";
    server.send(200, "application/json", j);
}

static void handleUpdateBeta()
{
    if (server.hasArg("beta"))
    {
        updateBetaChannel = server.arg("beta") == "1";
        prefs.begin(PREFS_NS, false);
        prefs.putBool("update_beta", updateBetaChannel);
        prefs.end();
        dashLog("[OTA] Channel: " + String(updateBetaChannel ? "beta" : "stable"));
    }
    String j = "{\"ok\":true,\"beta\":" + String(updateBetaChannel ? "true" : "false");
    j += ",\"version\":\"" + jsonEscape(FIRMWARE_VERSION) + "\"}";
    server.send(200, "application/json", j);
}

// ── Plugin frame callback wrapper ───────────────────────────────

static void dashPluginProcess(const CanFrame &frame, CanDriver &driver)
{
    pluginProcessFrame(frame, driver);
}

static void webTask(void *)
{
    for (;;)
    {
        ArduinoOTA.handle();
        server.handleClient();
        dashCheckWifi();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

static CarManagerBase *handlerPool[3] = {};

static void dashInitHandlers()
{
    handlerPool[0] = new LegacyHandler();
    handlerPool[1] = new HW3Handler();
    handlerPool[2] = new HW4Handler();
    for (int i = 0; i < 3; i++)
    {
        handlerPool[i]->onFrame = mcpDashOnFrame;
        handlerPool[i]->onSend = mcpDashOnSend;
    }
}

static void dashSwapHandler(uint8_t mode)
{
    if (mode > 2 || !handlerPool[mode])
        return;
    CarManagerBase *next = handlerPool[mode];
    if (dashHandler)
    {
        next->speedProfile = (int)dashHandler->speedProfile;
        next->enablePrint = (bool)dashHandler->enablePrint;
    }
    appActiveHandler = next;
    dashHandler = next;
    // Update driver acceptance filters for the new handler.
    // For MCP2515 (ext) dashApplyFilters() will also fine-tune the hardware
    // filter registers. For TWAI and old MCP2515 this abstract call is enough.
    if (dashDriver)
        dashDriver->setFilters(next->filterIds(), next->filterIdCount());
    const char *hwName = "LEGACY";
    if (mode == 1)
        hwName = "HW3";
    else if (mode == 2)
        hwName = "HW4";
    dashLog("[CFG] Handler switched to " + String(hwName));
}

#if defined(DRIVER_ESP32_EXT_MCP2515)
static void mcpDashboardSetup(CarManagerBase *handler, CanDriver *driver, MCP2515 *mcp)
{
    dashHandler = handler;
    dashDriver = driver;
    dashMcp = mcp;
#else
static void mcpDashboardSetup(CarManagerBase *handler, CanDriver *driver)
{
    dashHandler = handler;
    dashDriver = driver;
#endif
    startMs = millis();
    fpsLastMs = millis();

    if (!SPIFFS.begin(true))
        dashLog("[WARN] SPIFFS mount failed");

    dashLoadPrefs();
    dashInitHandlers();
    dashSwapHandler(hwMode);
    dashApplyFilters();

    // Load plugins from SPIFFS
    pluginLoadAll();
    if (pluginCount > 0)
    {
        dashLog("[PLG] Loaded " + String(pluginCount) + " plugin(s)");
        dashReapplyFiltersWithPlugins();
    }

    // Set plugin processing hook
    appPluginProcess = dashPluginProcess;

    // WiFi setup: AP+STA if STA credentials configured, AP-only otherwise.
    // WiFi.softAP(ssid, pass, channel=1, hidden=0, max_connection=4)
    const int apChannel = 1;
    const int apHiddenFlag = apHidden ? 1 : 0;
    const int apMaxConn = 4;
    if (strlen(staSSID) > 0)
    {
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(apSSID, apPass, apChannel, apHiddenFlag, apMaxConn);
        WiFi.begin(staSSID, staPass);
        dashLog("[WIFI] AP+STA mode, connecting to " + String(staSSID));
    }
    else
    {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(apSSID, apPass, apChannel, apHiddenFlag, apMaxConn);
    }
    if (apHidden)
        dashLog("[WIFI] AP SSID is hidden");
    Serial.printf("[WIFI] AP: %s  IP: %s\n", apSSID, WiFi.softAPIP().toString().c_str());

    xTaskCreatePinnedToCore(webTask, "web", 8192, nullptr, 1, nullptr, 0);

    ArduinoOTA.setHostname("ev-open-can-tools");
    ArduinoOTA.setPassword(DASH_OTA_PASS);
    ArduinoOTA.onStart([]()
                       { dashLog("[OTA] Starting..."); });
    ArduinoOTA.onEnd([]()
                     { dashLog("[OTA] Done -- rebooting"); });
    ArduinoOTA.onError([](ota_error_t e)
                       { dashLog("[OTA] Error: " + String(e)); });
    ArduinoOTA.begin();

    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/config", HTTP_POST, handleConfig);
    server.on("/features", HTTP_POST, handleFeatures);
    server.on("/frames", HTTP_GET, handleFrames);
    server.on("/log", HTTP_GET, handleLog);
    server.on("/reset_stats", HTTP_POST, handleResetStats);
    server.on("/rec_start", HTTP_POST, handleRecStart);
    server.on("/rec_stop", HTTP_POST, handleRecStop);
    server.on("/rec_status", HTTP_GET, handleRecStatus);
    server.on("/rec_download", HTTP_GET, handleRecDownload);
    server.on("/disable", HTTP_POST, handleDisable);
    server.on("/reboot", HTTP_POST, handleReboot);
    server.on("/update", HTTP_POST, handleOtaResult, handleOtaUpload);
    server.on("/plugins", HTTP_GET, handlePluginList);
    server.on("/plugin_upload", HTTP_POST, handlePluginUpload);
    server.on("/plugin_install", HTTP_POST, handlePluginInstallFromUrl);
    server.on("/plugin_toggle", HTTP_POST, handlePluginToggle);
    server.on("/plugin_remove", HTTP_POST, handlePluginRemove);
    server.on("/ap_config", HTTP_POST, handleApConfig);
    server.on("/ap_status", HTTP_GET, handleApStatus);
    server.on("/can_pins", HTTP_GET, handleCanPins);
    server.on("/can_pins", HTTP_POST, handleCanPinsSave);
    server.on("/settings_export", HTTP_GET, handleSettingsExport);
    server.on("/settings_import", HTTP_POST, handleSettingsImport);
    server.on("/wifi_scan", HTTP_GET, handleWifiScan);
    server.on("/wifi_config", HTTP_POST, handleWifiConfig);
    server.on("/wifi_status", HTTP_GET, handleWifiStatus);
    server.on("/update_check", HTTP_GET, handleUpdateCheck);
    server.on("/update_install", HTTP_POST, handleUpdateInstall);
    server.on("/update_beta", HTTP_POST, handleUpdateBeta);
    server.on("/auto_update", HTTP_GET, handleAutoUpdate);
    server.on("/auto_update", HTTP_POST, handleAutoUpdate);

    server.begin();
    Serial.println("[WEB] Dashboard: http://" + WiFi.softAPIP().toString());
    dashLog("[BOOT] ev-open-can-tools ready");
}

static void mcpDashboardLoop()
{
    if (Update.isRunning())
        return;
    dashCheckBusHealth();
    if (canOnline && millis() - lastFrameMs > 10000)
    {
        canOnline = false;
        dashLog("[CAN] Bus OFFLINE (timeout)");
    }
}

#endif
