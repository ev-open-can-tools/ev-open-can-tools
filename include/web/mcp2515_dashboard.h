#pragma once

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD)

#ifdef ESP_PLATFORM
#include "platform/espidf_runtime.h"
#else
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#endif
#include <cerrno>
#include <exception>
#include <new>
#include <soc/soc_caps.h>
#ifdef ESP_PLATFORM
#include <freertos/semphr.h>
#endif
#ifndef ESP_PLATFORM
#include <Preferences.h>
#include <SPIFFS.h>
#endif
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

static_assert(sizeof(DASH_SSID) > 1 && sizeof(DASH_SSID) <= 33, "DASH_SSID must be 1-32 bytes");
static_assert(sizeof(DASH_PASS) >= 9 && sizeof(DASH_PASS) <= 64, "DASH_PASS must be 8-63 bytes");

#ifndef DASH_DEFAULT_HW
#define DASH_DEFAULT_HW 1
#endif

#if defined(DASH_INJECTION_ON_BOOT)
static constexpr bool kDashInjectionDefaultEnabled = true;
#else
static constexpr bool kDashInjectionDefaultEnabled = false;
#endif

#if defined(INJECTION_AFTER_AP) || defined(DASH_INJECTION_AFTER_AP)
static constexpr bool kDashApGateDefaultEnabled = true;
#else
static constexpr bool kDashApGateDefaultEnabled = false;
#endif

#if defined(DRIVER_TWAI)
#ifndef TWAI_TX_PIN
#define TWAI_TX_PIN GPIO_NUM_5
#endif
#ifndef TWAI_RX_PIN
#define TWAI_RX_PIN GPIO_NUM_4
#endif
#endif

#if DASH_DEFAULT_HW < 0 || DASH_DEFAULT_HW > 2
#error "DASH_DEFAULT_HW must be 0 (LEGACY), 1 (HW3), or 2 (HW4)"
#endif

#ifndef DASH_ALLOW_CAN_GPIO_6_11
#define DASH_ALLOW_CAN_GPIO_6_11 0
#endif

static bool dashCanPinReserved(int pin)
{
#if DASH_ALLOW_CAN_GPIO_6_11
    (void)pin;
    return false;
#else
    return pin >= 6 && pin <= 11;
#endif
}

#define PREFS_NS "ADunlock"
static constexpr uint8_t kDashUnsetU8 = 0xFF;

static Preferences prefs;

class DashDataGuard
{
public:
    DashDataGuard()
    {
#ifdef ESP_PLATFORM
        initialize();
        if (mutex_)
            locked_ = xSemaphoreTakeRecursive(mutex_, portMAX_DELAY) == pdTRUE;
#endif
    }

    ~DashDataGuard()
    {
#ifdef ESP_PLATFORM
        if (locked_)
            xSemaphoreGiveRecursive(mutex_);
#endif
    }

    static bool initialize()
    {
#ifdef ESP_PLATFORM
        if (!mutex_)
            mutex_ = xSemaphoreCreateRecursiveMutex();
        return mutex_ != nullptr;
#else
        return true;
#endif
    }

private:
#ifdef ESP_PLATFORM
    inline static SemaphoreHandle_t mutex_ = nullptr;
    bool locked_ = false;
#endif
};

class DashWifiGuard
{
public:
    DashWifiGuard()
    {
#ifdef ESP_PLATFORM
        initialize();
        if (mutex_)
            locked_ = xSemaphoreTakeRecursive(mutex_, portMAX_DELAY) == pdTRUE;
#endif
    }

    ~DashWifiGuard()
    {
#ifdef ESP_PLATFORM
        if (locked_)
            xSemaphoreGiveRecursive(mutex_);
#endif
    }

    static bool initialize()
    {
#ifdef ESP_PLATFORM
        if (!mutex_)
            mutex_ = xSemaphoreCreateRecursiveMutex();
        return mutex_ != nullptr;
#else
        return true;
#endif
    }

private:
#ifdef ESP_PLATFORM
    inline static SemaphoreHandle_t mutex_ = nullptr;
    bool locked_ = false;
#endif
};

class DashPrefsGuard
{
public:
    DashPrefsGuard()
    {
#ifdef ESP_PLATFORM
        initialize();
        if (mutex_)
            locked_ = xSemaphoreTakeRecursive(mutex_, portMAX_DELAY) == pdTRUE;
#endif
    }

    ~DashPrefsGuard()
    {
#ifdef ESP_PLATFORM
        if (locked_)
            xSemaphoreGiveRecursive(mutex_);
#endif
    }

    static bool initialize()
    {
#ifdef ESP_PLATFORM
        if (!mutex_)
            mutex_ = xSemaphoreCreateRecursiveMutex();
        return mutex_ != nullptr;
#else
        return true;
#endif
    }

private:
#ifdef ESP_PLATFORM
    inline static SemaphoreHandle_t mutex_ = nullptr;
    bool locked_ = false;
#endif
};

class DashHandlerRef
{
public:
    DashHandlerRef &operator=(CarManagerBase *handler)
    {
        value_ = handler;
        return *this;
    }
    operator CarManagerBase *() const { return value_; }
    CarManagerBase *operator->() const { return value_; }

private:
    Shared<CarManagerBase *> value_{nullptr};
};

static DashHandlerRef dashHandler;
static CanDriver *dashDriver = nullptr;
#if defined(DRIVER_ESP32_EXT_MCP2515)
static ESP32_MCP2515Driver *dashMcpDriver = nullptr;
#endif

static unsigned long lastFrameMs = 0;
static bool canOnline = false;

static Shared<uint8_t> hwMode{DASH_DEFAULT_HW};
static Shared<bool> canActive{kDashInjectionDefaultEnabled};
static Shared<bool> apInjectionGate{kDashApGateDefaultEnabled};
static Shared<bool> dashSpeedProfileAuto{true};
static Shared<uint8_t> dashManualSpeedProfile{1};

static constexpr uint8_t kHw3SlewRateMin = 1;
static constexpr uint8_t kHw3SlewRateMax = 25;
static constexpr uint8_t kHw3SlewRateDefault = 5;
static Shared<bool> hw3OffsetSlew{false};
static Shared<uint8_t> hw3SlewRate{kHw3SlewRateDefault};
static Shared<uint8_t> hw3OffsetTargetRaw{0};
static Shared<uint8_t> hw3OffsetLastRaw{0};
static Shared<uint32_t> hw3OffsetLastSentMs{0};
static Shared<uint32_t> hw3OffsetSlewCount{0};

#ifdef RGB_BRIGHTNESS
static constexpr uint8_t kDashLedBrightnessDefault = RGB_BRIGHTNESS;
#else
static constexpr uint8_t kDashLedBrightnessDefault = 32;
#endif
static Shared<uint8_t> dashLedBrightness{kDashLedBrightnessDefault};

// WiFi AP (hotspot) — overridable at runtime
static char apSSID[33] = "";
static char apPass[65] = "";
static bool apHidden = false; // when true, SSID is not broadcast (hidden AP)
static constexpr size_t kDashMaxSsidLen = 32;
static constexpr size_t kDashMinApPassLen = 8;
static constexpr size_t kDashMaxPassLen = 63;
static constexpr int kDashApChannel = 1;
static constexpr int kDashApMaxConn = 4;

// WiFi STA (client) mode for internet access
static char staSSID[33] = "";
static char staPass[65] = "";
static bool staConnected = false;
static bool staConnectAttemptActive = false;
static bool staStaticIP = false;

// Multi-SSID storage
static constexpr uint8_t kDashMaxWifiNetworks = 4;
struct DashWifiNetwork
{
    char ssid[33];
    char pass[65];
    bool useStatic;
    char ip[16];
    char gw[16];
    char mask[16];
    char dns[16];
};
static DashWifiNetwork wifiNetworks[kDashMaxWifiNetworks] = {};
static uint8_t wifiNetworkCount = 0;
static int8_t wifiActiveSlot = -1;    // slot currently selected for STA attempt
static int8_t wifiNextRotateSlot = 0; // next slot to try when rotating
static Shared<bool> updateBetaChannel{false};
static Shared<bool> autoUpdateEnabled{false};
static Shared<bool> autoUpdateDone{false};            // one-shot per boot
static Shared<unsigned long> autoUpdateEligibleAt{0}; // millis() at which auto-check may fire
static unsigned long staConnectStartedAt = 0;
static unsigned long staRetryAt = 0;
static constexpr unsigned long kDashStaBootDelayMs = 5000;
static constexpr unsigned long kDashStaConnectTimeoutMs = 25000;
static constexpr unsigned long kDashStaRetryMs = 120000;
static IPAddress staIP(0, 0, 0, 0);
static IPAddress staGW(0, 0, 0, 0);
static IPAddress staMask(255, 255, 255, 0);
static IPAddress staDNS(0, 0, 0, 0);

static bool dashStaConnectedSnapshot()
{
    DashWifiGuard guard;
    return staConnected;
}

// Multi-SSID NVS helpers (key form: w0s, w0p, w0t, w0i, w0g, w0m, w0d)
static String dashWifiKey(uint8_t slot, const char *sub)
{
    String k = "w";
    k += slot;
    k += sub;
    return k;
}
static void dashClearWifiNetwork(DashWifiNetwork &n)
{
    n.ssid[0] = 0;
    n.pass[0] = 0;
    n.useStatic = false;
    n.ip[0] = 0;
    n.gw[0] = 0;
    n.mask[0] = 0;
    n.dns[0] = 0;
}
static void dashRotateAndConnect();
static void dashSwapHandler(uint8_t mode);
static void dashApplyFilters();
static void dashReapplyFiltersWithPlugins();
static void dashApplyRuntimeState();
static void dashRestorePluginStates();
static void dashClearLegacyOptionPrefs();
static void dashSchedulePluginStateSave(unsigned long delayMs = 750);
static void dashFlushPluginStatesIfDue();

static Shared<bool> pluginStatesDirty{false};
static Shared<unsigned long> pluginStatesFlushAt{0};

enum DashWriteProbeState : uint8_t
{
    kDashWriteProbeIdle = 0,
    kDashWriteProbePending = 1,
    kDashWriteProbeMatch = 2,
    kDashWriteProbeDifferent = 3,
    kDashWriteProbeFailed = 4,
};

struct DashWriteProbe
{
    bool active = false;
    bool hasRx = false;
    uint8_t state = kDashWriteProbeIdle;
    uint32_t id = 0;
    int8_t mux = -1;
    uint8_t txDlc = 0;
    uint8_t rxDlc = 0;
    uint8_t txData[8] = {};
    uint8_t rxData[8] = {};
    unsigned long txMs = 0;
    unsigned long rxMs = 0;
};
static DashWriteProbe dashWriteProbe;

static int8_t dashFrameMux(const CanFrame &frame)
{
    if ((frame.id == 1006 || frame.id == 1021) && frame.dlc > 0)
        return static_cast<int8_t>(readMuxID(frame));
    return -1;
}

static void dashResetWriteProbe()
{
    dashWriteProbe = {};
    dashWriteProbe.mux = -1;
    dashWriteProbe.state = kDashWriteProbeIdle;
}

static bool dashWriteProbeMatches(const CanFrame &frame)
{
    if (!dashWriteProbe.active || dashWriteProbe.id != frame.id)
        return false;

    int8_t mux = dashFrameMux(frame);
    if (dashWriteProbe.mux < 0)
        return mux < 0;
    return mux == dashWriteProbe.mux;
}

static void dashLog(const String &s)
{
    Serial.println(s);
}

// Public hooks
static void mcpDashOnFrame(const CanFrame &f)
{
    DashDataGuard guard;
    unsigned long now = millis();
    lastFrameMs = now;
    canOnline = true;
    if (dashWriteProbe.active && dashWriteProbe.state != kDashWriteProbeFailed && dashWriteProbeMatches(f))
    {
        dashWriteProbe.hasRx = true;
        dashWriteProbe.rxMs = now;
        dashWriteProbe.rxDlc = (f.dlc <= 8) ? f.dlc : 8;
        memset(dashWriteProbe.rxData, 0, sizeof(dashWriteProbe.rxData));
        memcpy(dashWriteProbe.rxData, f.data, dashWriteProbe.rxDlc);
        bool same = dashWriteProbe.txDlc == dashWriteProbe.rxDlc &&
                    memcmp(dashWriteProbe.txData, dashWriteProbe.rxData, dashWriteProbe.txDlc) == 0;
        dashWriteProbe.state = same ? kDashWriteProbeMatch : kDashWriteProbeDifferent;
    }
}

