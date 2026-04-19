#pragma once
// WiFi bridge (NAT forwarding) + DNS filtering for ESP32 dashboard builds.
// Included from mcp2515_dashboard.h inside the ESP32_DASHBOARD guard.
// All symbols are file-static; no linkage outside this translation unit.

#if __has_include("lwip/lwip_napt.h")
#  include "lwip/lwip_napt.h"
#  define BRIDGE_NAPT_AVAILABLE 1
#else
#  define BRIDGE_NAPT_AVAILABLE 0
#endif

#include <WiFiUdp.h>
#include <ArduinoJson.h>

// ─── State ────────────────────────────────────────────────────────────────────

static bool     bridgeEnabled     = false;
static bool     bridgeNaptActive  = false;

static bool     dnsFilterEnabled  = false;
static uint8_t  dnsFilterMode     = 0;     // 0 = blocklist, 1 = allowlist
static String   dnsRulesJson      = "[]";

#define DNS_RULES_MAX 50
static char     dnsRules[DNS_RULES_MAX][64];
static int      dnsRuleCount      = 0;

static TaskHandle_t       dnsTaskHandle    = nullptr;
static volatile uint32_t  dnsQueriesTotal  = 0;
static volatile uint32_t  dnsQueriesBlocked = 0;

// ─── DNS rule helpers ─────────────────────────────────────────────────────────

static void dnsParseRules()
{
    dnsRuleCount = 0;
    JsonDocument doc;
    if (deserializeJson(doc, dnsRulesJson) != DeserializationError::Ok) return;
    if (!doc.is<JsonArray>()) return;
    for (JsonVariant v : doc.as<JsonArray>())
    {
        if (dnsRuleCount >= DNS_RULES_MAX) break;
        const char *s = v.as<const char *>();
        if (!s || s[0] == '\0') continue;
        strlcpy(dnsRules[dnsRuleCount], s, 64);
        for (char *p = dnsRules[dnsRuleCount]; *p; p++)
            *p = tolower((unsigned char)*p);
        dnsRuleCount++;
    }
}

static void dnsExtractQName(const uint8_t *pkt, int len, char *out, size_t outSize)
{
    int pos = 12, outPos = 0;
    while (pos < len && pkt[pos] != 0 && outPos < (int)outSize - 1)
    {
        uint8_t llen = pkt[pos++];
        if (llen > 63 || pos + llen > len) break;
        if (outPos > 0) out[outPos++] = '.';
        for (int i = 0; i < llen && outPos < (int)outSize - 1; i++)
            out[outPos++] = (char)tolower((unsigned char)pkt[pos++]);
    }
    out[outPos] = '\0';
}

static bool dnsDomainMatches(const char *query, const char *rule)
{
    if (rule[0] == '*' && rule[1] == '.')
    {
        const char *suffix = rule + 1;
        size_t qlen = strlen(query), slen = strlen(suffix);
        if (qlen >= slen && strcmp(query + qlen - slen, suffix) == 0) return true;
        if (strcmp(query, suffix + 1) == 0) return true;
        return false;
    }
    return strcmp(query, rule) == 0;
}

static bool dnsShouldBlock(const char *qname)
{
    if (dnsRuleCount == 0)
        return dnsFilterMode == 1; // allowlist with no rules → block everything
    bool matched = false;
    for (int i = 0; i < dnsRuleCount; i++)
        if (dnsDomainMatches(qname, dnsRules[i])) { matched = true; break; }
    return dnsFilterMode == 0 ? matched : !matched;
}

static int dnsBuildNxdomain(const uint8_t *query, int qlen, uint8_t *reply, int replySize)
{
    if (qlen < 12 || qlen > replySize) return -1;
    memcpy(reply, query, qlen);
    reply[2] = 0x81; // QR=1 RD=1
    reply[3] = 0x83; // RA=1 RCODE=3 (NXDOMAIN)
    reply[6] = reply[7] = 0;  // ANCOUNT=0
    reply[8] = reply[9] = 0;  // NSCOUNT=0
    reply[10] = reply[11] = 0; // ARCOUNT=0
    return qlen;
}

static IPAddress dnsGetUpstream()
{
    if (staConnected)
    {
        IPAddress d = WiFi.dnsIP(0);
        if ((uint32_t)d != 0) return d;
    }
    return IPAddress(8, 8, 8, 8);
}

// ─── DNS proxy FreeRTOS task ──────────────────────────────────────────────────

