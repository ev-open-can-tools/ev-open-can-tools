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
    // Separate runtime toggle for the CAN 880 counter+1 echo path.
    bool nagKiller = kNagKillerDefaultEnabled;
    bool isaSuppress = false;
    // Force AP-driven auto wipers off on engage, but release immediately after a
    // manual wiper request so the driver can opt back in without toggling AP.
    bool autoWipersOffOnAp = kAutoWipersOffOnApDefaultEnabled;
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

// WiFi STA (client) mode for internet access
static char staSSID[33] = "";
static char staPass[65] = "";
static bool staConnected = false;

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
    prefs.putBool("f_nk", feat.nagKiller);
    prefs.putBool("f_isa", feat.isaSuppress);
    prefs.putBool("f_awoa", feat.autoWipersOffOnAp);
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
    feat.nagKiller = prefs.getBool("f_nk", kNagKillerDefaultEnabled);
    enhancedAutopilotRuntime = feat.nagSuppress || feat.summonUnlock;
    feat.isaSuppress = prefs.getBool("f_isa", false);
    feat.autoWipersOffOnAp = prefs.getBool("f_awoa", kAutoWipersOffOnApDefaultEnabled);
    feat.evDetection = prefs.getBool("f_evd", false);
    feat.hw4Offset = prefs.getUChar("f_h4o", 0);
    speedProfileLocked = prefs.getBool("sp_lock", false);
    uint8_t sp = prefs.getUChar("sp", 1);
    bool ep = prefs.getBool("eprn", true);
    prefs.end();

    canActive = true;
    if (!canActiveLoaded)
        dashLog("[WARN] canActive was OFF (prior emergency stop) -- auto-reset to ON");

    bypassTlsscRequirementRuntime = bypassTlssc;
    emergencyVehicleDetectionRuntime = feat.evDetection;
    isaSpeedChimeSuppressRuntime = feat.isaSuppress;
    nagKillerRuntime = feat.nagKiller;
    autoWipersOffOnApRuntime = feat.autoWipersOffOnAp;
    hw4OffsetRuntime = feat.hw4Offset;
    if (dashHandler)
    {
        dashHandler->speedProfile = sp;
        dashHandler->enablePrint = ep;
    }
    // Load WiFi STA credentials
    String wifiSsid = prefs.getString("wifi_ssid", "");
    String wifiPass = prefs.getString("wifi_pass", "");
    strlcpy(staSSID, wifiSsid.c_str(), sizeof(staSSID));
    strlcpy(staPass, wifiPass.c_str(), sizeof(staPass));

    dashLog("[BOOT] Prefs loaded HW=" + String(hwMode) + " SP=" + String(sp));
    dashLog("[BOOT] canActive=YES bypassTlssc=" + String(bypassTlssc ? "YES" : "NO"));
    dashLog("[BOOT] feat: AD=" + String(feat.ADEnabled ? "ON" : "OFF") +
            " nag=" + String(feat.nagSuppress ? "ON" : "OFF") +
            " summon=" + String(feat.summonUnlock ? "ON" : "OFF") +
            " nk=" + String(feat.nagKiller ? "ON" : "OFF") +
            " isa=" + String(feat.isaSuppress ? "ON" : "OFF") +
            " awoa=" + String(feat.autoWipersOffOnAp ? "ON" : "OFF") +
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
        dashMcp->setFilter(MCP2515::RXF3, false, 1001);
        dashMcp->setFilter(MCP2515::RXF4, false, 627);
        dashMcp->setFilter(MCP2515::RXF5, false, 585);
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
    j += ",\"nk\":";
    j += feat.nagKiller ? "true" : "false";
    j += ",\"isa\":";
    j += feat.isaSuppress ? "true" : "false";
    j += ",\"awoa\":";
    j += feat.autoWipersOffOnAp ? "true" : "false";
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
    if (server.hasArg("nk"))
    {
        feat.nagKiller = server.arg("nk") == "1";
        nagKillerRuntime = feat.nagKiller;
        dashLog("[FEAT] Nag killer " + String(feat.nagKiller ? "ON" : "OFF"));
    }
    if (server.hasArg("isa"))
    {
        feat.isaSuppress = server.arg("isa") == "1";
        isaSpeedChimeSuppressRuntime = feat.isaSuppress;
        dashLog("[FEAT] ISA suppress " + String(feat.isaSuppress ? "ON" : "OFF"));
    }
    if (server.hasArg("awoa"))
    {
        feat.autoWipersOffOnAp = server.arg("awoa") == "1";
        autoWipersOffOnApRuntime = feat.autoWipersOffOnAp;
        dashLog("[FEAT] Auto wipers off on AP " + String(feat.autoWipersOffOnAp ? "ON" : "OFF"));
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
    feat.nagKiller = false;
    feat.isaSuppress = false;
    feat.autoWipersOffOnAp = false;
    feat.evDetection = false;
    canActive = false;
    nagKillerRuntime = false;
    isaSpeedChimeSuppressRuntime = false;
    autoWipersOffOnApRuntime = false;
    emergencyVehicleDetectionRuntime = false;
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
    String j = "{\"plugins\":[";
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
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid plugin JSON or max plugins reached\"}");
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
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid plugin JSON\"}");
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
    WiFi.softAP(DASH_SSID, DASH_PASS);
    WiFi.begin(staSSID, staPass);
    dashLog("[WIFI] Connecting to " + String(staSSID) + "...");
}

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
            dashLog("[WIFI] Connected to " + String(staSSID) + " IP: " + WiFi.localIP().toString());
        else
            dashLog("[WIFI] Disconnected from " + String(staSSID));
    }
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
        prefs.end();

        staConnected = false;
        dashConnectSTA();
    }
    server.send(200, "application/json", "{\"ok\":true}");
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

    // WiFi setup: AP+STA if STA credentials configured, AP-only otherwise
    if (strlen(staSSID) > 0)
    {
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(DASH_SSID, DASH_PASS);
        WiFi.begin(staSSID, staPass);
        dashLog("[WIFI] AP+STA mode, connecting to " + String(staSSID));
    }
    else
    {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(DASH_SSID, DASH_PASS);
    }
    Serial.printf("[WIFI] AP: %s  IP: %s\n", DASH_SSID, WiFi.softAPIP().toString().c_str());

    xTaskCreatePinnedToCore(webTask, "web", 8192, nullptr, 1, nullptr, 0);

    ArduinoOTA.setHostname("ADunlock");
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
    server.on("/wifi_config", HTTP_POST, handleWifiConfig);

    server.begin();
    Serial.println("[WEB] Dashboard: http://192.168.4.1");
    dashLog("[BOOT] ADUnlock ready");
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