static void mcpDashOnTxFrame(const CanFrame &frame, bool ok)
{
    DashDataGuard guard;
    int8_t mux = dashFrameMux(frame);
    dashWriteProbe.active = true;
    dashWriteProbe.hasRx = false;
    dashWriteProbe.state = ok ? kDashWriteProbePending : kDashWriteProbeFailed;
    dashWriteProbe.id = frame.id;
    dashWriteProbe.mux = mux;
    dashWriteProbe.txMs = millis();
    dashWriteProbe.rxMs = 0;
    dashWriteProbe.txDlc = (frame.dlc <= 8) ? frame.dlc : 8;
    dashWriteProbe.rxDlc = 0;
    memset(dashWriteProbe.txData, 0, sizeof(dashWriteProbe.txData));
    memset(dashWriteProbe.rxData, 0, sizeof(dashWriteProbe.rxData));
    memcpy(dashWriteProbe.txData, frame.data, dashWriteProbe.txDlc);
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

static bool dashParseLong(const String &text, long &value)
{
    if (text.length() == 0)
        return false;
    size_t i = text[0] == '-' ? 1 : 0;
    if (i == text.length())
        return false;
    for (; i < text.length(); i++)
    {
        if (text[i] < '0' || text[i] > '9')
            return false;
    }
    char *end = nullptr;
    errno = 0;
    value = strtol(text.c_str(), &end, 10);
    return errno == 0 && end && *end == '\0';
}

static bool dashParseBool(const String &text, bool &value)
{
    if (text == "0")
    {
        value = false;
        return true;
    }
    if (text == "1")
    {
        value = true;
        return true;
    }
    return false;
}

static bool dashCheckADEnabled()
{
    return canActive;
}

static constexpr unsigned long kDashApInjectionStableDelayMs = 1000;
static unsigned long dashApStableStartedMs = 0;
static bool dashApStableTracking = false;
static bool dashLastApForLog = false;

static unsigned long dashTrackApStableMs(bool ap, unsigned long now)
{
    DashDataGuard guard;
    if (ap && !dashLastApForLog)
    {
        dashApStableStartedMs = now;
        dashApStableTracking = true;
        dashLog("[APGATE] AP rising edge");
    }
    else if (!ap && dashLastApForLog)
    {
        dashApStableStartedMs = 0;
        dashApStableTracking = false;
        dashLog("[APGATE] AP falling edge");
    }
    dashLastApForLog = ap;
    return (ap && dashApStableTracking) ? now - dashApStableStartedMs : 0;
}

static bool dashApInjectionAllowed()
{
    if (!apInjectionGate)
        return true;
    if (!dashHandler)
        return false;

    unsigned long now = millis();
    bool ap = dashHandler->APActive;
    bool parked = dashHandler->Parked;
    bool summoning = dashHandler->Summoning;
    unsigned long apStableMs = dashTrackApStableMs(ap, now);
    return injectionGateOpenWithStableAp(ap, parked, summoning, apStableMs,
                                         kDashApInjectionStableDelayMs);
}

static bool dashInjectionActive()
{
    return canActive && appInjectionReady() && dashApInjectionAllowed();
}

static bool dashCheckNagDisabled()
{
    return false;
}

static bool dashStaSsidLooksCorrupt(const String &ssid)
{
    if (ssid.indexOf("\"ssid\"") >= 0 || ssid.indexOf("{\"") >= 0 ||
        ssid.indexOf("\",\"") >= 0)
        return true;
    for (size_t i = 0; i < ssid.length(); i++)
    {
        unsigned char c = static_cast<unsigned char>(ssid[i]);
        if (c < 0x20 || c == 0x7F)
            return true;
    }
    return false;
}

static uint8_t dashClampHw3SlewRate(int rate)
{
    if (rate < kHw3SlewRateMin)
        return kHw3SlewRateMin;
    if (rate > kHw3SlewRateMax)
        return kHw3SlewRateMax;
    return static_cast<uint8_t>(rate);
}

static uint8_t dashLoadHw3SlewRate(uint8_t rate)
{
    if (rate < kHw3SlewRateMin || rate > kHw3SlewRateMax)
        return kHw3SlewRateDefault;
    return rate;
}

static uint8_t dashClampSpeedProfileForHw(uint8_t hw, int profile)
{
    int maxProfile = hw == 2 ? 4 : 2;
    if (profile < 0)
        return 0;
    if (profile > maxProfile)
        return static_cast<uint8_t>(maxProfile);
    return static_cast<uint8_t>(profile);
}

static void dashApplySpeedProfileState()
{
    if (!dashHandler)
        return;
    dashHandler->speedProfileAuto = (bool)dashSpeedProfileAuto;
    if (!(bool)dashSpeedProfileAuto)
        dashHandler->speedProfile = dashClampSpeedProfileForHw(hwMode, dashManualSpeedProfile);
}

static bool dashReadHw3OffsetRaw(const CanFrame &frame, uint8_t &raw)
{
    if ((hwMode != 1 && hwMode != 2) || frame.id != 1021 || frame.dlc < 2 || readMuxID(frame) != 2)
        return false;

    raw = static_cast<uint8_t>(((frame.data[1] & 0x3F) << 2) | ((frame.data[0] >> 6) & 0x03));
    return true;
}

static void dashWriteHw3OffsetRaw(CanFrame &frame, uint8_t raw)
{
    frame.data[0] = static_cast<uint8_t>((frame.data[0] & ~0xC0) | ((raw & 0x03) << 6));
    frame.data[1] = static_cast<uint8_t>((frame.data[1] & ~0x3F) | (raw >> 2));
}

static bool dashApplyHw3OffsetSlew(CanFrame &modified, const CanFrame & /*original*/)
{
    uint8_t activeRaw = 0;
    if (!dashReadHw3OffsetRaw(modified, activeRaw))
        return false;

    hw3OffsetTargetRaw = activeRaw;
    uint8_t shapedRaw = activeRaw;
    uint32_t now = millis();

    if (hw3OffsetSlew)
    {
        uint8_t last = hw3OffsetLastRaw;
        if (activeRaw < last && hw3OffsetLastSentMs != 0)
        {
            uint32_t rateRawPerSec = static_cast<uint32_t>(dashLoadHw3SlewRate(hw3SlewRate)) * 4;
            uint32_t dt = now - hw3OffsetLastSentMs;
            uint32_t maxDrop = (rateRawPerSec * dt + 500) / 1000;
            uint8_t floorRaw = last > maxDrop ? static_cast<uint8_t>(last - maxDrop) : 0;
            if (activeRaw < floorRaw)
            {
                shapedRaw = floorRaw;
                hw3OffsetSlewCount++;
            }
        }
    }

    hw3OffsetLastRaw = shapedRaw;
    hw3OffsetLastSentMs = now;
    if (shapedRaw == activeRaw)
        return false;

    dashWriteHw3OffsetRaw(modified, shapedRaw);
    return true;
}

static void dashApplyRuntimeState()
{
    bypassTlsscRequirementRuntime = false;
    emergencyVehicleDetectionRuntime = false;
    isaSpeedChimeSuppressRuntime = false;
    enhancedAutopilotRuntime = false;
    nagKillerRuntime = false;

    if (dashHandler)
    {
        dashHandler->checkAD = dashCheckADEnabled;
        dashHandler->checkNag = dashCheckNagDisabled;
        dashApplySpeedProfileState();
        if (!canActive)
        {
            dashHandler->ADEnabled = false;
            dashHandler->APActive = false;
        }
    }

#if defined(DASH_RGB_STATUS_LED)
    appRefreshStatusLed();
#endif
}

// Store config
static bool dashSavePrefs()
{
    DashPrefsGuard guard;
    if (!prefs.begin(PREFS_NS, false))
        return false;
    prefs.putUChar("hw", hwMode);
    prefs.putUChar("hw_def", DASH_DEFAULT_HW);
    prefs.putBool("can", canActive);
    prefs.putBool("ap_gate", apInjectionGate);
    prefs.putBool("sp_auto", dashSpeedProfileAuto);
    prefs.putUChar("sp_sel", dashManualSpeedProfile);
    prefs.putUChar("plg_rep", pluginGetReplayCount());
    prefs.putBool("h3_slw", hw3OffsetSlew);
    prefs.putUChar("h3_srt", hw3SlewRate);
    prefs.putUChar("led_b", dashLedBrightness);
    return prefs.end();
}

static bool dashSetCanActive(bool active, const char *reason = nullptr)
{
    bool changed = canActive != active;
    canActive = active;
    dashApplyRuntimeState();
    bool saved = dashSavePrefs();
    if (changed)
    {
        String msg = String("[CFG] Injection ") + (active ? "ON" : "OFF");
        if (reason && *reason)
            msg += String(" via ") + reason;
        dashLog(msg);
    }
    if (!saved)
        dashLog("[ERR] Failed to persist dashboard settings");
    return saved;
}

[[maybe_unused]] static void dashToggleCanActive(const char *reason = nullptr)
{
    dashSetCanActive(!canActive, reason);
}

static bool dashApPasswordLengthValid(size_t len)
{
    return len >= kDashMinApPassLen && len <= kDashMaxPassLen;
}

static bool dashApConfigValid(const char *ssid, const char *pass)
{
    size_t ssidLen = strlen(ssid);
    size_t passLen = strlen(pass);
    return ssidLen > 0 && ssidLen <= kDashMaxSsidLen &&
           !dashStaSsidLooksCorrupt(String(ssid)) && dashApPasswordLengthValid(passLen);
}

static void dashUseDefaultApConfig()
{
    strlcpy(apSSID, DASH_SSID, sizeof(apSSID));
    strlcpy(apPass, DASH_PASS, sizeof(apPass));
    apHidden = false;
}

static bool dashStaConfigLengthValid(const String &ssid, const String &pass)
{
    return ssid.length() <= kDashMaxSsidLen && pass.length() <= kDashMaxPassLen;
}

static bool dashParseCanonicalIpv4(const char *text, uint32_t &value)
{
    if (!text || *text == '\0')
        return false;
    const char *cursor = text;
    value = 0;
    for (uint8_t part = 0; part < 4; part++)
    {
        if (*cursor < '0' || *cursor > '9')
            return false;
        const char *start = cursor;
        unsigned int octet = 0;
        while (*cursor >= '0' && *cursor <= '9')
        {
            octet = octet * 10 + static_cast<unsigned int>(*cursor - '0');
            if (octet > 255)
                return false;
            cursor++;
        }
        if (cursor - start > 1 && *start == '0')
            return false;
        value = (value << 8) | octet;
        if (part < 3)
        {
            if (*cursor++ != '.')
                return false;
        }
        else if (*cursor != '\0')
            return false;
    }
    return true;
}

static bool dashStaticIpConfigValid(const char *ip, const char *gateway, const char *mask, const char *dns)
{
    uint32_t ipValue = 0, gatewayValue = 0, maskValue = 0, dnsValue = 0;
    if (!dashParseCanonicalIpv4(ip, ipValue) || !dashParseCanonicalIpv4(gateway, gatewayValue) ||
        !dashParseCanonicalIpv4(mask, maskValue) || !dashParseCanonicalIpv4(dns, dnsValue) ||
        ipValue == 0 || gatewayValue == 0 || maskValue == 0 || dnsValue == 0)
        return false;
    uint32_t invertedMask = ~maskValue;
    if ((invertedMask & (invertedMask + 1)) != 0)
        return false;
    return (ipValue & maskValue) == (gatewayValue & maskValue);
}

static void dashClearLegacyOptionPrefs()
{
    static const char *const keys[] = {
        "fAD",
        "f_AD",
        "f_nag",
        "f_sum",
        "f_isa",
        "f_evd",
        "f_h4o",
        "sp",
        "sp_lock",
    };

    bool removed = false;
    for (const char *key : keys)
    {
        if (!prefs.isKey(key))
            continue;
        prefs.remove(key);
        removed = true;
    }

    if (removed)
        dashLog("[BOOT] Cleared legacy dashboard prefs from NVS");
}

static void dashLoadPrefs()
{
    DashPrefsGuard guard;
    prefs.begin(PREFS_NS, false);
    dashClearLegacyOptionPrefs();
    bool hasStoredHw = prefs.isKey("hw");
    uint8_t storedHw = prefs.getUChar("hw", DASH_DEFAULT_HW);
    uint8_t storedDefaultHw = prefs.getUChar("hw_def", kDashUnsetU8);
    bool migratedHw = false;

    hwMode = storedHw <= 2 ? storedHw : DASH_DEFAULT_HW;
    if (!hasStoredHw || storedHw > 2)
        migratedHw = true;

    // If the stored selection only mirrors the old firmware default, follow the
    // new build default after reflashing instead of staying pinned to stale NVS.
    if (storedDefaultHw <= 2 && storedDefaultHw != DASH_DEFAULT_HW && hwMode == storedDefaultHw)
    {
        hwMode = DASH_DEFAULT_HW;
        migratedHw = true;
    }

    if (migratedHw)
        prefs.putUChar("hw", hwMode);
    if (storedDefaultHw != DASH_DEFAULT_HW)
        prefs.putUChar("hw_def", DASH_DEFAULT_HW);
    canActive = prefs.getBool("can", kDashInjectionDefaultEnabled);
    apInjectionGate = prefs.getBool("ap_gate", kDashApGateDefaultEnabled);
    dashSpeedProfileAuto = prefs.getBool("sp_auto", true);
    dashManualSpeedProfile = dashClampSpeedProfileForHw(hwMode, prefs.getUChar("sp_sel", 1));
    pluginSetReplayCount(prefs.getUChar("plg_rep", PLUGIN_REPLAY_COUNT));
    hw3OffsetSlew = prefs.getBool("h3_slw", false);
    hw3SlewRate = dashLoadHw3SlewRate(prefs.getUChar("h3_srt", kHw3SlewRateDefault));
    dashLedBrightness = prefs.getUChar("led_b", kDashLedBrightnessDefault);
    dashApplyRuntimeState();
    // Load WiFi AP overrides (hotspot name/password)
    String apSsidPref = prefs.isKey("ap_ssid") ? prefs.getString("ap_ssid", "") : "";
    String apPassPref = prefs.isKey("ap_pass") ? prefs.getString("ap_pass", "") : "";
    bool hasApOverride = apSsidPref.length() > 0 || apPassPref.length() > 0 || prefs.isKey("ap_hidden");
    bool invalidApOverride = apSsidPref.length() > kDashMaxSsidLen ||
                             (apPassPref.length() > 0 && !dashApPasswordLengthValid(apPassPref.length()));
    if (apSsidPref.length() > 0)
        strlcpy(apSSID, apSsidPref.c_str(), sizeof(apSSID));
    else
        strlcpy(apSSID, DASH_SSID, sizeof(apSSID));
    if (apPassPref.length() > 0)
        strlcpy(apPass, apPassPref.c_str(), sizeof(apPass));
    else
        strlcpy(apPass, DASH_PASS, sizeof(apPass));
    apHidden = prefs.getBool("ap_hidden", false);
    if (invalidApOverride || !dashApConfigValid(apSSID, apPass))
    {
        if (hasApOverride)
        {
            prefs.remove("ap_ssid");
            prefs.remove("ap_pass");
            prefs.remove("ap_hidden");
            dashLog("[WIFI] Invalid saved AP config ignored");
        }
        dashUseDefaultApConfig();
    }

    // Load WiFi STA networks (multi-SSID slot array)
    wifiNetworkCount = 0;
    for (uint8_t i = 0; i < kDashMaxWifiNetworks; i++)
        dashClearWifiNetwork(wifiNetworks[i]);

    uint8_t storedCount = prefs.getUChar("wn_cnt", 0);
    if (storedCount > kDashMaxWifiNetworks)
        storedCount = kDashMaxWifiNetworks;

    for (uint8_t i = 0; i < storedCount; i++)
    {
        DashWifiNetwork &n = wifiNetworks[wifiNetworkCount];
        String s = prefs.getString(dashWifiKey(i, "s").c_str(), "");
        String p = prefs.getString(dashWifiKey(i, "p").c_str(), "");
        if (!dashStaConfigLengthValid(s, p) || dashStaSsidLooksCorrupt(s) || s.length() == 0)
            continue;
        strlcpy(n.ssid, s.c_str(), sizeof(n.ssid));
        strlcpy(n.pass, p.c_str(), sizeof(n.pass));
        n.useStatic = prefs.getBool(dashWifiKey(i, "t").c_str(), false);
        if (n.useStatic)
        {
            String ip = prefs.getString(dashWifiKey(i, "i").c_str(), "0.0.0.0");
            String gw = prefs.getString(dashWifiKey(i, "g").c_str(), "0.0.0.0");
            String mk = prefs.getString(dashWifiKey(i, "m").c_str(), "255.255.255.0");
            String dn = prefs.getString(dashWifiKey(i, "d").c_str(), "0.0.0.0");
            if (!dashStaticIpConfigValid(ip.c_str(), gw.c_str(), mk.c_str(), dn.c_str()))
            {
                dashClearWifiNetwork(n);
                continue;
            }
            strlcpy(n.ip, ip.c_str(), sizeof(n.ip));
            strlcpy(n.gw, gw.c_str(), sizeof(n.gw));
            strlcpy(n.mask, mk.c_str(), sizeof(n.mask));
            strlcpy(n.dns, dn.c_str(), sizeof(n.dns));
        }
        wifiNetworkCount++;
    }

    // One-shot migration from legacy single-SSID keys
    if (wifiNetworkCount == 0 && prefs.isKey("wifi_ssid"))
    {
        String s = prefs.getString("wifi_ssid", "");
        String p = prefs.getString("wifi_pass", "");
        if (dashStaConfigLengthValid(s, p) && !dashStaSsidLooksCorrupt(s) && s.length() > 0)
        {
            DashWifiNetwork &n = wifiNetworks[0];
            strlcpy(n.ssid, s.c_str(), sizeof(n.ssid));
            strlcpy(n.pass, p.c_str(), sizeof(n.pass));
            n.useStatic = prefs.getBool("wifi_static", false);
            if (n.useStatic)
            {
                String ip = prefs.getString("wifi_ip", "0.0.0.0");
                String gateway = prefs.getString("wifi_gw", "0.0.0.0");
                String mask = prefs.getString("wifi_mask", "255.255.255.0");
                String dns = prefs.getString("wifi_dns", "0.0.0.0");
                if (!dashStaticIpConfigValid(ip.c_str(), gateway.c_str(), mask.c_str(), dns.c_str()))
                    n.useStatic = false;
                else
                {
                    strlcpy(n.ip, ip.c_str(), sizeof(n.ip));
                    strlcpy(n.gw, gateway.c_str(), sizeof(n.gw));
                    strlcpy(n.mask, mask.c_str(), sizeof(n.mask));
                    strlcpy(n.dns, dns.c_str(), sizeof(n.dns));
                }
            }
            wifiNetworkCount = 1;
            prefs.putUChar("wn_cnt", 1);
            prefs.putString(dashWifiKey(0, "s").c_str(), s);
            prefs.putString(dashWifiKey(0, "p").c_str(), p);
            prefs.putBool(dashWifiKey(0, "t").c_str(), n.useStatic);
            if (n.useStatic)
            {
                prefs.putString(dashWifiKey(0, "i").c_str(), String(n.ip));
                prefs.putString(dashWifiKey(0, "g").c_str(), String(n.gw));
                prefs.putString(dashWifiKey(0, "m").c_str(), String(n.mask));
                prefs.putString(dashWifiKey(0, "d").c_str(), String(n.dns));
            }
            dashLog("[WIFI] Migrated legacy STA config to slot 0");
        }
        prefs.remove("wifi_ssid");
        prefs.remove("wifi_pass");
        prefs.remove("wifi_static");
        prefs.remove("wifi_ip");
        prefs.remove("wifi_gw");
        prefs.remove("wifi_mask");
        prefs.remove("wifi_dns");
    }

    // Seed staSSID/staPass with first slot for compat with existing connect path
    if (wifiNetworkCount > 0)
    {
        const DashWifiNetwork &n = wifiNetworks[0];
        strlcpy(staSSID, n.ssid, sizeof(staSSID));
        strlcpy(staPass, n.pass, sizeof(staPass));
        staStaticIP = n.useStatic;
        if (n.useStatic)
        {
            staIP.fromString(n.ip);
            staGW.fromString(n.gw);
            staMask.fromString(n.mask);
            staDNS.fromString(n.dns);
        }
        wifiActiveSlot = 0;
        wifiNextRotateSlot = wifiNetworkCount > 1 ? 1 : 0;
    }
    else
    {
        staSSID[0] = 0;
        staPass[0] = 0;
        staStaticIP = false;
        wifiActiveSlot = -1;
        wifiNextRotateSlot = 0;
    }

    updateBetaChannel = prefs.getBool("update_beta", false);
    autoUpdateEnabled = prefs.getBool("auto_upd", false);
    prefs.end();

    if (migratedHw)
        dashLog("[BOOT] HW default synced to " + String(hwMode == 0 ? "LEGACY" : hwMode == 1 ? "HW3"
                                                                                             : "HW4"));
    dashLog("[BOOT] Prefs loaded HW=" + String(hwMode));
    dashLog("[BOOT] canActive=" + String(canActive ? "YES" : "NO"));
    dashLog("[BOOT] pluginReplay=" + String(pluginGetReplayCount()));
}

static uint32_t dashPluginStateHash(const char *value)
{
    uint32_t hash = 2166136261u;
    while (*value)
    {
        hash ^= (uint8_t)*value++;
        hash *= 16777619u;
    }
    return hash;
}

static void dashPluginStateKey(const char *filename, char *key, size_t keySize)
{
    snprintf(key, keySize, "plg_%08lx", (unsigned long)dashPluginStateHash(filename));
}

static void dashPluginOrderKey(const char *filename, char *key, size_t keySize)
{
    snprintf(key, keySize, "plo_%08lx", (unsigned long)dashPluginStateHash(filename));
}

static bool dashSaveAllPluginStates()
{
    struct PluginStateSnapshot
    {
        char filename[32];
        bool enabled;
    };
    PluginStateSnapshot snapshot[PLUGIN_MAX] = {};
    uint8_t snapshotCount = 0;
    {
        PluginLockGuard guard;
        snapshotCount = pluginCount;
        for (uint8_t i = 0; i < snapshotCount; i++)
        {
            strlcpy(snapshot[i].filename, pluginStore[i].filename, sizeof(snapshot[i].filename));
            snapshot[i].enabled = pluginStore[i].enabled;
        }
    }
    Preferences pluginPrefs;
    if (!pluginPrefs.begin(PREFS_NS, false))
        return false;

    for (uint8_t i = 0; i < snapshotCount; i++)
    {
        char key[13];
        dashPluginStateKey(snapshot[i].filename, key, sizeof(key));
        pluginPrefs.putBool(key, snapshot[i].enabled);
        dashPluginOrderKey(snapshot[i].filename, key, sizeof(key));
        pluginPrefs.putUChar(key, i);
    }
    return pluginPrefs.end();
}

static bool dashClearPluginState(const PluginData &plugin)
{
    Preferences pluginPrefs;
    if (!pluginPrefs.begin(PREFS_NS, false))
        return false;

    char key[13];
    dashPluginStateKey(plugin.filename, key, sizeof(key));
    pluginPrefs.remove(key);
    dashPluginOrderKey(plugin.filename, key, sizeof(key));
    pluginPrefs.remove(key);
    return pluginPrefs.end();
}

static void dashRestorePluginStates()
{
    Preferences pluginPrefs;
    if (!pluginPrefs.begin(PREFS_NS, false))
        return;

    bool missingOrder = false;
    for (uint8_t i = 0; i < pluginCount; i++)
    {
        char key[13];
        dashPluginStateKey(pluginStore[i].filename, key, sizeof(key));
        pluginStore[i].enabled = pluginPrefs.getBool(key, pluginStore[i].enabled);

        dashPluginOrderKey(pluginStore[i].filename, key, sizeof(key));
        if (pluginPrefs.isKey(key))
            pluginStore[i].priority = pluginPrefs.getUChar(key, i);
        else
        {
            pluginStore[i].priority = i;
            missingOrder = true;
        }
    }
    pluginPrefs.end();

    pluginSortByPriority();
    if (missingOrder)
        dashSaveAllPluginStates();
}

static void dashSchedulePluginStateSave(unsigned long delayMs)
{
    pluginStatesDirty = true;
    pluginStatesFlushAt = millis() + delayMs;
}

static void dashFlushPluginStatesIfDue()
{
    if (!pluginStatesDirty)
        return;

    unsigned long now = millis();
    if ((long)(now - pluginStatesFlushAt) < 0)
        return;

    if (dashSaveAllPluginStates())
        pluginStatesDirty = false;
    else
        pluginStatesFlushAt = now + 5000;
}

// MCP2515-only: fine-grained filter register reload on HW mode switch.
// Other builds use dashDriver->setFilters() in dashSwapHandler instead.
static void dashApplyFilters()
{
#if defined(DRIVER_ESP32_EXT_MCP2515)
    dashReapplyFiltersWithPlugins();
    dashLog("[CFG] Filters set for " + String(hwMode == 0 ? "LEGACY" : hwMode == 1 ? "HW3"
                                                                                   : "HW4"));
#endif
}

// Bus-off recovery (MCP2515 only — TWAI driver handles its own bus-off internally)
#if defined(DRIVER_ESP32_EXT_MCP2515)
static unsigned long lastEflgCheckMs = 0;
static void dashCheckBusHealth()
{
    if (!dashMcpDriver)
        return;
    if (millis() - lastEflgCheckMs < 5000)
        return;
    lastEflgCheckMs = millis();
    uint8_t eflg = dashMcpDriver->errorFlags();
    if (eflg & 0x20)
    {
        dashLog("[ERR] MCP2515 BUS-OFF -- recovering");
        bool recovered = dashMcpDriver->recover();
        delay(10);
        if (recovered)
        {
            dashApplyFilters();
            dashLog("[OK] MCP2515 recovered");
        }
        else
            dashLog("[ERR] MCP2515 recovery failed");
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
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "max-age=3600");
#ifdef ESP_PLATFORM
    server.sendRaw(200, "text/html",
                   reinterpret_cast<const char *>(DASH_HTML_GZ),
                   DASH_HTML_GZ_LEN);
#else
    server.send_P(200, "text/html", reinterpret_cast<const char *>(DASH_HTML_GZ), DASH_HTML_GZ_LEN);
#endif
}

static void handleStatus()
{
    const unsigned long now = millis();
    bool canOnlineSnapshot = false;
    DashWriteProbe writeProbeSnapshot = {};
    {
        DashDataGuard guard;
        if (canOnline && now - lastFrameMs > 10000)
            canOnline = false;
        canOnlineSnapshot = canOnline;
        writeProbeSnapshot = dashWriteProbe;
    }

    char driverJson[768] = "{\"type\":\"unavailable\",\"stateCode\":0}";
    if (dashDriver)
        dashDriver->diagnosticsJson(driverJson, sizeof(driverJson));

    String j = "{\"can\":";
    j += canOnlineSnapshot ? "true" : "false";
    j += ",\"ia\":";
    j += dashInjectionActive() ? "true" : "false";
    j += ",\"ready\":";
    j += appInjectionReady() ? "true" : "false";
#ifdef ESP_PLATFORM
    j += ",\"runtime\":{\"uptimeMs\":" + String(now);
    j += ",\"canFrames\":" + String(RuntimeDiagnostics::canFrames.load(std::memory_order_relaxed));
    j += ",\"canAgeMs\":" + String(RuntimeDiagnostics::canAgeMs(now));
    j += ",\"txOk\":" + String(RuntimeDiagnostics::txOk.load(std::memory_order_relaxed));
    j += ",\"txFail\":" + String(RuntimeDiagnostics::txFail.load(std::memory_order_relaxed));
    j += ",\"freeHeap\":" + String(esp_get_free_heap_size());
    j += ",\"delayRemainingMs\":" + String(RuntimeDiagnostics::injectionDelayRemainingMs(now));
    j += ",\"frameThreshold\":" + String(CAN_LIVE_FRAME_THRESHOLD);
    j += "}";
#endif
    j += ",\"driver\":";
    j += driverJson;
    j += ",\"probe\":{\"active\":";
    j += writeProbeSnapshot.active ? "true" : "false";
    j += ",\"state\":";
    j += writeProbeSnapshot.state;
    j += ",\"id\":";
    j += writeProbeSnapshot.id;
    j += ",\"mux\":";
    j += writeProbeSnapshot.mux;
    j += ",\"txa\":";
    j += writeProbeSnapshot.active ? String(now - writeProbeSnapshot.txMs) : String(0);
    j += ",\"rxa\":";
    j += writeProbeSnapshot.hasRx ? String(now - writeProbeSnapshot.rxMs) : String(0);
    j += ",\"txdlc\":";
    j += writeProbeSnapshot.txDlc;
    j += ",\"rxdlc\":";
    j += writeProbeSnapshot.rxDlc;
    j += ",\"hasrx\":";
    j += writeProbeSnapshot.hasRx ? "true" : "false";
    j += ",\"tx\":[";
    for (uint8_t i = 0; i < writeProbeSnapshot.txDlc; i++)
    {
        if (i)
            j += ",";
        j += String(writeProbeSnapshot.txData[i]);
    }
    j += "],\"rx\":[";
    for (uint8_t i = 0; i < writeProbeSnapshot.rxDlc; i++)
    {
        if (i)
            j += ",";
        j += String(writeProbeSnapshot.rxData[i]);
    }
    j += "]}}";
    server.send(200, "application/json", j);
}

static void handleConfigGet()
{
    CarManagerBase *handler = dashHandler;
    String json = "{\"hw\":" + String(hwMode);
    json += ",\"speedProfile\":" + String(handler ? (int)handler->speedProfile : (int)dashManualSpeedProfile);
    json += ",\"speedAuto\":" + String(dashSpeedProfileAuto ? "true" : "false");
    json += ",\"injectionArmed\":" + String(canActive ? "true" : "false");
    json += ",\"pluginReplay\":" + String(pluginGetReplayCount());
    json += ",\"pluginReplayMax\":" + String(PLUGIN_REPLAY_COUNT_MAX);
    json += ",\"apGate\":" + String(apInjectionGate ? "true" : "false");
    json += ",\"hw3OffsetSlew\":" + String(hw3OffsetSlew ? "true" : "false");
    json += ",\"hw3SlewRate\":" + String(hw3SlewRate);
    json += ",\"ledBrightness\":" + String(dashLedBrightness);
    json += "}";
    server.send(200, "application/json", json);
}

static void handleConfig()
{
    long hwValue = hwMode;
    long speedValue = dashManualSpeedProfile;
    long replayValue = pluginGetReplayCount();
    long slewRateValue = hw3SlewRate;
    bool canValue = canActive;
    bool speedAutoValue = dashSpeedProfileAuto;
    bool gateValue = apInjectionGate;
    bool slewValue = hw3OffsetSlew;
    const char *slewArg = server.hasArg("hw3OffsetSlew") ? "hw3OffsetSlew" : "offsetSlew";
    const char *slewRateArg = server.hasArg("hw3SlewRate") ? "hw3SlewRate" : "offsetSlewRate";
    bool valid = true;
    if (server.hasArg("hw"))
        valid &= dashParseLong(server.arg("hw"), hwValue) && hwValue >= 0 && hwValue <= 2;
    if (server.hasArg("sp"))
        valid &= dashParseLong(server.arg("sp"), speedValue) && speedValue >= 0 &&
                 speedValue <= (hwValue == 2 ? 4 : 2);
    if (server.hasArg("plgr"))
        valid &= dashParseLong(server.arg("plgr"), replayValue) && replayValue >= 1 &&
                 replayValue <= PLUGIN_REPLAY_COUNT_MAX;
    if (server.hasArg("hw3SlewRate") || server.hasArg("offsetSlewRate"))
        valid &= dashParseLong(server.arg(slewRateArg), slewRateValue) &&
                 slewRateValue >= kHw3SlewRateMin && slewRateValue <= kHw3SlewRateMax;
    if (server.hasArg("can"))
        valid &= dashParseBool(server.arg("can"), canValue);
    if (server.hasArg("spa"))
        valid &= dashParseBool(server.arg("spa"), speedAutoValue);
    if (server.hasArg("apg"))
        valid &= dashParseBool(server.arg("apg"), gateValue);
    if (server.hasArg("hw3OffsetSlew") || server.hasArg("offsetSlew"))
        valid &= dashParseBool(server.arg(slewArg), slewValue);
    if (!valid)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid configuration value\"}");
        return;
    }

    uint8_t oldHw = hwMode;
    bool oldCan = canActive;
    bool oldSpeedAuto = dashSpeedProfileAuto;
    uint8_t oldSpeed = dashManualSpeedProfile;
    bool oldGate = apInjectionGate;
    uint8_t oldReplay = pluginGetReplayCount();
    bool oldSlew = hw3OffsetSlew;
    uint8_t oldSlewRate = hw3SlewRate;
    bool hwChanged = false;
    if (server.hasArg("hw"))
    {
        uint8_t v = static_cast<uint8_t>(hwValue);
        if (v <= 2 && v != hwMode)
        {
            hwMode = v;
            hwChanged = true;
            dashLog("[CFG] HW=" + String(v == 0 ? "LEGACY" : v == 1 ? "HW3"
                                                                    : "HW4"));
        }
    }
    if (server.hasArg("can"))
        canActive = canValue;
    bool profileAutoRequested = server.hasArg("spa") && speedAutoValue;
    if (server.hasArg("sp"))
    {
        uint8_t v = static_cast<uint8_t>(speedValue);
        if (!profileAutoRequested && (v != dashManualSpeedProfile || dashSpeedProfileAuto))
            dashLog("[CFG] Speed profile manual " + String(v));
        dashManualSpeedProfile = v;
        if (!profileAutoRequested)
            dashSpeedProfileAuto = false;
    }
    if (server.hasArg("spa"))
    {
        bool v = speedAutoValue;
        if (v != dashSpeedProfileAuto)
            dashLog("[CFG] Speed profile " + String(v ? "AUTO" : "MANUAL"));
        dashSpeedProfileAuto = v;
    }
    if (server.hasArg("apg"))
    {
        bool v = gateValue;
        if (v != apInjectionGate)
        {
            apInjectionGate = v;
            dashLog("[CFG] AP injection gate " + String(v ? "ON" : "OFF"));
        }
    }
    if (server.hasArg("plgr"))
    {
        uint8_t previous = pluginGetReplayCount();
        pluginSetReplayCount(replayValue);
        if (pluginGetReplayCount() != previous)
            dashLog("[CFG] Plugin replay x" + String(pluginGetReplayCount()));
    }
    if (server.hasArg("hw3OffsetSlew") || server.hasArg("offsetSlew"))
    {
        bool v = slewValue;
        if (v != hw3OffsetSlew)
        {
            hw3OffsetSlew = v;
            dashLog("[CFG] Offset slew " + String(v ? "ON" : "OFF"));
        }
    }
    if (server.hasArg("hw3SlewRate") || server.hasArg("offsetSlewRate"))
    {
        uint8_t v = static_cast<uint8_t>(slewRateValue);
        if (v != hw3SlewRate)
        {
            hw3SlewRate = v;
            dashLog("[CFG] Offset slew rate " + String(hw3SlewRate) + "%/s");
        }
    }
    if (hwChanged)
    {
        dashSwapHandler(hwMode);
        dashApplyFilters();
    }
    dashApplyRuntimeState();
    if (!dashSavePrefs())
    {
        hwMode = oldHw;
        canActive = oldCan;
        dashSpeedProfileAuto = oldSpeedAuto;
        dashManualSpeedProfile = oldSpeed;
        apInjectionGate = oldGate;
        pluginSetReplayCount(oldReplay);
        hw3OffsetSlew = oldSlew;
        hw3SlewRate = oldSlewRate;
        if (hwChanged)
        {
            dashSwapHandler(oldHw);
            dashApplyFilters();
        }
        dashApplyRuntimeState();
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS write failed\"}");
        return;
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleLedBrightness()
{
    if (!server.hasArg("b"))
    {
        server.send(400, "application/json", "{\"ok\":false,\"err\":\"missing b\"}");
        return;
    }
    long raw = 0;
    if (!dashParseLong(server.arg("b"), raw) || raw < 0 || raw > 255)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Brightness must be 0-255\"}");
        return;
    }
    uint8_t v = static_cast<uint8_t>(raw);
    if (v != dashLedBrightness)
    {
        uint8_t previous = dashLedBrightness;
        dashLedBrightness = v;
        dashLog("[CFG] LED brightness " + String(v));
        if (!dashSavePrefs())
        {
            dashLedBrightness = previous;
            server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS write failed\"}");
            return;
        }