#define DNS_PROXY_STACK      4096
#define DNS_BUF_SIZE         512
#define DNS_QUERY_TIMEOUT_MS 3000

static void dnsProxyTask(void *)
{
    WiFiUDP udp;
    IPAddress apIp = WiFi.softAPIP();
    if (!udp.begin(apIp, 53))
    {
        dashLog("[DNS] Failed to bind UDP :53");
        dnsTaskHandle = nullptr;
        vTaskDelete(nullptr);
        return;
    }
    dashLog("[DNS] Proxy started on " + apIp.toString() + ":53");

    uint8_t buf[DNS_BUF_SIZE];
    for (;;)
    {
        int pktSize = udp.parsePacket();
        if (pktSize <= 0) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }
        if (pktSize > DNS_BUF_SIZE) { udp.flush(); continue; }

        IPAddress  clientIp   = udp.remoteIP();
        uint16_t   clientPort = udp.remotePort();
        int n = udp.read(buf, DNS_BUF_SIZE);
        if (n < 12) continue;

        char qname[256];
        dnsExtractQName(buf, n, qname, sizeof(qname));

        if (dnsFilterEnabled && dnsShouldBlock(qname))
        {
            dnsQueriesBlocked++;
            dnsQueriesTotal++;
            uint8_t reply[DNS_BUF_SIZE];
            int rlen = dnsBuildNxdomain(buf, n, reply, sizeof(reply));
            if (rlen > 0)
            {
                udp.beginPacket(clientIp, clientPort);
                udp.write(reply, rlen);
                udp.endPacket();
            }
            continue;
        }
        dnsQueriesTotal++;

        IPAddress upstream = dnsGetUpstream();
        WiFiUDP fwd;
        fwd.begin(0);
        fwd.beginPacket(upstream, 53);
        fwd.write(buf, n);
        fwd.endPacket();

        unsigned long t0 = millis();
        int rn = 0;
        uint8_t rbuf[DNS_BUF_SIZE];
        while (millis() - t0 < DNS_QUERY_TIMEOUT_MS)
        {
            rn = fwd.parsePacket();
            if (rn > 0) break;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        if (rn > 0 && rn <= DNS_BUF_SIZE)
        {
            fwd.read(rbuf, rn);
            udp.beginPacket(clientIp, clientPort);
            udp.write(rbuf, rn);
            udp.endPacket();
        }
        fwd.stop();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void dnsProxyTaskStart()
{
    if (dnsTaskHandle != nullptr) return;
    xTaskCreatePinnedToCore(dnsProxyTask, "dns_proxy", DNS_PROXY_STACK,
                            nullptr, 1, &dnsTaskHandle, 0);
}

static void dnsProxyTaskStop()
{
    if (dnsTaskHandle == nullptr) return;
    vTaskDelete(dnsTaskHandle);
    dnsTaskHandle    = nullptr;
    dnsQueriesTotal  = 0;
    dnsQueriesBlocked = 0;
    dashLog("[DNS] Proxy stopped");
}

// ─── NAT bridge ───────────────────────────────────────────────────────────────

static bool bridgeNaptEnable()
{
#if !BRIDGE_NAPT_AVAILABLE
    dashLog("[BRIDGE] NAPT not available in this SDK build");
    return false;
#else
    if (bridgeNaptActive) return true;
    uint32_t apIp = (uint32_t)WiFi.softAPIP();
    if (apIp == 0) { dashLog("[BRIDGE] softAP IP not ready"); return false; }
    esp_err_t ret = ip_napt_enable(apIp, 1);
    if (ret == ESP_OK)
    {
        bridgeNaptActive = true;
        dashLog("[BRIDGE] NAT enabled  AP=" + WiFi.softAPIP().toString()
                + "  uplink=" + WiFi.localIP().toString());
        return true;
    }
    dashLog("[BRIDGE] ip_napt_enable failed: " + String(ret));
    return false;
#endif
}

static void bridgeNaptDisable()
{
#if BRIDGE_NAPT_AVAILABLE
    if (!bridgeNaptActive) return;
    uint32_t apIp = (uint32_t)WiFi.softAPIP();
    ip_napt_enable(apIp, 0);
    bridgeNaptActive = false;
    dashLog("[BRIDGE] NAT disabled");
#endif
}

static void bridgeApplyState()
{
    bool shouldBeActive = bridgeEnabled && staConnected;
    if (shouldBeActive && !bridgeNaptActive)  bridgeNaptEnable();
    else if (!shouldBeActive && bridgeNaptActive) bridgeNaptDisable();

    if (dnsFilterEnabled && dnsTaskHandle == nullptr) dnsProxyTaskStart();
    else if (!dnsFilterEnabled && dnsTaskHandle != nullptr) dnsProxyTaskStop();
}

// ─── NVS persistence ─────────────────────────────────────────────────────────

static void bridgeLoadPrefs()
{
    // prefs is already open (called from dashLoadPrefs which opens it)
    bridgeEnabled    = prefs.getBool("bridge_en",  false);
    dnsFilterEnabled = prefs.getBool("dns_flt_en", false);
    dnsFilterMode    = prefs.getUChar("dns_mode",  0);
    dnsRulesJson     = prefs.isKey("dns_rules")
                       ? prefs.getString("dns_rules", "[]") : "[]";
    dnsParseRules();
    dashLog("[BRIDGE] bridge=" + String(bridgeEnabled ? "ON" : "OFF")
            + " dns=" + String(dnsFilterEnabled ? "ON" : "OFF")
            + " rules=" + String(dnsRuleCount));
}

static void bridgeSavePrefs()
{
    prefs.begin(PREFS_NS, false);
    prefs.putBool("bridge_en",   bridgeEnabled);
    prefs.putBool("dns_flt_en",  dnsFilterEnabled);
    prefs.putUChar("dns_mode",   dnsFilterMode);
    prefs.putString("dns_rules", dnsRulesJson);
    prefs.end();
}

// ─── HTTP handlers ────────────────────────────────────────────────────────────

static void handleBridgeStatus()
{
    String j = "{";
    j += "\"bridge_en\":"    + String(bridgeEnabled     ? "true" : "false");
    j += ",\"napt_active\":" + String(bridgeNaptActive  ? "true" : "false");
    j += ",\"dns_flt_en\":"  + String(dnsFilterEnabled  ? "true" : "false");
    j += ",\"dns_mode\":"    + String(dnsFilterMode);
    j += ",\"dns_total\":"   + String(dnsQueriesTotal);
    j += ",\"dns_blocked\":" + String(dnsQueriesBlocked);
    j += ",\"sta_connected\":" + String(staConnected ? "true" : "false");
    j += ",\"sta_ssid\":\""  + jsonEscape(staSSID) + "\"";
    if (staConnected)
        j += ",\"sta_ip\":\"" + WiFi.localIP().toString() + "\"";
    j += ",\"ap_clients\":"  + String(WiFi.softAPgetStationNum());
    j += ",\"ap_ip\":\""     + WiFi.softAPIP().toString() + "\"";
    j += ",\"dns_rules\":"   + dnsRulesJson; // raw JSON array for UI textarea population
    j += "}";
    server.send(200, "application/json", j);
}

static void handleBridgeConfig()
{
    bool changed = false;
    if (server.hasArg("bridge_en"))
    {
        bridgeEnabled = server.arg("bridge_en") == "1";
        changed = true;
    }
    if (server.hasArg("dns_flt_en"))
    {
        dnsFilterEnabled = server.arg("dns_flt_en") == "1";
        changed = true;
    }
    if (server.hasArg("dns_mode"))
    {
        dnsFilterMode = (uint8_t)(server.arg("dns_mode").toInt() == 1 ? 1 : 0);
        changed = true;
    }
    if (server.hasArg("dns_rules"))
    {
        String rules = server.arg("dns_rules");
        if (rules.length() > 3800)
        {
            server.send(400, "application/json",
                        "{\"ok\":false,\"error\":\"Rules list too long (max 3800 bytes)\"}");
            return;
        }
        JsonDocument doc;
        if (deserializeJson(doc, rules) != DeserializationError::Ok || !doc.is<JsonArray>())
        {
            server.send(400, "application/json",
                        "{\"ok\":false,\"error\":\"Invalid rules JSON\"}");
            return;
        }
        dnsRulesJson = rules;
        dnsParseRules();
        changed = true;
    }
    if (changed)
    {
        bridgeSavePrefs();
        bridgeApplyState();
        dashLog("[BRIDGE] Config updated: bridge=" + String(bridgeEnabled ? "ON" : "OFF")
                + " dns=" + String(dnsFilterEnabled ? "ON" : "OFF")
                + " mode=" + String(dnsFilterMode == 0 ? "block" : "allow")
                + " rules=" + String(dnsRuleCount));
    }
    server.send(200, "application/json", "{\"ok\":true}");
}