#if defined(DASH_RGB_STATUS_LED)
        appRefreshStatusLed(true);
#endif
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleDisable()
{
    bool saved = dashSetCanActive(false, "dashboard");
    server.send(saved ? 200 : 500, "text/plain", saved ? "Injection stopped." : "Injection stopped; NVS write failed.");
}

static void handleReboot()
{
    server.send(200, "text/plain", "Rebooting...");
    delay(200);
    ESP.restart();
}

#ifdef ESP_PLATFORM
static void handleGvretStatus()
{
    String json = "{\"enabled\":";
    json += GvretSerial::enabled.load(std::memory_order_relaxed) ? "true" : "false";
    json += ",\"connected\":";
    json += GvretSerial::clientConnected.load(std::memory_order_relaxed) ? "true" : "false";
    json += ",\"frames\":" + String(GvretSerial::framesSent.load(std::memory_order_relaxed));
    json += ",\"dropped\":" + String(GvretSerial::framesDropped.load(std::memory_order_relaxed));
    json += ",\"bytes\":" + String(GvretSerial::bytesSent.load(std::memory_order_relaxed));
    json += ",\"runtimeMs\":" + String(GvretSerial::runtimeMs());
    json += ",\"maxRuntimeMs\":" + String(GvretSerial::kMaxRunMs);
    json += ",\"protocol\":\"GVRET serial\",\"baud\":115200}";
    server.send(200, "application/json", json);
}

static void handleGvretStart()
{
    bool ok = GvretSerial::start();
    server.send(ok ? 200 : 500, "application/json",
                ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"GVRET task unavailable\"}");
}

static void handleGvretStop()
{
    GvretSerial::stop();
    server.send(200, "application/json", "{\"ok\":true}");
}
#endif

static void handleOtaResult()
{
    if (!server.authenticate(DASH_OTA_USER, DASH_OTA_PASS))
    {
        server.requestAuthentication();
        return;
    }
    bool ok = Update.isFinished() && !Update.hasError();
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

static bool manualOtaAccepted = false;

static void handleOtaUpload()
{
    if (!server.authenticate(DASH_OTA_USER, DASH_OTA_PASS))
        return;
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        dashLog("[OTA] Receiving: " + String(upload.filename.c_str()));
        manualOtaAccepted = Update.begin(upload.totalSize);
        if (!manualOtaAccepted)
            dashLog("[OTA] Begin failed");
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (manualOtaAccepted && Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            manualOtaAccepted = false;
            Update.abort();
            dashLog("[OTA] Write error");
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        bool accepted = manualOtaAccepted;
        manualOtaAccepted = false;
        if (accepted && Update.end(true))
            dashLog("[OTA] Done: " + String(upload.totalSize) + " bytes");
        else
            dashLog("[OTA] End failed");
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        bool accepted = manualOtaAccepted;
        manualOtaAccepted = false;
        if (accepted)
            Update.abort();
        dashLog("[OTA] Upload aborted");
    }
}

// ── PLUGIN MANAGEMENT ───────────────────────────────────────────

static void dashReapplyFiltersWithPlugins()
{
    if (!dashHandler || !dashDriver)
        return;
    // Merge handler + plugin filter IDs
    static constexpr uint8_t kDashMergedFilterMax = 160;
    uint32_t mergedIds[kDashMergedFilterMax];
    uint8_t count = 0;
    const uint32_t *hIds = dashHandler->filterIds();
    uint8_t hCount = dashHandler->filterIdCount();
    for (uint8_t i = 0; i < hCount && count < kDashMergedFilterMax; i++)
        mergedIds[count++] = hIds[i];
    count += pluginGetFilterIds(mergedIds + count, kDashMergedFilterMax - count);
    dashDriver->setFilters(mergedIds, count);
}

static void handlePluginList()
{
    struct PluginSummary
    {
        char name[32];
        char version[16];
        char author[32];
        uint8_t ruleCount;
        bool enabled;
    };
    PluginSummary snapshot[PLUGIN_MAX] = {};
    uint8_t count = 0;
    {
        PluginLockGuard guard;
        count = pluginCount;
        for (uint8_t i = 0; i < count; i++)
        {
            strlcpy(snapshot[i].name, pluginStore[i].name, sizeof(snapshot[i].name));
            strlcpy(snapshot[i].version, pluginStore[i].version, sizeof(snapshot[i].version));
            strlcpy(snapshot[i].author, pluginStore[i].author, sizeof(snapshot[i].author));
            snapshot[i].ruleCount = pluginStore[i].ruleCount;
            snapshot[i].enabled = pluginStore[i].enabled;
        }
    }
    String j = "{\"maxPlugins\":" + String(PLUGIN_MAX) + ",\"plugins\":[";
    for (uint8_t i = 0; i < count; i++)
    {
        if (i)
            j += ",";
        j += "{\"name\":\"" + jsonEscape(snapshot[i].name) + "\"";
        j += ",\"version\":\"" + jsonEscape(snapshot[i].version) + "\"";
        j += ",\"author\":\"" + jsonEscape(snapshot[i].author) + "\"";
        j += ",\"rules\":" + String(snapshot[i].ruleCount);
        j += ",\"priority\":" + String(i + 1);
        j += ",\"enabled\":" + String(snapshot[i].enabled ? "true" : "false") + "}";
    }
    j += "]}";
    server.send(200, "application/json", j);
}

static bool pluginInstallJson(const String &json, const String &url)
{
    PluginData temp;
    if (!pluginParseJson(json, temp))
        return false;

    // Check for duplicate name
    int existing = pluginFindByName(temp.name);
    if (existing < 0 && pluginCount >= PLUGIN_MAX)
        return false;

    uint8_t insertIndex = pluginCount;
    String oldFilename;
    if (existing >= 0)
    {
        insertIndex = (uint8_t)existing;
        oldFilename = pluginStore[existing].filename;
    }

    // Keep path under SPIFFS 31-character object name limit: "/p_" + base + ".json".
    String fname;
    for (size_t i = 0; temp.name[i] != '\0'; i++)
    {
        char c = temp.name[i];
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        bool allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
        fname += allowed ? c : '_';
    }
    const size_t maxSpiffsPathLen = 31;
    const size_t prefixLen = 3; // "/p_"
    const size_t suffixLen = 5; // ".json"
    const size_t maxBaseLen = maxSpiffsPathLen - prefixLen - suffixLen;
    if (fname.length() > maxBaseLen)
        fname = fname.substring(0, maxBaseLen);
    fname += ".json";
    strlcpy(temp.filename, fname.c_str(), sizeof(temp.filename));
    strlcpy(temp.sourceUrl, url.c_str(), sizeof(temp.sourceUrl));
    temp.enabled = false;
    temp.priority = insertIndex;

    {
        PluginLockGuard guard;
        for (uint8_t i = 0; i < pluginCount; i++)
        {
            if (i != insertIndex && strcmp(pluginStore[i].filename, temp.filename) == 0)
                return false;
        }
    }

    if (!pluginSaveToSpiffs(json, temp.filename))
        return false;

    if (existing >= 0)
    {
        if (oldFilename.length() > 0 && oldFilename != temp.filename)
        {
            if (!SPIFFS.remove(pluginFilePath(oldFilename.c_str())))
            {
                SPIFFS.remove(pluginFilePath(temp.filename));
                return false;
            }
        }
        PluginLockGuard guard;
        pluginsLocked = true;
        pluginStore[insertIndex] = temp;
        pluginNormalizePriorities();
        pluginResetDiagnostics();
        pluginsLocked = false;
    }
    else if (!pluginInsert(pluginCount, temp))
    {
        SPIFFS.remove(pluginFilePath(temp.filename));
        return false;
    }

    dashSaveAllPluginStates();
    pluginResetDiagnostics();
    pluginResetPeriodicEmit();

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
    if (url.length() > 1024 || !url.startsWith("https://"))
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"HTTPS URL required\"}");
        return;
    }
    if (!dashStaConnectedSnapshot())
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"WiFi not connected. Configure WiFi first.\"}");
        return;
    }

    HTTPClient http;
    WiFiClientSecure client;
#ifndef ESP_PLATFORM
    client.setInsecure(); // skip cert verification for simplicity
#endif
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
    long parsedIdx = -1;
    if (!dashParseLong(server.arg("idx"), parsedIdx) || parsedIdx < 0 || parsedIdx > 255)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid idx\"}");
        return;
    }
    uint8_t idx = static_cast<uint8_t>(parsedIdx);
    if (idx < pluginCount)
    {
        bool enabled = false;
        char name[32] = {};
        {
            PluginLockGuard guard;
            pluginStore[idx].enabled = !pluginStore[idx].enabled;
            enabled = pluginStore[idx].enabled;
            strlcpy(name, pluginStore[idx].name, sizeof(name));
        }
        pluginResetDiagnostics();
        pluginResetPeriodicEmit();
        dashSchedulePluginStateSave();
        dashReapplyFiltersWithPlugins();
        dashLog("[PLG] " + String(name) + " " + String(enabled ? "enabled" : "disabled"));
        server.send(200, "application/json",
                    String("{\"ok\":true,\"enabled\":") +
                        (enabled ? "true" : "false") + "}");
        return;
    }
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid idx\"}");
}

static void handlePluginRemove()
{
    if (!server.hasArg("idx"))
    {
        server.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    long parsedIdx = -1;
    if (!dashParseLong(server.arg("idx"), parsedIdx) || parsedIdx < 0 || parsedIdx > 255)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid idx\"}");
        return;
    }
    uint8_t idx = static_cast<uint8_t>(parsedIdx);
    if (idx < pluginCount)
    {
        PluginData removedPlugin;
        {
            PluginLockGuard guard;
            removedPlugin = pluginStore[idx];
        }
        String name = removedPlugin.name;
        if (!dashClearPluginState(removedPlugin))
        {
            dashSaveAllPluginStates();
            server.send(500, "application/json", "{\"ok\":false,\"error\":\"Plugin removal failed\"}");
            return;
        }
        if (!pluginRemove(idx))
        {
            dashSaveAllPluginStates();
            server.send(500, "application/json", "{\"ok\":false,\"error\":\"Plugin removal failed\"}");
            return;
        }
        pluginResetPeriodicEmit();
        if (!dashSaveAllPluginStates())
            dashLog("[WARN] Plugin order persistence failed");
        dashReapplyFiltersWithPlugins();
        dashLog("[PLG] Removed: " + name);
        server.send(200, "application/json", "{\"ok\":true}");
        return;
    }
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid idx\"}");
}

static void handlePluginPriority()
{
    if (!server.hasArg("idx") || !server.hasArg("priority"))
    {
        server.send(400, "application/json", "{\"ok\":false}");
        return;
    }

    long idx = -1, priority = -1;
    if (!dashParseLong(server.arg("idx"), idx) || !dashParseLong(server.arg("priority"), priority))
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid priority\"}");
        return;
    }
    if (idx < 0 || idx >= pluginCount || priority < 0 || priority >= pluginCount)
    {
        server.send(400, "application/json", "{\"ok\":false}");
        return;
    }

    if (pluginMove((uint8_t)idx, (uint8_t)priority))
    {
        if (!dashSaveAllPluginStates())
        {
            pluginMove((uint8_t)priority, (uint8_t)idx);
            server.send(500, "application/json", "{\"ok\":false,\"error\":\"Plugin priority persistence failed\"}");
            return;
        }
        pluginResetPeriodicEmit();
        dashReapplyFiltersWithPlugins();
        char movedName[32] = {};
        {
            PluginLockGuard guard;
            strlcpy(movedName, pluginStore[priority].name, sizeof(movedName));
        }
        dashLog("[PLG] Priority: " + String(movedName) + " #" + String(priority + 1));
        server.send(200, "application/json", "{\"ok\":true}");
        return;
    }

    server.send(400, "application/json", "{\"ok\":false}");
}

// ── WIFI STA ────────────────────────────────────────────────────

static bool dashStartAccessPoint(bool withSta)
{
    DashWifiGuard guard;
    WiFi.persistent(false);
    WiFi.mode(withSta ? WIFI_AP_STA : WIFI_AP);
    WiFi.setSleep(false);

    IPAddress apIp(192, 168, 4, 1);
    IPAddress apMask(255, 255, 255, 0);
    WiFi.softAPConfig(apIp, apIp, apMask);

    if (!dashApConfigValid(apSSID, apPass))
        dashUseDefaultApConfig();

    bool ok = WiFi.softAP(apSSID, apPass, kDashApChannel, apHidden ? 1 : 0, kDashApMaxConn);
    if (!ok)
    {
        dashUseDefaultApConfig();
        ok = WiFi.softAP(apSSID, apPass, kDashApChannel, 0, kDashApMaxConn);
    }
    if (!ok)
        dashLog("[WIFI] AP start failed");
    return ok;
}

static void dashBeginSTA()
{
    DashWifiGuard guard;
    if (strlen(staSSID) == 0)
        return;

    if (staStaticIP && (uint32_t)staIP != 0)
    {
        if (WiFi.config(staIP, staGW, staMask, staDNS))
            dashLog("[WIFI] Static IP: " + staIP.toString());
        else
        {
            staStaticIP = false;
            if (!WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE))
            {
                dashLog("[WIFI] Network interface configuration failed");
                staRetryAt = millis() + kDashStaRetryMs;
                return;
            }
            dashLog("[WIFI] Static IP failed; using DHCP");
        }
    }
    else
    {
        if (!WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE))
        {
            dashLog("[WIFI] DHCP configuration failed");
            staRetryAt = millis() + kDashStaRetryMs;
            return;
        }
    }
    WiFi.begin(staSSID, staPass);
    staConnectAttemptActive = true;
    staConnectStartedAt = millis();
    staRetryAt = 0;
    dashLog("[WIFI] Connecting to " + String(staSSID) + "...");
}

static void dashPrepareStaReconnect()
{
    DashWifiGuard guard;
    if (staConnectAttemptActive || staConnected || WiFi.status() == WL_CONNECTED)
        WiFi.disconnect(false, false);
    staConnected = false;
    staConnectAttemptActive = false;
    staRetryAt = 0;
    autoUpdateEligibleAt = 0;
}

static void dashApplyWifiSlot(uint8_t slot)
{
    DashWifiGuard guard;
    if (slot >= wifiNetworkCount)
        return;
    const DashWifiNetwork &n = wifiNetworks[slot];
    strlcpy(staSSID, n.ssid, sizeof(staSSID));
    strlcpy(staPass, n.pass, sizeof(staPass));
    staStaticIP = n.useStatic;
    if (n.useStatic)
    {
        staIP.fromString(n.ip);
        staGW.fromString(n.gw);
        staMask.fromString(n.mask);
        staDNS.fromString(n.dns);
    }
    else
    {
        staIP = IPAddress(0, 0, 0, 0);
    }
    wifiActiveSlot = static_cast<int8_t>(slot);
}

static void dashRotateAndConnect()
{
    DashWifiGuard guard;
    if (wifiNetworkCount == 0)
        return;
    uint8_t next = wifiNextRotateSlot % wifiNetworkCount;
    wifiNextRotateSlot = (next + 1) % wifiNetworkCount;
    dashApplyWifiSlot(next);
    dashLog("[WIFI] Trying slot " + String(next) + ": " + String(staSSID));
    dashStartAccessPoint(true);
    dashBeginSTA();
}

static void dashScheduleSTAConnect(unsigned long delayMs)
{
    DashWifiGuard guard;
    if (strlen(staSSID) == 0)
        return;
    staConnectAttemptActive = false;
    staRetryAt = millis() + delayMs;
}

static void dashPrepareWifiScan()
{
    DashWifiGuard guard;
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
}

static void performAutoUpdate(); // forward decl, defined below

static void dashCheckWifi()
{
    bool runAutoUpdate = false;
    {
        DashWifiGuard guard;
        static unsigned long lastCheck = 0;
        if (wifiNetworkCount == 0)
            return;
        unsigned long now = millis();
        if (!staConnected && !staConnectAttemptActive && staRetryAt > 0 && (long)(now - staRetryAt) >= 0)
        {
            staRetryAt = 0;
            dashRotateAndConnect();
        }

        if (now - lastCheck < 5000)
            return;
        lastCheck = now;

        bool connected = WiFi.status() == WL_CONNECTED;
        if (connected != staConnected)
        {
            staConnected = connected;
            if (connected)
            {
                staConnectAttemptActive = false;
                staRetryAt = 0;
                dashLog("[WIFI] Connected to " + String(staSSID) + " IP: " + WiFi.localIP().toString());
                // Schedule auto-update check 15 s after STA comes up (grace period for other boot work)
                if (autoUpdateEnabled && !autoUpdateDone)
                    autoUpdateEligibleAt = millis() + 15000;
            }
            else
            {
                dashLog("[WIFI] Disconnected from " + String(staSSID));
                staConnectAttemptActive = false;
                staRetryAt = now + kDashStaRetryMs;
            }
        }

        if (!connected && staConnectAttemptActive && now - staConnectStartedAt >= kDashStaConnectTimeoutMs)
        {
            staConnectAttemptActive = false;
            WiFi.disconnect(false, false);
            dashStartAccessPoint(false);
            staRetryAt = now + kDashStaRetryMs;
            dashLog("[WIFI] STA connect timed out; keeping AP-only mode");
        }

        // Fire one-shot auto-update check once eligible
        if (autoUpdateEnabled && !autoUpdateDone && staConnected && autoUpdateEligibleAt > 0 &&
            static_cast<long>(millis() - autoUpdateEligibleAt) >= 0)
        {
            autoUpdateDone = true;
            runAutoUpdate = true;
        }
    }
    if (runAutoUpdate)
        performAutoUpdate();
}

static void handleWifiScan()
{
    DashWifiGuard guard;
    dashPrepareWifiScan();
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

static void dashPersistWifiSlot(uint8_t slot)
{
    if (slot >= wifiNetworkCount)
        return;
    const DashWifiNetwork &n = wifiNetworks[slot];
    prefs.putString(dashWifiKey(slot, "s").c_str(), String(n.ssid));
    prefs.putString(dashWifiKey(slot, "p").c_str(), String(n.pass));
    prefs.putBool(dashWifiKey(slot, "t").c_str(), n.useStatic);
    if (n.useStatic)
    {
        prefs.putString(dashWifiKey(slot, "i").c_str(), String(n.ip));
        prefs.putString(dashWifiKey(slot, "g").c_str(), String(n.gw));
        prefs.putString(dashWifiKey(slot, "m").c_str(), String(n.mask));
        prefs.putString(dashWifiKey(slot, "d").c_str(), String(n.dns));
    }
    else
    {
        prefs.remove(dashWifiKey(slot, "i").c_str());
        prefs.remove(dashWifiKey(slot, "g").c_str());
        prefs.remove(dashWifiKey(slot, "m").c_str());
        prefs.remove(dashWifiKey(slot, "d").c_str());
    }
}

static void dashRemoveWifiSlotKeys(uint8_t slot)
{
    prefs.remove(dashWifiKey(slot, "s").c_str());
    prefs.remove(dashWifiKey(slot, "p").c_str());
    prefs.remove(dashWifiKey(slot, "t").c_str());
    prefs.remove(dashWifiKey(slot, "i").c_str());
    prefs.remove(dashWifiKey(slot, "g").c_str());
    prefs.remove(dashWifiKey(slot, "m").c_str());
    prefs.remove(dashWifiKey(slot, "d").c_str());
}

// Save to slot N (0..count). idx == count means append (new). Reconnect on save.
static void handleWifiConfig()
{
    DashWifiGuard guard;
    DashPrefsGuard prefsGuard;
    if (!server.hasArg("ssid"))
    {
        server.send(200, "application/json", "{\"ok\":true}");
        return;
    }

    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (!dashStaConfigLengthValid(ssid, pass) || dashStaSsidLooksCorrupt(ssid) || ssid.length() == 0)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid SSID or password\"}");
        return;
    }

    int idx = -1;
    if (server.hasArg("idx"))
    {
        long parsedIdx = -1;
        if (!dashParseLong(server.arg("idx"), parsedIdx) || parsedIdx < -1 || parsedIdx > kDashMaxWifiNetworks)
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid network index\"}");
            return;
        }
        idx = static_cast<int>(parsedIdx);
    }
    if (idx < 0 || idx > wifiNetworkCount)
        idx = wifiNetworkCount; // append

    if (idx == kDashMaxWifiNetworks)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Max networks reached\"}");
        return;
    }

    DashWifiNetwork candidate = {};
    dashClearWifiNetwork(candidate);
    strlcpy(candidate.ssid, ssid.c_str(), sizeof(candidate.ssid));
    strlcpy(candidate.pass, pass.c_str(), sizeof(candidate.pass));
    candidate.useStatic = false;
    if (server.hasArg("static") && !dashParseBool(server.arg("static"), candidate.useStatic))
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid static IP flag\"}");
        return;
    }
    if (candidate.useStatic)
    {
        String ip = server.arg("ip");
        String gateway = server.arg("gw");
        String mask = server.arg("mask");
        String dns = server.arg("dns");
        if (!dashStaticIpConfigValid(ip.c_str(), gateway.c_str(), mask.c_str(), dns.c_str()))
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid static IP configuration\"}");
            return;
        }
        strlcpy(candidate.ip, ip.c_str(), sizeof(candidate.ip));
        strlcpy(candidate.gw, gateway.c_str(), sizeof(candidate.gw));
        strlcpy(candidate.mask, mask.c_str(), sizeof(candidate.mask));
        strlcpy(candidate.dns, dns.c_str(), sizeof(candidate.dns));
    }
    DashWifiNetwork previous = wifiNetworks[idx];
    uint8_t previousCount = wifiNetworkCount;
    wifiNetworks[idx] = candidate;
    if (idx == wifiNetworkCount)
        wifiNetworkCount++;

    if (!prefs.begin(PREFS_NS, false))
    {
        wifiNetworks[idx] = previous;
        wifiNetworkCount = previousCount;
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS open failed\"}");
        return;
    }
    prefs.putUChar("wn_cnt", wifiNetworkCount);
    dashPersistWifiSlot(idx);
    if (!prefs.end())
    {
        wifiNetworks[idx] = previous;
        wifiNetworkCount = previousCount;
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS write failed\"}");
        return;
    }

    dashLog("[WIFI] Saved slot " + String(idx) + ": " + ssid);

    // Switch to newly saved slot and connect
    wifiNextRotateSlot = idx;
    dashApplyWifiSlot(idx);
    dashPrepareStaReconnect();

    server.send(200, "application/json", "{\"ok\":true,\"idx\":" + String(idx) + "}");
    dashScheduleSTAConnect(1000);
}

static void handleWifiDelete()
{
    DashWifiGuard guard;
    DashPrefsGuard prefsGuard;
    if (!server.hasArg("idx"))
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing idx\"}");
        return;
    }
    long parsedIdx = -1;
    if (!dashParseLong(server.arg("idx"), parsedIdx))
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"bad idx\"}");
        return;
    }
    int idx = static_cast<int>(parsedIdx);
    if (idx < 0 || idx >= wifiNetworkCount)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"bad idx\"}");
        return;
    }

    String removedSsid = wifiNetworks[idx].ssid;
    DashWifiNetwork previousNetworks[kDashMaxWifiNetworks];
    memcpy(previousNetworks, wifiNetworks, sizeof(wifiNetworks));
    uint8_t previousCount = wifiNetworkCount;
    // Shift slots down
    for (uint8_t i = idx; i + 1 < wifiNetworkCount; i++)
        wifiNetworks[i] = wifiNetworks[i + 1];
    wifiNetworkCount--;
    dashClearWifiNetwork(wifiNetworks[wifiNetworkCount]);

    // Rewrite all slot keys
    if (!prefs.begin(PREFS_NS, false))
    {
        memcpy(wifiNetworks, previousNetworks, sizeof(wifiNetworks));
        wifiNetworkCount = previousCount;
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS open failed\"}");
        return;
    }
    prefs.putUChar("wn_cnt", wifiNetworkCount);
    for (uint8_t i = 0; i < wifiNetworkCount; i++)
        dashPersistWifiSlot(i);
    for (uint8_t i = wifiNetworkCount; i < kDashMaxWifiNetworks; i++)
        dashRemoveWifiSlotKeys(i);
    if (!prefs.end())
    {
        memcpy(wifiNetworks, previousNetworks, sizeof(wifiNetworks));
        wifiNetworkCount = previousCount;
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS write failed\"}");
        return;
    }

    dashLog("[WIFI] Deleted slot " + String(idx) + ": " + removedSsid);

    // Adjust active slot if needed
    if (wifiActiveSlot == idx)
    {
        wifiActiveSlot = -1;
        if (staConnectAttemptActive || staConnected)
        {
            WiFi.disconnect(false, false);
            staConnectAttemptActive = false;
            staConnected = false;
        }
        if (wifiNetworkCount > 0)
        {
            wifiNextRotateSlot = 0;
            dashRotateAndConnect();
        }
        else
        {
            staSSID[0] = 0;
            staPass[0] = 0;
            dashStartAccessPoint(false);
        }
    }
    else if (wifiActiveSlot > idx)
    {
        wifiActiveSlot--;
    }
    if (wifiNextRotateSlot >= wifiNetworkCount)
        wifiNextRotateSlot = 0;

    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleWifiNetworks()
{
    DashWifiGuard guard;
    String j = "{\"max\":";
    j += kDashMaxWifiNetworks;
    j += ",\"count\":";
    j += wifiNetworkCount;
    j += ",\"active\":";
    j += wifiActiveSlot;
    j += ",\"networks\":[";
    for (uint8_t i = 0; i < wifiNetworkCount; i++)
    {
        if (i)
            j += ",";
        const DashWifiNetwork &n = wifiNetworks[i];
        j += "{\"idx\":";
        j += i;
        j += ",\"ssid\":\"" + jsonEscape(n.ssid) + "\"";
        j += ",\"hasPass\":" + String(strlen(n.pass) > 0 ? "true" : "false");
        j += ",\"static\":" + String(n.useStatic ? "true" : "false");
        if (n.useStatic)
        {
            j += ",\"ip\":\"" + String(n.ip) + "\"";
            j += ",\"gw\":\"" + String(n.gw) + "\"";
            j += ",\"mask\":\"" + String(n.mask) + "\"";
            j += ",\"dns\":\"" + String(n.dns) + "\"";
        }
        j += "}";
    }
    j += "]}";
    server.send(200, "application/json", j);
}

static void handleWifiStatus()
{
    DashWifiGuard guard;
    bool stored = wifiNetworkCount > 0;
    bool connectedNow = WiFi.status() == WL_CONNECTED;
    IPAddress staIp = WiFi.localIP();
    bool connected = connectedNow;
    String activeSsid = connectedNow ? WiFi.SSID() : String(staSSID);
    if (dashStaSsidLooksCorrupt(activeSsid))
        activeSsid = "";
    String j = "{\"connected\":";
    j += connected ? "true" : "false";
    j += ",\"ssid\":\"" + jsonEscape(activeSsid) + "\"";
    j += ",\"stored\":" + String(stored ? "true" : "false");
    j += ",\"count\":" + String(wifiNetworkCount);
    j += ",\"active\":" + String(wifiActiveSlot);
    if (connected)
        j += ",\"ip\":\"" + staIp.toString() + "\"";
    j += ",\"static\":" + String(staStaticIP ? "true" : "false");
    if (staStaticIP)
    {
        j += ",\"cfg_ip\":\"" + staIP.toString() + "\"";
        j += ",\"cfg_gw\":\"" + staGW.toString() + "\"";
        j += ",\"cfg_mask\":\"" + staMask.toString() + "\"";
        j += ",\"cfg_dns\":\"" + staDNS.toString() + "\"";
    }
    if (!connected)
        j += ",\"connecting\":" + String(staConnectAttemptActive ? "true" : "false");
    j += "}";
    server.send(200, "application/json", j);
}

// ── AP Config (hotspot name/password) ───────────────────────────

static void handleCanPins()
{
    Preferences canPrefs;
    bool customized = false;
    int tx = -1, rx = -1;
#if defined(DRIVER_TWAI)
    tx = (int)TWAI_TX_PIN;
    rx = (int)TWAI_RX_PIN;
#endif
    if (canPrefs.begin("can", false))
    {
        int storedTx = canPrefs.getChar("tx", -1);
        int storedRx = canPrefs.getChar("rx", -1);
        canPrefs.end();
        if (storedTx >= 0 && GPIO_IS_VALID_OUTPUT_GPIO(storedTx))
        {
            tx = storedTx;
            customized = true;
        }
        if (storedRx >= 0 && GPIO_IS_VALID_GPIO(storedRx))
        {
            rx = storedRx;
            customized = true;
        }
    }
    String j = "{\"tx\":" + String(tx);
    j += ",\"rx\":" + String(rx);
    j += ",\"customized\":" + String(customized ? "true" : "false");
    j += "}";
    server.send(200, "application/json", j);
}

static void handleCanPinsSave()
{
    long parsedTx = -1, parsedRx = -1;
    if (!dashParseLong(server.arg("tx"), parsedTx) || !dashParseLong(server.arg("rx"), parsedRx) ||
        parsedTx < 0 || parsedTx > 255 || parsedRx < 0 || parsedRx > 255)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid pin value\"}");
        return;
    }
    int tx = static_cast<int>(parsedTx);
    int rx = static_cast<int>(parsedRx);

    if (tx < 0 || !GPIO_IS_VALID_OUTPUT_GPIO(tx) || rx < 0 || !GPIO_IS_VALID_GPIO(rx))
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Pin is not valid for this board\"}");
        return;
    }
    if (tx == rx)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"TX and RX must differ\"}");
        return;
    }
    // GPIO 6-11 are reserved for SPI flash on most ESP32 modules
    if (dashCanPinReserved(tx) || dashCanPinReserved(rx))
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
    if (!canPrefs.end())
    {
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS write failed\"}");
        return;
    }

    dashLog("[CAN] Pins saved: TX=" + String(tx) + " RX=" + String(rx) + " (reboot required)");
    server.send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
}

// ── Settings Backup / Restore ───────────────────────────────────

static void handleSettingsExport()
{
    if (!server.authenticate(DASH_OTA_USER, DASH_OTA_PASS))
    {
        server.requestAuthentication();
        return;
    }
    DashWifiGuard wifiGuard;
    DashPrefsGuard prefsGuard;
    String apSsid = apSSID, apPassword = apPass;
    bool beta = updateBetaChannel;
    bool apHid = apHidden;
    bool startAfterAp = apInjectionGate;
    bool h3Slew = hw3OffsetSlew;
    uint8_t h3SlewRate = hw3SlewRate;
    int canTx = -1, canRx = -1;
    Preferences cp;
    if (cp.begin("can", true))
    {
        canTx = cp.getChar("tx", -1);
        canRx = cp.getChar("rx", -1);
        cp.end();
    }

    String j = "{\"version\":\"" FIRMWARE_VERSION "\"";
    j += ",\"ap\":{\"ssid\":\"" + jsonEscape(apSsid) + "\",\"pass\":\"" + jsonEscape(apPassword) + "\",\"hidden\":" + String(apHid ? "true" : "false") + "}";
    j += ",\"wifi\":{\"networks\":[";
    for (uint8_t i = 0; i < wifiNetworkCount; i++)
    {
        if (i)
            j += ",";
        const DashWifiNetwork &network = wifiNetworks[i];
        j += "{\"ssid\":\"" + jsonEscape(network.ssid) + "\",\"pass\":\"" + jsonEscape(network.pass) + "\"";
        j += ",\"static\":" + String(network.useStatic ? "true" : "false");
        j += ",\"ip\":\"" + jsonEscape(network.ip) + "\",\"gw\":\"" + jsonEscape(network.gw) + "\"";
        j += ",\"mask\":\"" + jsonEscape(network.mask) + "\",\"dns\":\"" + jsonEscape(network.dns) + "\"}";
    }
    j += "]}";
    j += ",\"plugins\":{\"replay\":" + String(pluginGetReplayCount()) +
         ",\"startAfterAp\":" + String(startAfterAp ? "true" : "false") + "}";
    j += ",\"hw3\":{\"offsetSlew\":" + String(h3Slew ? "true" : "false") + ",\"slewRate\":" + String(h3SlewRate) + "}";
    j += ",\"dashboard\":{\"hw\":" + String(hwMode) + ",\"can\":" + String(canActive ? "true" : "false");
    j += ",\"speedAuto\":" + String(dashSpeedProfileAuto ? "true" : "false");
    j += ",\"speedProfile\":" + String(dashManualSpeedProfile);
    j += ",\"ledBrightness\":" + String(dashLedBrightness) + "}";
    j += ",\"can\":{\"tx\":" + String(canTx) + ",\"rx\":" + String(canRx) + "}";
    j += ",\"beta\":" + String(beta ? "true" : "false");
    j += ",\"autoUpdate\":" + String(autoUpdateEnabled ? "true" : "false");
    j += "}";

    server.sendHeader("Content-Disposition", "attachment; filename=\"evtools-backup.json\"");
    server.send(200, "application/json", j);
}

static void handleSettingsImport()
{
    if (!server.authenticate(DASH_OTA_USER, DASH_OTA_PASS))
    {
        server.requestAuthentication();
        return;
    }
    DashPrefsGuard prefsGuard;
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

    for (const char *section : {"ap", "wifi", "can", "dashboard", "plugins", "hw3"})
    {
        JsonVariant value = doc[section];
        if (!value.isNull() && !value.is<JsonObject>())
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid settings section\"}");
            return;
        }
    }
    auto validOptionalString = [](JsonVariant value) -> bool
    {
        return value.isNull() || value.is<const char *>();
    };

    DashWifiNetwork importedNetworks[kDashMaxWifiNetworks] = {};
    uint8_t importedNetworkCount = 0;
    bool hasWifiImport = false;
    if (doc["wifi"].is<JsonObject>())
    {
        JsonObject wifi = doc["wifi"];
        if (!wifi["networks"].isNull() && !wifi["networks"].is<JsonArray>())
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid WiFi networks\"}");
            return;
        }
        JsonArray networks = wifi["networks"];
        if (networks)
        {
            hasWifiImport = true;
            if (networks.size() > kDashMaxWifiNetworks)
            {
                server.send(400, "application/json", "{\"ok\":false,\"error\":\"Too many WiFi networks\"}");
                return;
            }
            for (JsonObject item : networks)
            {
                if (!validOptionalString(item["ssid"]) || !validOptionalString(item["pass"]) ||
                    !validOptionalString(item["ip"]) || !validOptionalString(item["gw"]) ||
                    !validOptionalString(item["mask"]) || !validOptionalString(item["dns"]))
                {
                    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid WiFi network\"}");
                    return;
                }
                String ssid = item["ssid"] | "";
                String pass = item["pass"] | "";
                if (ssid.length() == 0 || !dashStaConfigLengthValid(ssid, pass) || dashStaSsidLooksCorrupt(ssid))
                {
                    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid WiFi network\"}");
                    return;
                }
                DashWifiNetwork &network = importedNetworks[importedNetworkCount++];
                strlcpy(network.ssid, ssid.c_str(), sizeof(network.ssid));
                strlcpy(network.pass, pass.c_str(), sizeof(network.pass));
                JsonVariant staticValue = item["static"];
                if (!staticValue.isNull() && !staticValue.is<bool>())
                {
                    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid static IP flag\"}");
                    return;
                }
                network.useStatic = staticValue.isNull() ? false : staticValue.as<bool>();
                String ip = item["ip"] | "";
                String gateway = item["gw"] | "";
                String mask = item["mask"] | "";
                String dns = item["dns"] | "";
                if (network.useStatic &&
                    !dashStaticIpConfigValid(ip.c_str(), gateway.c_str(), mask.c_str(), dns.c_str()))
                {
                    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid static IP configuration\"}");
                    return;
                }
                strlcpy(network.ip, ip.c_str(), sizeof(network.ip));
                strlcpy(network.gw, gateway.c_str(), sizeof(network.gw));
                strlcpy(network.mask, mask.c_str(), sizeof(network.mask));
                strlcpy(network.dns, dns.c_str(), sizeof(network.dns));
            }
        }
        else if (!wifi["ssid"].isNull())
        {
            hasWifiImport = true;
            if (!validOptionalString(wifi["ssid"]) || !validOptionalString(wifi["pass"]) ||
                !validOptionalString(wifi["ip"]) || !validOptionalString(wifi["gw"]) ||
                !validOptionalString(wifi["mask"]) || !validOptionalString(wifi["dns"]))
            {
                server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid WiFi network\"}");
                return;
            }
            String ssid = wifi["ssid"] | "";
            String pass = wifi["pass"] | "";
            if (ssid.length() > 0)
            {
                if (!dashStaConfigLengthValid(ssid, pass) || dashStaSsidLooksCorrupt(ssid))
                {
                    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid WiFi network\"}");
                    return;
                }
                DashWifiNetwork &network = importedNetworks[0];
                importedNetworkCount = 1;
                strlcpy(network.ssid, ssid.c_str(), sizeof(network.ssid));
                strlcpy(network.pass, pass.c_str(), sizeof(network.pass));
                JsonVariant staticValue = wifi["static"];
                if (!staticValue.isNull() && !staticValue.is<bool>())
                {
                    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid static IP flag\"}");
                    return;
                }
                network.useStatic = staticValue.isNull() ? false : staticValue.as<bool>();
                String ip = wifi["ip"] | "";
                String gateway = wifi["gw"] | "";
                String mask = wifi["mask"] | "";
                String dns = wifi["dns"] | "";
                if (network.useStatic &&
                    !dashStaticIpConfigValid(ip.c_str(), gateway.c_str(), mask.c_str(), dns.c_str()))
                {
                    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid static IP configuration\"}");
                    return;
                }
                strlcpy(network.ip, ip.c_str(), sizeof(network.ip));
                strlcpy(network.gw, gateway.c_str(), sizeof(network.gw));
                strlcpy(network.mask, mask.c_str(), sizeof(network.mask));
                strlcpy(network.dns, dns.c_str(), sizeof(network.dns));
            }
        }
    }

    int importedCanTx = -1;
    int importedCanRx = -1;
    bool hasCustomPins = false;
    if (doc["can"].is<JsonObject>())
    {
        JsonVariant txValue = doc["can"]["tx"];
        JsonVariant rxValue = doc["can"]["rx"];
        if ((!txValue.isNull() && !txValue.is<int>()) || (!rxValue.isNull() && !rxValue.is<int>()))
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid CAN pins\"}");
            return;
        }
        importedCanTx = txValue.isNull() ? -1 : txValue.as<int>();
        importedCanRx = rxValue.isNull() ? -1 : rxValue.as<int>();
        hasCustomPins = importedCanTx >= 0 || importedCanRx >= 0;
        if (hasCustomPins &&
            !(importedCanTx >= 0 && GPIO_IS_VALID_OUTPUT_GPIO(importedCanTx) && importedCanRx >= 0 &&
              GPIO_IS_VALID_GPIO(importedCanRx) && importedCanTx != importedCanRx &&
              !dashCanPinReserved(importedCanTx) && !dashCanPinReserved(importedCanRx)))
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid CAN pins\"}");
            return;
        }
    }

    if (doc["ap"].is<JsonObject>())
    {
        if (!validOptionalString(doc["ap"]["ssid"]) || !validOptionalString(doc["ap"]["pass"]))
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid AP configuration\"}");
            return;
        }
        const char *ssid = doc["ap"]["ssid"] | "";
        const char *password = doc["ap"]["pass"] | "";
        bool bothEmpty = *ssid == '\0' && *password == '\0';
        if (!bothEmpty && !dashApConfigValid(ssid, password))
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid AP configuration\"}");
            return;
        }
        if (!doc["ap"]["hidden"].isNull() && !doc["ap"]["hidden"].is<bool>())
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid AP configuration\"}");
            return;
        }
    }

    bool hasDashboardImport = doc["dashboard"].is<JsonObject>();
    int importedHw = hwMode;
    int importedSpeedProfile = dashManualSpeedProfile;
    int importedLedBrightness = dashLedBrightness;
    bool importedCanActive = canActive;
    bool importedSpeedAuto = dashSpeedProfileAuto;
    if (hasDashboardImport)
    {
        JsonObject dashboard = doc["dashboard"];
        auto readBool = [&](const char *key, bool &out) -> bool
        {
            JsonVariant value = dashboard[key];
            if (value.isNull())
                return true;
            if (!value.is<bool>())
                return false;
            out = value.as<bool>();
            return true;
        };
        JsonVariant hwValue = dashboard["hw"];
        JsonVariant speedValue = dashboard["speedProfile"];
        JsonVariant ledValue = dashboard["ledBrightness"];
        if ((!hwValue.isNull() && !hwValue.is<int>()) ||
            (!speedValue.isNull() && !speedValue.is<int>()) ||
            (!ledValue.isNull() && !ledValue.is<int>()) ||
            !readBool("can", importedCanActive) || !readBool("speedAuto", importedSpeedAuto))
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid dashboard settings\"}");
            return;
        }
        if (!hwValue.isNull())
            importedHw = hwValue.as<int>();
        if (!speedValue.isNull())
            importedSpeedProfile = speedValue.as<int>();
        if (!ledValue.isNull())
            importedLedBrightness = ledValue.as<int>();
        if (importedHw < 0 || importedHw > 2 || importedSpeedProfile < 0 ||
            importedSpeedProfile > (importedHw == 2 ? 4 : 2) ||
            importedLedBrightness < 0 || importedLedBrightness > 255)
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid dashboard settings\"}");
            return;
        }
    }
    if (!doc["beta"].isNull() && !doc["beta"].is<bool>())
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid beta setting\"}");
        return;
    }
    if (!doc["autoUpdate"].isNull() && !doc["autoUpdate"].is<bool>())
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid auto-update setting\"}");
        return;
    }
    if (doc["plugins"].is<JsonObject>())
    {
        JsonVariant replay = doc["plugins"]["replay"];
        JsonVariant gate = doc["plugins"]["startAfterAp"];
        if ((!replay.isNull() && (!replay.is<int>() || replay.as<int>() < 1 ||
                                  replay.as<int>() > PLUGIN_REPLAY_COUNT_MAX)) ||
            (!gate.isNull() && !gate.is<bool>()))
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid plugin settings\"}");
            return;
        }
    }
    if (doc["hw3"].is<JsonObject>())
    {
        JsonVariant enabled = doc["hw3"]["offsetSlew"];
        JsonVariant rate = doc["hw3"]["slewRate"];
        if ((!enabled.isNull() && !enabled.is<bool>()) ||
            (!rate.isNull() && (!rate.is<int>() || rate.as<int>() < kHw3SlewRateMin ||
                                rate.as<int>() > kHw3SlewRateMax)))
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid HW3 settings\"}");
            return;
        }
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
        size_t ssidLen = strlen(s);
        size_t passLen = strlen(pw);
        if (ssidLen > 0)
            p.putString("ap_ssid", s);
        if (passLen > 0)
            p.putString("ap_pass", pw);
        if (doc["ap"]["hidden"].is<bool>())
            p.putBool("ap_hidden", doc["ap"]["hidden"].as<bool>());
    }
    if (hasWifiImport)
    {
        p.putUChar("wn_cnt", importedNetworkCount);
        for (uint8_t i = 0; i < kDashMaxWifiNetworks; i++)
        {
            const String ssidKey = dashWifiKey(i, "s");
            const String passKey = dashWifiKey(i, "p");
            const String staticKey = dashWifiKey(i, "t");
            const String ipKey = dashWifiKey(i, "i");
            const String gwKey = dashWifiKey(i, "g");
            const String maskKey = dashWifiKey(i, "m");
            const String dnsKey = dashWifiKey(i, "d");
            if (i < importedNetworkCount)
            {
                const DashWifiNetwork &network = importedNetworks[i];
                p.putString(ssidKey.c_str(), network.ssid);
                p.putString(passKey.c_str(), network.pass);
                p.putBool(staticKey.c_str(), network.useStatic);
                if (network.useStatic)
                {
                    p.putString(ipKey.c_str(), network.ip);
                    p.putString(gwKey.c_str(), network.gw);
                    p.putString(maskKey.c_str(), network.mask);
                    p.putString(dnsKey.c_str(), network.dns);
                }
                else
                {
                    p.remove(ipKey.c_str());
                    p.remove(gwKey.c_str());
                    p.remove(maskKey.c_str());
                    p.remove(dnsKey.c_str());
                }
            }
            else
            {
                p.remove(ssidKey.c_str());
                p.remove(passKey.c_str());
                p.remove(staticKey.c_str());
                p.remove(ipKey.c_str());
                p.remove(gwKey.c_str());
                p.remove(maskKey.c_str());
                p.remove(dnsKey.c_str());
            }
        }
        p.remove("wifi_ssid");
        p.remove("wifi_pass");
        p.remove("wifi_static");
        p.remove("wifi_ip");
        p.remove("wifi_gw");
        p.remove("wifi_mask");
        p.remove("wifi_dns");
    }
    if (doc["beta"].is<bool>())
        p.putBool("update_beta", doc["beta"].as<bool>());
    if (doc["autoUpdate"].is<bool>())
        p.putBool("auto_upd", doc["autoUpdate"].as<bool>());
    if (hasDashboardImport)
    {
        p.putUChar("hw", static_cast<uint8_t>(importedHw));
        p.putUChar("hw_def", DASH_DEFAULT_HW);
        p.putBool("can", importedCanActive);
        p.putBool("sp_auto", importedSpeedAuto);
        p.putUChar("sp_sel", static_cast<uint8_t>(importedSpeedProfile));
        p.putUChar("led_b", static_cast<uint8_t>(importedLedBrightness));
    }
    if (doc["plugins"].is<JsonObject>() && doc["plugins"]["replay"].is<int>())
        p.putUChar("plg_rep", pluginClampReplayCount(doc["plugins"]["replay"].as<int>()));
    if (doc["plugins"].is<JsonObject>() && doc["plugins"]["startAfterAp"].is<bool>())
        p.putBool("ap_gate", doc["plugins"]["startAfterAp"].as<bool>());
    if (doc["hw3"].is<JsonObject>())
    {
        if (doc["hw3"]["offsetSlew"].is<bool>())
            p.putBool("h3_slw", doc["hw3"]["offsetSlew"].as<bool>());
        if (doc["hw3"]["slewRate"].is<int>())
            p.putUChar("h3_srt", dashClampHw3SlewRate(doc["hw3"]["slewRate"].as<int>()));
    }
    if (!p.end())
    {
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS write failed\"}");
        return;
    }

    if (hasCustomPins)
    {
        Preferences cp;
        if (cp.begin("can", false))
        {
            cp.putChar("tx", (int8_t)importedCanTx);
            cp.putChar("rx", (int8_t)importedCanRx);
            if (!cp.end())
            {
                server.send(500, "application/json", "{\"ok\":false,\"error\":\"CAN pin write failed\"}");
                return;
            }
        }
        else
        {
            server.send(500, "application/json", "{\"ok\":false,\"error\":\"CAN pin NVS open failed\"}");
            return;
        }
    }

    dashLog("[BACKUP] Settings imported (reboot required)");
    server.send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
}

static void handleApConfig()
{
    DashWifiGuard guard;
    DashPrefsGuard prefsGuard;
    String newSsid = server.arg("ssid");
    String newPass = server.arg("pass");
    bool hasHidden = server.hasArg("hidden");
    bool newHidden = apHidden;
    if (hasHidden && !dashParseBool(server.arg("hidden"), newHidden))
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid hidden flag\"}");
        return;
    }

    if (newSsid.length() == 0)
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"SSID required\"}");
        return;
    }
    if (newSsid.length() > kDashMaxSsidLen || dashStaSsidLooksCorrupt(newSsid))
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid SSID\"}");
        return;
    }
    if (newPass.length() > 0 && !dashApPasswordLengthValid(newPass.length()))
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Password must be 8-63 characters\"}");
        return;
    }

    char previousSsid[sizeof(apSSID)];
    char previousPass[sizeof(apPass)];
    strlcpy(previousSsid, apSSID, sizeof(previousSsid));
    strlcpy(previousPass, apPass, sizeof(previousPass));
    bool previousHidden = apHidden;
    strlcpy(apSSID, newSsid.c_str(), sizeof(apSSID));
    if (newPass.length() > 0)
        strlcpy(apPass, newPass.c_str(), sizeof(apPass));
    if (hasHidden)
        apHidden = newHidden;

    if (!prefs.begin(PREFS_NS, false))
    {
        strlcpy(apSSID, previousSsid, sizeof(apSSID));
        strlcpy(apPass, previousPass, sizeof(apPass));
        apHidden = previousHidden;
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS open failed\"}");
        return;
    }
    prefs.putString("ap_ssid", newSsid);
    if (newPass.length() > 0)
        prefs.putString("ap_pass", newPass);
    if (hasHidden)
        prefs.putBool("ap_hidden", newHidden);
    if (!prefs.end())
    {
        strlcpy(apSSID, previousSsid, sizeof(apSSID));
        strlcpy(apPass, previousPass, sizeof(apPass));
        apHidden = previousHidden;
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS write failed\"}");
        return;
    }

    dashLog("[WIFI] AP config updated: SSID=" + newSsid + (apHidden ? " (hidden)" : ""));
    server.send(200, "application/json", "{\"ok\":true,\"msg\":\"Saved. Reboot to apply new AP settings.\"}");
}

static void handleApStatus()
{
    DashWifiGuard guard;
    Preferences p;
    bool stored = false;
    if (p.begin(PREFS_NS, false))
    {
        stored = p.isKey("ap_ssid") && p.getString("ap_ssid", "").length() > 0;
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
#ifdef FIRMWARE_ARTIFACT
    return FIRMWARE_ARTIFACT;
#else
    return "firmware-esp32.bin";
#endif
}

static bool isTrustedFirmwareUrl(const String &url)
{
    String suffix = "/" + String(getFirmwareArtifact());
    return url.startsWith("https://github.com/ev-open-can-tools/ev-open-can-tools/releases/download/") &&
           url.endsWith(suffix.c_str()) && url.indexOf('?') < 0 && url.indexOf('#') < 0;
}

// Parse the release version format into (major, minor, patch, preRank, preNum).
// Pre-release rank: 0 = stable (no suffix, sorts highest among same M.m.p),
//                  1 = -alpha.N, 2 = -beta.N, 3 = -rc.N (higher rank = closer to stable).
static bool parseVersion(const String &v, int &maj, int &min, int &pat, int &preRank, int &preNum)
{
    maj = min = pat = 0;
    preRank = 0;
    preNum = 0;
    String parsedVersion = v;
    int plus = parsedVersion.indexOf('+');
    if (plus >= 0)
    {
        if (plus == 0 || static_cast<size_t>(plus + 1) >= parsedVersion.length())
            return false;
        bool previousDot = true;
        for (size_t p = static_cast<size_t>(plus + 1); p < parsedVersion.length(); p++)
        {
            char c = parsedVersion[p];
            bool validChar = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                             (c >= 'a' && c <= 'z') || c == '-';
            if (c == '.')
            {
                if (previousDot)
                    return false;
                previousDot = true;
            }
            else if (validChar)
                previousDot = false;
            else
                return false;
        }
        if (previousDot)
            return false;
        parsedVersion = parsedVersion.substring(0, static_cast<size_t>(plus));
    }

    size_t i = 0;
    size_t len = parsedVersion.length();
    auto readInt = [&](int &out) -> bool
    {
        int val = 0;
        size_t start = i;
        while (i < len && parsedVersion[i] >= '0' && parsedVersion[i] <= '9')
        {
            if (val > 1000000)
                return false;
            val = val * 10 + (parsedVersion[i] - '0');
            i++;
        }
        if (i == start)
            return false;
        if (i - start > 1 && parsedVersion[start] == '0')
            return false;
        out = val;
        return true;
    };
    if (!readInt(maj) || i >= len || parsedVersion[i++] != '.' || !readInt(min) ||
        i >= len || parsedVersion[i++] != '.' || !readInt(pat))
        return false;
    if (i == len)
        return true;
    if (parsedVersion[i++] != '-')
        return false;

    String label = parsedVersion.substring(i);
    label.toLowerCase();
    int dot = label.indexOf('.');
    if (dot <= 0)
        return false;
    String kind = label.substring(0, static_cast<size_t>(dot));
    if (kind == "alpha")
        preRank = 1;
    else if (kind == "beta")
        preRank = 2;
    else if (kind == "rc")
        preRank = 3;
    else
        return false;

    String number = label.substring(static_cast<size_t>(dot + 1));
    if (number.length() == 0)
        return false;
    if (number.length() > 1 && number[0] == '0')
        return false;
    preNum = 0;
    for (size_t n = 0; n < number.length(); n++)
    {
        if (number[n] < '0' || number[n] > '9' || preNum > 1000000)
            return false;
        preNum = preNum * 10 + (number[n] - '0');
    }
    return true;
}

// Returns true iff `candidate` is strictly newer than `current`.
static bool isVersionNewer(const String &candidate, const String &current)
{
    int cM, cm, cp, cR, cN;
    int uM, um, up, uR, uN;
    if (!parseVersion(candidate, cM, cm, cp, cR, cN) ||
        !parseVersion(current, uM, um, up, uR, uN))
        return false;
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
    if (!dashStaConnectedSnapshot())
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"WiFi not connected\"}");
        return;
    }

    WiFiClientSecure client;
#ifndef ESP_PLATFORM
    client.setInsecure();
#endif
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
            downloadUrl = String(asset["browser_download_url"] | "");
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
    bool trustedAsset = downloadUrl.length() > 0 && isTrustedFirmwareUrl(downloadUrl);
    j += ",\"update\":" + String(isNewer && trustedAsset ? "true" : "false");
    j += ",\"beta\":" + String(updateBetaChannel ? "true" : "false");
    j += "}";
    server.send(200, "application/json", j);
}

static void handleUpdateInstall()
{
    if (!server.authenticate(DASH_OTA_USER, DASH_OTA_PASS))
    {
        server.requestAuthentication();
        return;
    }
    if (!dashStaConnectedSnapshot())
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
    if (!isTrustedFirmwareUrl(url))
    {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Untrusted firmware URL\"}");
        return;
    }

    dashLog("[OTA] Starting GitHub update from: " + url);

    WiFiClientSecure client;
#ifndef ESP_PLATFORM
    client.setInsecure();
#endif

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
        server.send(502, "application/json", "{\"ok\":false,\"error\":\"Firmware download failed\"}");
        return;
    }

    int contentLength = http.getSize();
    dashLog(contentLength > 0 ? "[OTA] Downloading " + String(contentLength) + " bytes..."
                              : "[OTA] Downloading chunked firmware...");

    if (!Update.begin(contentLength > 0 ? static_cast<size_t>(contentLength) : UPDATE_SIZE_UNKNOWN))
    {
        dashLog("[OTA] Update.begin failed: " + String(Update.errorString()));
        http.end();
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"OTA initialization failed\"}");
        return;
    }

    WiFiClient *stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);
    http.end();

    if (written == 0 || (contentLength > 0 && written != static_cast<size_t>(contentLength)))
    {
        dashLog("[OTA] Written " + String(written) + " of " + String(contentLength) + " bytes: " + String(Update.errorString()));
        Update.abort();
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"Incomplete firmware download\"}");
        return;
    }

    if (!Update.end(true))
    {
        dashLog("[OTA] Update finalize failed: " + String(Update.errorString()));
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"Firmware validation failed\"}");
        return;
    }

    if (!Update.isFinished())
    {
        dashLog("[OTA] Update not finished");
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"Firmware update did not finish\"}");
        return;
    }

    dashLog("[OTA] Update successful! Rebooting...");
    server.send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
    delay(1000);
    ESP.restart();
}

// Check GitHub for a newer release and, if found, download + install it.
// Blocking; on success calls ESP.restart() and never returns.
static void performAutoUpdate()
{
    if (!dashStaConnectedSnapshot())
        return;

    dashLog("[AUTO-OTA] Checking for updates...");

    WiFiClientSecure client;
#ifndef ESP_PLATFORM
    client.setInsecure();
#endif
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
            downloadUrl = String(asset["browser_download_url"] | "");
            break;
        }
    }
    if (!downloadUrl.length())
    {
        dashLog("[AUTO-OTA] No matching artifact for this build");
        return;
    }
    if (!isTrustedFirmwareUrl(downloadUrl))
    {
        dashLog("[AUTO-OTA] Release asset URL rejected");
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
    if (!Update.begin(len > 0 ? static_cast<size_t>(len) : UPDATE_SIZE_UNKNOWN))
    {
        dashLog("[AUTO-OTA] Update.begin failed: " + String(Update.errorString()));
        http2.end();
        return;
    }
    WiFiClient *stream = http2.getStreamPtr();
    size_t written = Update.writeStream(*stream);
    http2.end();
    if (written == 0 || (len > 0 && written != static_cast<size_t>(len)))
    {
        dashLog("[AUTO-OTA] Written " + String(written) + "/" + String(len) + " bytes: " + String(Update.errorString()));
        Update.abort();
        return;
    }
    if (!Update.end(true))
    {
        dashLog("[AUTO-OTA] Finalize failed: " + String(Update.errorString()));
        return;
    }
    dashLog("[AUTO-OTA] Update successful! Rebooting...");
    delay(1000);
    ESP.restart();
}

static void handleAutoUpdate()
{
    DashPrefsGuard guard;
    if (server.hasArg("enabled"))
    {
        bool requested = false;
        if (!dashParseBool(server.arg("enabled"), requested))
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid auto-update value\"}");
            return;
        }
        bool previous = autoUpdateEnabled;
        autoUpdateEnabled = requested;
        if (!prefs.begin(PREFS_NS, false))
        {
            autoUpdateEnabled = previous;
            server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS open failed\"}");
            return;
        }
        prefs.putBool("auto_upd", autoUpdateEnabled);
        if (!prefs.end())
        {
            autoUpdateEnabled = previous;
            server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS write failed\"}");
            return;
        }
        dashLog("[AUTO-OTA] " + String(autoUpdateEnabled ? "enabled" : "disabled"));
    }
    String j = "{\"ok\":true,\"enabled\":";
    j += autoUpdateEnabled ? "true" : "false";
    j += "}";
    server.send(200, "application/json", j);
}

static void handleUpdateBeta()
{
    DashPrefsGuard guard;
    if (server.hasArg("beta"))
    {
        bool requested = false;
        if (!dashParseBool(server.arg("beta"), requested))
        {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid beta value\"}");
            return;
        }
        bool previous = updateBetaChannel;
        updateBetaChannel = requested;
        if (!prefs.begin(PREFS_NS, false))
        {
            updateBetaChannel = previous;
            server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS open failed\"}");
            return;
        }
        prefs.putBool("update_beta", updateBetaChannel);
        if (!prefs.end())
        {
            updateBetaChannel = previous;
            server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS write failed\"}");
            return;
        }
        dashLog("[OTA] Channel: " + String(updateBetaChannel ? "beta" : "stable"));
    }
    String j = "{\"ok\":true,\"beta\":" + String(updateBetaChannel ? "true" : "false");
    j += ",\"version\":\"" + jsonEscape(FIRMWARE_VERSION) + "\"}";
    server.send(200, "application/json", j);
}

// ── Plugin frame callback wrapper ───────────────────────────────

static void dashPluginProcess(const CanFrame &frame, CanDriver &driver)
{
    if (!dashInjectionActive())
        return;
    pluginProcessFrame(frame, driver);
}

static void webTask(void *)
{
    for (;;)
    {
        try
        {
#ifdef ESP_PLATFORM
            RuntimeDiagnostics::noteWebLoop();
#endif
            ArduinoOTA.handle();
            server.handleClient();
            dashCheckWifi();
        }
        catch (const std::bad_alloc &)
        {
            Serial.println("[ERR] Web task out of memory");
        }
        catch (const std::exception &)
        {
            Serial.println("[ERR] Web task request failed");
        }
        catch (...)
        {
            Serial.println("[ERR] Web task request failed");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static CarManagerBase *handlerPool[3] = {};

static void dashInitHandlers()
{
    static LegacyHandler legacyHandler;
    static HW3Handler hw3Handler;
    static HW4Handler hw4Handler;
    handlerPool[0] = &legacyHandler;
    handlerPool[1] = &hw3Handler;
    handlerPool[2] = &hw4Handler;
    for (int i = 0; i < 3; i++)
    {
        handlerPool[i]->onFrame = mcpDashOnFrame;
    }
}

static void dashSwapHandler(uint8_t mode)
{
    if (mode > 2 || !handlerPool[mode])
        return;
    AppHandlerGuard appGuard;
    CarManagerBase *next = handlerPool[mode];
    CarManagerBase *previous = dashHandler;
    if (previous)
    {
        next->enablePrint = (bool)previous->enablePrint;
        next->APActive = (bool)previous->APActive;
        next->Parked = (bool)previous->Parked;
        next->Summoning = (bool)previous->Summoning;
        next->last921Ms = (uint32_t)previous->last921Ms;
        next->last923Ms = (uint32_t)previous->last923Ms;
        next->last280Ms = (uint32_t)previous->last280Ms;
        next->last390Ms = (uint32_t)previous->last390Ms;
        next->last1016Ms = (uint32_t)previous->last1016Ms;
        next->last1021Ms = (uint32_t)previous->last1021Ms;
    }
    next->ADEnabled = false;
    next->gatewayAutopilot = -1;
    next->dasAutopilotStatus = -1;
    next->sprSeen = false;
    next->lastAca = false;
    next->lastSummonActivityMs = 0;
    next->checkAD = dashCheckADEnabled;
    next->checkNag = dashCheckNagDisabled;
    next->speedProfileAuto = (bool)dashSpeedProfileAuto;
    next->speedProfile = dashClampSpeedProfileForHw(hwMode, dashManualSpeedProfile);
    dashHandler = next;
    dashApplyRuntimeState();
    appActiveHandler = next;
    // Preserve plugin acceptance IDs across handler changes.
    dashReapplyFiltersWithPlugins();
    const char *hwName = "LEGACY";
    if (mode == 1)
        hwName = "HW3";
    else if (mode == 2)
        hwName = "HW4";
    dashLog("[CFG] Handler switched to " + String(hwName));
}

static bool dashInitializeGuards()
{
    return DashDataGuard::initialize() && DashWifiGuard::initialize() &&
           DashPrefsGuard::initialize() && PluginLockGuard::initialize();
}

#if defined(DRIVER_ESP32_EXT_MCP2515)
static void mcpDashboardSetup(CarManagerBase *handler, CanDriver *driver, ESP32_MCP2515Driver *mcpDriver)
{
    if (!dashInitializeGuards())
    {
        Serial.println("[ERR] Dashboard mutex allocation failed");
        return;
    }
    dashHandler = handler;
    dashDriver = driver;
    dashMcpDriver = mcpDriver;
#else
static void mcpDashboardSetup(CarManagerBase *handler, CanDriver *driver)
{
    if (!dashInitializeGuards())
    {
        Serial.println("[ERR] Dashboard mutex allocation failed");
        return;
    }
    dashHandler = handler;
    dashDriver = driver;
#endif
    appDashboardTxObserver = mcpDashOnTxFrame;
    pluginSetDiagnosticsLogger([](const char *message)
                               { dashLog(String(message)); });
    dashResetWriteProbe();

    if (!SPIFFS.begin(true))
        dashLog("[WARN] SPIFFS mount failed");

    dashLoadPrefs();
    dashStartAccessPoint(false);
    if (apHidden)
        dashLog("[WIFI] AP SSID is hidden");
    Serial.printf("[WIFI] AP: %s  IP: %s\n", apSSID, WiFi.softAPIP().toString().c_str());

    dashInitHandlers();
    dashSwapHandler(hwMode);
    dashApplyFilters();

    // Load plugins from SPIFFS
    pluginLoadAll();
    dashRestorePluginStates();
    if (pluginCount > 0)
    {
        dashLog("[PLG] Loaded " + String(pluginCount) + " plugin(s)");
        dashReapplyFiltersWithPlugins();
    }

    // Set plugin processing hook
    appPluginProcess = dashPluginProcess;
    pluginBeforeSend = dashApplyHw3OffsetSlew;

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
    server.on("/config", HTTP_GET, handleConfigGet);
    server.on("/config", HTTP_POST, handleConfig);
    server.on("/led_brightness", HTTP_POST, handleLedBrightness);
    server.on("/disable", HTTP_POST, handleDisable);
    server.on("/reboot", HTTP_POST, handleReboot);
#ifdef ESP_PLATFORM
    server.on("/gvret/status", HTTP_GET, handleGvretStatus);
    server.on("/gvret/start", HTTP_POST, handleGvretStart);
    server.on("/gvret/stop", HTTP_POST, handleGvretStop);
#endif
    server.on("/update", HTTP_POST, handleOtaResult, handleOtaUpload);
    server.on("/plugins", HTTP_GET, handlePluginList);
    server.on("/plugin_upload", HTTP_POST, handlePluginUpload);
    server.on("/plugin_install", HTTP_POST, handlePluginInstallFromUrl);
    server.on("/plugin_toggle", HTTP_POST, handlePluginToggle);
    server.on("/plugin_remove", HTTP_POST, handlePluginRemove);
    server.on("/plugin_priority", HTTP_POST, handlePluginPriority);
    server.on("/ap_config", HTTP_POST, handleApConfig);
    server.on("/ap_status", HTTP_GET, handleApStatus);
    server.on("/can_pins", HTTP_GET, handleCanPins);
    server.on("/can_pins", HTTP_POST, handleCanPinsSave);
    server.on("/settings_export", HTTP_GET, handleSettingsExport);
    server.on("/settings_import", HTTP_POST, handleSettingsImport);
    server.on("/wifi_scan", HTTP_GET, handleWifiScan);
    server.on("/wifi_config", HTTP_POST, handleWifiConfig);
    server.on("/wifi_status", HTTP_GET, handleWifiStatus);
    server.on("/wifi_networks", HTTP_GET, handleWifiNetworks);
    server.on("/wifi_delete", HTTP_POST, handleWifiDelete);
    server.on("/update_check", HTTP_GET, handleUpdateCheck);
    server.on("/update_install", HTTP_POST, handleUpdateInstall);
    server.on("/update_beta", HTTP_POST, handleUpdateBeta);
    server.on("/auto_update", HTTP_GET, handleAutoUpdate);
    server.on("/auto_update", HTTP_POST, handleAutoUpdate);

    server.begin();
    if (!server.started())
        dashLog("[ERR] Dashboard HTTP server failed to start");
    if (strlen(staSSID) > 0)
        dashScheduleSTAConnect(kDashStaBootDelayMs);
#if SOC_CPU_CORES_NUM == 1
    BaseType_t webTaskResult = xTaskCreate(webTask, "web", 12288, nullptr, 1, nullptr);
#else
    BaseType_t webTaskResult = xTaskCreatePinnedToCore(webTask, "web", 12288, nullptr, 1, nullptr, 1);
#endif
    if (webTaskResult != pdPASS)
        dashLog("[ERR] Web maintenance task failed to start");
    Serial.println("[WEB] Dashboard: http://" + WiFi.softAPIP().toString());
    dashLog("[BOOT] Dashboard online; CAN initialization may still be pending");
}

static void mcpDashboardLoop()
{
    if (Update.isRunning())
        return;
    dashFlushPluginStatesIfDue();
    if (dashInjectionActive() && dashDriver)
        pluginEmitPeriodicTick(*dashDriver, millis());
    dashCheckBusHealth();
    bool wentOffline = false;
    {
        DashDataGuard guard;
        if (canOnline && millis() - lastFrameMs > 10000)
        {
            canOnline = false;
            wentOffline = true;
        }
    }
    if (wentOffline)
    {
        dashLog("[CAN] Bus OFFLINE (timeout)");
    }
#if defined(DASH_RGB_STATUS_LED)
    appRefreshStatusLed(false);
#endif
}

#endif
