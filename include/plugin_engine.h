#pragma once

#if defined(ESP32_DASHBOARD) && (!defined(NATIVE_BUILD) || defined(PLUGIN_ENGINE_NATIVE_TEST))

#include <ArduinoJson.h>
#if defined(NATIVE_BUILD)
#include <string>
using String = std::string;
#else
#include <SPIFFS.h>
#endif
#include "can_frame_types.h"
#include "can_helpers.h"
#include "drivers/can_driver.h"

#define PLUGIN_MAX 8
#define PLUGIN_RULES_MAX 16
#define PLUGIN_OPS_MAX 16
#ifndef PLUGIN_FILTER_IDS_MAX
#define PLUGIN_FILTER_IDS_MAX 32
#endif
#ifndef PLUGIN_REPLAY_COUNT
#define PLUGIN_REPLAY_COUNT 1
#endif
#ifndef PLUGIN_REPLAY_COUNT_MAX
#define PLUGIN_REPLAY_COUNT_MAX 20
#endif
#define PLUGIN_PERIODIC_INTERVAL_DEFAULT_MS 100
#define PLUGIN_PERIODIC_INTERVAL_MIN_MS 10
#define PLUGIN_PERIODIC_INTERVAL_MAX_MS 5000
#define PLUGIN_GTW_UDS_REQUEST_ID 0x684
#define PLUGIN_GTW_UDS_RESPONSE_ID 0x685
#define PLUGIN_GTW_UDS_KEEPALIVE_MS 2000 // TesterPresent cadence once session is active
#define PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS 400
#define PLUGIN_GTW_UDS_RETRY_BACKOFF_MS 5000 // after NRC or timeout, wait before retrying sequence
#define PLUGIN_GTW_UDS_SEED_MAX 6

enum PluginOpType : uint8_t
{
    OP_SET_BIT = 0,
    OP_SET_BYTE = 1,
    OP_OR_BYTE = 2,
    OP_AND_BYTE = 3,
    OP_CHECKSUM = 4,
    OP_COUNTER = 5,
    OP_EMIT_PERIODIC = 6,
};

struct PluginOp
{
    PluginOpType type;
    uint8_t index; // bit (0-63) or byte (0-7) index
    uint8_t value;
    uint8_t mask; // for SET_BYTE
    uint16_t intervalMs;
    bool gtwSilent;
};

enum PluginGtwUdsState : uint8_t
{
    GTW_UDS_IDLE = 0,           // no silencing attempt in progress
    GTW_UDS_SESSION_REQ = 1,    // 0x10 0x03 sent, waiting for positive response
    GTW_UDS_SEED_REQ = 2,       // 0x27 0x01 sent, waiting for seed
    GTW_UDS_KEY_SENT = 3,       // 0x27 0x02 sent, waiting for positive response
    GTW_UDS_COMM_CTRL_SENT = 4, // 0x28 0x01 0x01 sent, waiting for positive response
    GTW_UDS_ACTIVE = 5,         // silence is active; periodic 0x3E TesterPresent keeps it alive
    GTW_UDS_FAILED = 6,         // NRC/timeout — back off before retry
};

struct PluginGtwUdsMachine
{
    PluginGtwUdsState state;
    unsigned long stateEnteredAt;
    unsigned long nextActionAt;
    unsigned long retryAfterMs;
    uint8_t lastNrc;
    uint8_t seed[PLUGIN_GTW_UDS_SEED_MAX];
    uint8_t seedLen;
    uint8_t bus;
    uint8_t lastSeed[PLUGIN_GTW_UDS_SEED_MAX];
    uint8_t lastSeedLen;
    uint8_t lastKey[PLUGIN_GTW_UDS_SEED_MAX];
    uint8_t lastKeyLen;
};

struct PluginPeriodicEmitState
{
    bool active;
    bool gtwSilent;
    bool checksum;
    CanFrame frame;
    PluginOp counterOps[PLUGIN_OPS_MAX];
    uint8_t counterOpCount;
    uint16_t intervalMs;
    unsigned long nextFrameAt;
    PluginGtwUdsMachine uds;
};

struct PluginRule
{
    uint32_t canId;
    int16_t mux; // -1 = match any mux
    uint8_t muxMask;
    uint8_t busMask;
    PluginOp ops[PLUGIN_OPS_MAX];
    uint8_t opCount;
    bool sendAfter;
};

struct PluginData
{
    char name[32];
    char version[16];
    char author[32];
    char filename[32];
    char sourceUrl[200];
    bool enabled;
    uint8_t priority;
    PluginRule rules[PLUGIN_RULES_MAX];
    uint8_t ruleCount;
    uint32_t filterIds[PLUGIN_FILTER_IDS_MAX];
    uint8_t filterIdCount;
};

static PluginData pluginStore[PLUGIN_MAX];
static uint8_t pluginCount = 0;
static volatile bool pluginsLocked = false;
static PluginPeriodicEmitState pluginPeriodicEmit = {};
static bool (*pluginBeforeSend)(CanFrame &modified, const CanFrame &original) = nullptr;

static uint8_t pluginClampReplayCount(int32_t count)
{
    if (count < 1)
        return 1;
    if (count > PLUGIN_REPLAY_COUNT_MAX)
        return PLUGIN_REPLAY_COUNT_MAX;
    return static_cast<uint8_t>(count);
}

static uint8_t pluginReplayCount = pluginClampReplayCount(PLUGIN_REPLAY_COUNT);

static void pluginSetReplayCount(int32_t count)
{
    pluginReplayCount = pluginClampReplayCount(count);
}

static uint8_t pluginGetReplayCount()
{
    return pluginReplayCount;
}

static uint16_t pluginClampPeriodicInterval(int32_t intervalMs)
{
    if (intervalMs < PLUGIN_PERIODIC_INTERVAL_MIN_MS)
        return PLUGIN_PERIODIC_INTERVAL_MIN_MS;
    if (intervalMs > PLUGIN_PERIODIC_INTERVAL_MAX_MS)
        return PLUGIN_PERIODIC_INTERVAL_MAX_MS;
    return static_cast<uint16_t>(intervalMs);
}

static void pluginResetPeriodicEmit()
{
    pluginPeriodicEmit = {};
}

static bool pluginGtwSilentSupported()
{
#if defined(PLUGIN_GTW_UDS_KEY_READY)
    return true;
#else
    return false;
#endif
}

// ── GTW UDS SILENCING KEY HOOK ──────────────────────────────────
//
// SecurityAccess key computation. Tesla uses a proprietary seed-to-key
// algorithm. Without the real algorithm the gateway answers with NRC
// invalidKey and silencing will not take effect.
//
// Define PLUGIN_GTW_UDS_KEY_READY as the byte used by the current
// seed-to-key algorithm. Without it, gtw_silent is ignored at parse time.

#if defined(PLUGIN_GTW_UDS_KEY_READY)
static_assert(PLUGIN_GTW_UDS_KEY_READY >= 0 && PLUGIN_GTW_UDS_KEY_READY <= 0xFF,
              "PLUGIN_GTW_UDS_KEY_READY must be a byte value like 0x12");

static bool pluginGtwUdsComputeKey(const uint8_t *seed, uint8_t seedLen,
                                   uint8_t *outKey, uint8_t &outLen)
{
    if (seedLen == 0 || seedLen > PLUGIN_GTW_UDS_SEED_MAX)
        return false;
    const uint8_t xorKey = static_cast<uint8_t>(PLUGIN_GTW_UDS_KEY_READY);
    for (uint8_t i = 0; i < seedLen; i++)
        outKey[i] = seed[i] ^ xorKey;
    outLen = seedLen;
    return true;
}
#else
static bool pluginGtwUdsComputeKey(const uint8_t *seed, uint8_t seedLen,
                                   uint8_t *outKey, uint8_t &outLen)
{
    (void)seed;
    (void)seedLen;
    (void)outKey;
    outLen = 0;
    return false;
}
#endif

// ── GTW UDS STATE MACHINE ───────────────────────────────────────

static CanFrame pluginMakeUdsRequest(const uint8_t *payload, uint8_t len, uint8_t bus)
{
    CanFrame frame;
    frame.id = PLUGIN_GTW_UDS_REQUEST_ID;
    frame.dlc = 8;
    frame.bus = bus;
    // ISO-TP single frame PCI: high nibble = 0, low nibble = length
    frame.data[0] = len & 0x0F;
    for (uint8_t i = 0; i < len && i < 7; i++)
        frame.data[1 + i] = payload[i];
    for (uint8_t i = 1 + len; i < 8; i++)
        frame.data[i] = 0x00;
    return frame;
}

static void pluginGtwUdsEnter(PluginGtwUdsMachine &m, PluginGtwUdsState next, unsigned long now,
                              unsigned long actionDelayMs)
{
    m.state = next;
    m.stateEnteredAt = now;
    m.nextActionAt = now + actionDelayMs;
}

static void pluginGtwUdsFail(PluginGtwUdsMachine &m, uint8_t nrc, unsigned long now)
{
    m.lastNrc = nrc;
    m.state = GTW_UDS_FAILED;
    m.stateEnteredAt = now;
    m.nextActionAt = now + m.retryAfterMs;
}

// Returns true if the frame was a GTW UDS response that advanced the machine.
static bool pluginGtwUdsHandleResponse(PluginGtwUdsMachine &m, const CanFrame &frame, unsigned long now)
{
    if (frame.id != PLUGIN_GTW_UDS_RESPONSE_ID || frame.dlc < 2)
        return false;

    // Only single-frame ISO-TP responses are expected (all targeted services fit in 8 bytes).
    uint8_t pciType = frame.data[0] >> 4;
    if (pciType != 0x0)
        return false;

    uint8_t len = frame.data[0] & 0x0F;
    if (len < 1 || len > 7)
        return false;

    uint8_t sid = frame.data[1];

    // Negative response: 0x7F <requestedSid> <NRC>
    if (sid == 0x7F && len >= 3)
    {
        uint8_t nrc = frame.data[3];
        // 0x78 = responsePending — keep waiting, don't fail yet.
        // Reset both the send-sentinel and the timeout window so we neither
        // resend nor abort prematurely.
        if (nrc == 0x78)
        {
            m.stateEnteredAt = now;
            m.nextActionAt = now + PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS;
            return true;
        }
        pluginGtwUdsFail(m, nrc, now);
        return true;
    }

    switch (m.state)
    {
    case GTW_UDS_SESSION_REQ:
        if (sid == 0x50 && len >= 2 && frame.data[2] == 0x03)
        {
            pluginGtwUdsEnter(m, GTW_UDS_SEED_REQ, now, 0);
            return true;
        }
        break;

    case GTW_UDS_SEED_REQ:
        if (sid == 0x67 && len >= 3 && frame.data[2] == 0x01)
        {
            uint8_t seedLen = len - 2;
            if (seedLen > PLUGIN_GTW_UDS_SEED_MAX)
                seedLen = PLUGIN_GTW_UDS_SEED_MAX;
            for (uint8_t i = 0; i < seedLen; i++)
                m.seed[i] = frame.data[3 + i];
            m.seedLen = seedLen;
            pluginGtwUdsEnter(m, GTW_UDS_KEY_SENT, now, 0);
            return true;
        }
        break;

    case GTW_UDS_KEY_SENT:
        if (sid == 0x67 && len >= 2 && frame.data[2] == 0x02)
        {
            pluginGtwUdsEnter(m, GTW_UDS_COMM_CTRL_SENT, now, 0);
            return true;
        }
        break;

    case GTW_UDS_COMM_CTRL_SENT:
        if (sid == 0x68 && len >= 2 && frame.data[2] == 0x01)
        {
            pluginGtwUdsEnter(m, GTW_UDS_ACTIVE, now, PLUGIN_GTW_UDS_KEEPALIVE_MS);
            return true;
        }
        break;

    default:
        break;
    }

    return false;
}

// Drives the UDS state machine forward: sends the next request if ready, or
// handles timeouts. Caller must check pluginPeriodicEmit.gtwSilent first.
static void pluginGtwUdsTick(PluginGtwUdsMachine &m, CanDriver &driver, unsigned long now)
{
    if ((long)(now - m.nextActionAt) < 0)
        return;

    switch (m.state)
    {
    case GTW_UDS_IDLE:
    {
        const uint8_t payload[] = {0x10, 0x03}; // DiagnosticSessionControl → ExtendedSession
        driver.send(pluginMakeUdsRequest(payload, sizeof(payload), m.bus));
        pluginGtwUdsEnter(m, GTW_UDS_SESSION_REQ, now, PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS);
        break;
    }
    case GTW_UDS_SESSION_REQ:
        if ((long)(now - m.stateEnteredAt) >= PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS)
            pluginGtwUdsFail(m, 0xFF, now); // 0xFF = internal timeout marker
        break;

    case GTW_UDS_SEED_REQ:
        if (m.stateEnteredAt == m.nextActionAt)
        {
            const uint8_t payload[] = {0x27, 0x01}; // SecurityAccess requestSeed
            driver.send(pluginMakeUdsRequest(payload, sizeof(payload), m.bus));
            m.nextActionAt = now + PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS;
        }
        else if ((long)(now - m.stateEnteredAt) >= PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS)
            pluginGtwUdsFail(m, 0xFF, now);
        break;

    case GTW_UDS_KEY_SENT:
        if (m.stateEnteredAt == m.nextActionAt)
        {
            uint8_t key[PLUGIN_GTW_UDS_SEED_MAX];
            uint8_t keyLen = 0;
            if (!pluginGtwUdsComputeKey(m.seed, m.seedLen, key, keyLen))
            {
                pluginGtwUdsFail(m, 0xFE, now); // 0xFE = local key failure
                break;
            }
            m.lastSeedLen = m.seedLen;
            for (uint8_t i = 0; i < m.seedLen; i++)
                m.lastSeed[i] = m.seed[i];
            m.lastKeyLen = keyLen;
            for (uint8_t i = 0; i < keyLen; i++)
                m.lastKey[i] = key[i];
            uint8_t payload[2 + PLUGIN_GTW_UDS_SEED_MAX];
            payload[0] = 0x27;
            payload[1] = 0x02;
            for (uint8_t i = 0; i < keyLen; i++)
                payload[2 + i] = key[i];
            driver.send(pluginMakeUdsRequest(payload, 2 + keyLen, m.bus));
            m.nextActionAt = now + PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS;
        }
        else if ((long)(now - m.stateEnteredAt) >= PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS)
            pluginGtwUdsFail(m, 0xFF, now);
        break;

    case GTW_UDS_COMM_CTRL_SENT:
        if (m.stateEnteredAt == m.nextActionAt)
        {
            // 0x28 CommunicationControl: enableRxAndDisableTx (0x01), normalCommunication (0x01)
            const uint8_t payload[] = {0x28, 0x01, 0x01};
            driver.send(pluginMakeUdsRequest(payload, sizeof(payload), m.bus));
            m.nextActionAt = now + PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS;
        }
        else if ((long)(now - m.stateEnteredAt) >= PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS)
            pluginGtwUdsFail(m, 0xFF, now);
        break;

    case GTW_UDS_ACTIVE:
    {
        const uint8_t payload[] = {0x3E, 0x00}; // TesterPresent
        driver.send(pluginMakeUdsRequest(payload, sizeof(payload), m.bus));
        m.nextActionAt = now + PLUGIN_GTW_UDS_KEEPALIVE_MS;
        break;
    }

    case GTW_UDS_FAILED:
        // Back-off elapsed → retry the full sequence.
        pluginGtwUdsEnter(m, GTW_UDS_IDLE, now, 0);
        break;
    }
}

// ── JSON PARSING ────────────────────────────────────────────────

static uint8_t pluginDefaultMuxMask(int16_t mux)
{
    if (mux < 0)
        return 0;
    return mux > 7 ? 0xFF : 0x07;
}

static bool pluginBusTokenEquals(const char *token, uint8_t len, const char *expected)
{
    for (uint8_t i = 0; i < len || expected[i] != '\0'; i++)
    {
        char a = i < len ? token[i] : '\0';
        if (a >= 'a' && a <= 'z')
            a = static_cast<char>(a - 'a' + 'A');
        char b = expected[i];
        if (a != b)
            return false;
    }
    return true;
}

static uint8_t pluginBusMaskForToken(const char *token, uint8_t len)
{
    if (len == 0 || pluginBusTokenEquals(token, len, "ANY"))
        return CAN_BUS_ANY;
    if (pluginBusTokenEquals(token, len, "CH"))
        return CAN_BUS_CH;
    if (pluginBusTokenEquals(token, len, "VEH"))
        return CAN_BUS_VEH;
    if (pluginBusTokenEquals(token, len, "PARTY"))
        return CAN_BUS_PARTY;
    return CAN_BUS_ANY;
}

static uint8_t pluginParseBusString(const char *bus)
{
    if (!bus)
        return CAN_BUS_ANY;

    uint8_t mask = CAN_BUS_ANY;
    const char *token = nullptr;
    uint8_t len = 0;
    for (const char *p = bus;; p++)
    {
        char c = *p;
        bool separator = c == '\0' || c == ',' || c == '|' || c == '+' || c == ' ';
        if (!separator)
        {
            if (!token)
                token = p;
            if (len < 16)
                len++;
            continue;
        }
        if (token)
        {
            mask |= pluginBusMaskForToken(token, len);
            token = nullptr;
            len = 0;
        }
        if (c == '\0')
            break;
    }
    return mask;
}

static uint8_t pluginParseBus(JsonVariant value)
{
    if (value.isNull())
        return CAN_BUS_ANY;
    if (value.is<uint8_t>())
        return value.as<uint8_t>() & (CAN_BUS_CH | CAN_BUS_VEH | CAN_BUS_PARTY);
    if (value.is<const char *>())
        return pluginParseBusString(value.as<const char *>());
    if (value.is<JsonArray>())
    {
        uint8_t mask = CAN_BUS_ANY;
        for (JsonVariant item : value.as<JsonArray>())
            mask |= pluginParseBus(item);
        return mask;
    }
    return CAN_BUS_ANY;
}

static bool pluginRuleMatchesBus(const PluginRule &rule, const CanFrame &frame)
{
    if (rule.busMask == CAN_BUS_ANY || frame.bus == CAN_BUS_ANY)
        return true;
    return (rule.busMask & frame.bus) != 0;
}

static bool pluginRuleMatchesMux(const PluginRule &rule, const CanFrame &frame)
{
    if (rule.mux < 0)
        return true;
    if (frame.dlc == 0)
        return false;
    uint8_t mask = rule.muxMask ? rule.muxMask : pluginDefaultMuxMask(rule.mux);
    return (frame.data[0] & mask) == (static_cast<uint8_t>(rule.mux) & mask);
}

static bool pluginRuleMuxIncludes(const PluginRule &rule, uint8_t mux)
{
    if (rule.mux < 0)
        return false;
    uint8_t mask = rule.muxMask ? rule.muxMask : pluginDefaultMuxMask(rule.mux);
    return (static_cast<uint8_t>(rule.mux) & mask) == (mux & mask);
}

static bool pluginParseJson(const String &json, PluginData &out)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err)
        return false;

    strlcpy(out.name, doc["name"] | "Unknown", sizeof(out.name));
    strlcpy(out.version, doc["version"] | "1.0", sizeof(out.version));
    strlcpy(out.author, doc["author"] | "", sizeof(out.author));
    out.filename[0] = '\0';
    out.sourceUrl[0] = '\0';
    out.enabled = true;
    out.priority = 0;
    out.ruleCount = 0;
    out.filterIdCount = 0;

    JsonArray rules = doc["rules"];
    if (!rules)
        return false;

    for (JsonObject rule : rules)
    {
        if (out.ruleCount >= PLUGIN_RULES_MAX)
            break;
        PluginRule &r = out.rules[out.ruleCount];
        r.canId = rule["id"] | (uint32_t)0;
        int mux = rule["mux"] | (int)-1;
        if (mux < -1)
            mux = -1;
        if (mux > 255)
            mux = 255;
        r.mux = static_cast<int16_t>(mux);
        JsonVariant muxMaskValue = rule["mux_mask"];
        if (muxMaskValue.isNull())
            muxMaskValue = rule["muxMask"];
        r.muxMask = muxMaskValue | pluginDefaultMuxMask(r.mux);
        if (r.mux >= 0 && r.muxMask == 0)
            r.muxMask = pluginDefaultMuxMask(r.mux);
        r.busMask = pluginParseBus(rule["bus"]);
        r.sendAfter = rule["send"] | true;
        r.opCount = 0;

        JsonArray ops = rule["ops"];
        if (ops)
        {
            for (JsonObject op : ops)
            {
                if (r.opCount >= PLUGIN_OPS_MAX)
                    break;
                PluginOp &o = r.ops[r.opCount];
                o.index = 0;
                o.value = 0;
                o.mask = 0xFF;
                o.intervalMs = 0;
                o.gtwSilent = false;
                const char *type = op["type"] | "";
                if (strcmp(type, "set_bit") == 0)
                {
                    o.type = OP_SET_BIT;
                    o.index = op["bit"] | (uint8_t)0;
                    o.value = op["val"] | (uint8_t)1;
                }
                else if (strcmp(type, "set_byte") == 0)
                {
                    o.type = OP_SET_BYTE;
                    o.index = op["byte"] | (uint8_t)0;
                    o.mask = op["mask"] | (uint8_t)0xFF;
                    o.value = op["val"] | (uint8_t)0;
                }
                else if (strcmp(type, "or_byte") == 0)
                {
                    o.type = OP_OR_BYTE;
                    o.index = op["byte"] | (uint8_t)0;
                    o.value = op["val"] | (uint8_t)0;
                }
                else if (strcmp(type, "and_byte") == 0)
                {
                    o.type = OP_AND_BYTE;
                    o.index = op["byte"] | (uint8_t)0;
                    o.value = op["val"] | (uint8_t)0xFF;
                }
                else if (strcmp(type, "checksum") == 0)
                {
                    o.type = OP_CHECKSUM;
                }
                else if (strcmp(type, "counter") == 0)
                {
                    o.type = OP_COUNTER;
                    o.index = op["byte"] | (uint8_t)0;
                    o.mask = op["mask"] | (uint8_t)0x0F;
                    o.value = op["step"] | (uint8_t)1;
                    if (o.value == 0)
                        o.value = 1;
                }
                else if (strcmp(type, "emit_periodic") == 0)
                {
                    o.type = OP_EMIT_PERIODIC;
                    o.intervalMs =
                        pluginClampPeriodicInterval(op["interval"] | PLUGIN_PERIODIC_INTERVAL_DEFAULT_MS);
                    bool requestedSilent = op["gtw_silent"] | false;
                    if (!requestedSilent)
                        requestedSilent = op["silent"] | false;
                    o.gtwSilent = requestedSilent && pluginGtwSilentSupported();

                    // When gtw_silent is requested, ensure the hardware CAN filter
                    // accepts UDS traffic so the state machine can see responses.
                    if (o.gtwSilent)
                    {
                        const uint32_t udsIds[] = {PLUGIN_GTW_UDS_REQUEST_ID,
                                                   PLUGIN_GTW_UDS_RESPONSE_ID};
                        for (uint8_t k = 0; k < 2; k++)
                        {
                            bool dup = false;
                            for (uint8_t j = 0; j < out.filterIdCount; j++)
                            {
                                if (out.filterIds[j] == udsIds[k])
                                {
                                    dup = true;
                                    break;
                                }
                            }
                            if (!dup && out.filterIdCount < PLUGIN_FILTER_IDS_MAX)
                                out.filterIds[out.filterIdCount++] = udsIds[k];
                        }
                    }
                }
                else
                {
                    continue;
                }
                r.opCount++;
            }
        }

        // Deduplicate filter IDs
        bool found = false;
        for (uint8_t i = 0; i < out.filterIdCount; i++)
        {
            if (out.filterIds[i] == r.canId)
            {
                found = true;
                break;
            }
        }
        if (!found && out.filterIdCount < PLUGIN_FILTER_IDS_MAX)
            out.filterIds[out.filterIdCount++] = r.canId;

        out.ruleCount++;
    }

    return out.ruleCount > 0;
}

// ── SPIFFS STORAGE ──────────────────────────────────────────────

#if !defined(NATIVE_BUILD)
static String pluginFilePath(const char *filename)
{
    return String("/p_") + filename;
}

static bool pluginSaveToSpiffs(const String &json, const char *filename)
{
    File f = SPIFFS.open(pluginFilePath(filename), "w");
    if (!f)
        return false;
    f.print(json);
    f.close();
    return true;
}

static void pluginLoadAll()
{
    pluginResetPeriodicEmit();
    pluginsLocked = true;
    pluginCount = 0;

    File root = SPIFFS.open("/");
    if (!root)
    {
        pluginsLocked = false;
        return;
    }

    File f = root.openNextFile();
    while (f && pluginCount < PLUGIN_MAX)
    {
        String name = f.name();
        // Normalize: some SPIFFS versions include leading /
        if (name.startsWith("/"))
            name = name.substring(1);

        if (name.startsWith("p_") && name.endsWith(".json"))
        {
            String json = f.readString();
            PluginData &p = pluginStore[pluginCount];
            if (pluginParseJson(json, p))
            {
                // Store just the user-facing filename (without /p_ prefix)
                String userFilename = name.substring(2); // remove "p_"
                strlcpy(p.filename, userFilename.c_str(), sizeof(p.filename));
                p.priority = pluginCount;
                pluginCount++;
            }
        }
        f = root.openNextFile();
    }

    pluginsLocked = false;
}

static bool pluginRemove(uint8_t index)
{
    if (index >= pluginCount)
        return false;
    pluginsLocked = true;

    SPIFFS.remove(pluginFilePath(pluginStore[index].filename));

    for (uint8_t i = index; i < pluginCount - 1; i++)
    {
        pluginStore[i] = pluginStore[i + 1];
        pluginStore[i].priority = i;
    }
    pluginCount--;

    pluginsLocked = false;
    return true;
}
#endif

static void pluginNormalizePriorities()
{
    for (uint8_t i = 0; i < pluginCount; i++)
        pluginStore[i].priority = i;
}

static void pluginSortByPriority()
{
    pluginsLocked = true;
    for (uint8_t i = 1; i < pluginCount; i++)
    {
        PluginData current = pluginStore[i];
        uint8_t j = i;
        while (j > 0 && pluginStore[j - 1].priority > current.priority)
        {
            pluginStore[j] = pluginStore[j - 1];
            j--;
        }
        pluginStore[j] = current;
    }
    pluginNormalizePriorities();
    pluginsLocked = false;
}

static bool pluginInsert(uint8_t index, const PluginData &plugin)
{
    if (pluginCount >= PLUGIN_MAX)
        return false;
    if (index > pluginCount)
        index = pluginCount;

    pluginsLocked = true;
    for (uint8_t i = pluginCount; i > index; i--)
        pluginStore[i] = pluginStore[i - 1];
    pluginStore[index] = plugin;
    pluginCount++;
    pluginNormalizePriorities();
    pluginsLocked = false;
    return true;
}

static bool pluginMove(uint8_t from, uint8_t to)
{
    if (from >= pluginCount || to >= pluginCount || from == to)
        return from < pluginCount && to < pluginCount;

    pluginsLocked = true;
    PluginData moving = pluginStore[from];
    if (from < to)
    {
        for (uint8_t i = from; i < to; i++)
            pluginStore[i] = pluginStore[i + 1];
    }
    else
    {
        for (uint8_t i = from; i > to; i--)
            pluginStore[i] = pluginStore[i - 1];
    }
    pluginStore[to] = moving;
    pluginNormalizePriorities();
    pluginsLocked = false;
    return true;
}

static int pluginFindByName(const char *name)
{
    for (uint8_t i = 0; i < pluginCount; i++)
    {
        if (strcmp(pluginStore[i].name, name) == 0)
            return i;
    }
    return -1;
}

// ── RULE EXECUTION ──────────────────────────────────────────────

static uint8_t pluginCounterShift(uint8_t mask)
{
    uint8_t shift = 0;
    while (shift < 8 && (mask & (1U << shift)) == 0)
        shift++;
    return shift;
}

static void pluginApplyCounter(CanFrame &frame, const PluginOp &op)
{
    if (op.index >= 8 || op.mask == 0)
        return;

    uint8_t shift = pluginCounterShift(op.mask);
    uint8_t fieldMask = op.mask >> shift;
    if (fieldMask == 0)
        return;

    uint8_t current = (frame.data[op.index] >> shift) & fieldMask;
    uint8_t next = (current + op.value) & fieldMask;
    frame.data[op.index] = (frame.data[op.index] & ~op.mask) | ((next << shift) & op.mask);
}

static void pluginApplyOp(CanFrame &frame, const PluginOp &op)
{
    switch (op.type)
    {
    case OP_SET_BIT:
        setBit(frame, op.index, op.value);
        break;
    case OP_SET_BYTE:
        if (op.index < 8)
            frame.data[op.index] = (frame.data[op.index] & ~op.mask) | (op.value & op.mask);
        break;
    case OP_OR_BYTE:
        if (op.index < 8)
            frame.data[op.index] |= op.value;
        break;
    case OP_AND_BYTE:
        if (op.index < 8)
            frame.data[op.index] &= op.value;
        break;
    case OP_CHECKSUM:
        frame.data[7] = computeVehicleChecksum(frame);
        break;
    case OP_COUNTER:
        pluginApplyCounter(frame, op);
        break;
    case OP_EMIT_PERIODIC:
        break;
    }
}

static uint64_t pluginOpWriteMask(const PluginOp &op)
{
    switch (op.type)
    {
    case OP_SET_BIT:
        if (op.index < 64)
            return 1ULL << op.index;
        return 0;
    case OP_SET_BYTE:
        if (op.index < 8)
            return static_cast<uint64_t>(op.mask) << (op.index * 8);
        return 0;
    case OP_OR_BYTE:
        if (op.index < 8)
            return static_cast<uint64_t>(op.value) << (op.index * 8);
        return 0;
    case OP_AND_BYTE:
        if (op.index < 8)
            return static_cast<uint64_t>(static_cast<uint8_t>(~op.value)) << (op.index * 8);
        return 0;
    case OP_CHECKSUM:
        return 0xFFULL << 56;
    case OP_COUNTER:
        if (op.index < 8)
            return static_cast<uint64_t>(op.mask) << (op.index * 8);
        return 0;
    case OP_EMIT_PERIODIC:
        return 0;
    }
    return 0;
}

static bool pluginApplyOpMasked(CanFrame &frame, const PluginOp &op, uint64_t allowedMask)
{
    switch (op.type)
    {
    case OP_SET_BIT:
        if ((allowedMask & pluginOpWriteMask(op)) != 0)
        {
            setBit(frame, op.index, op.value);
            return true;
        }
        return false;
    case OP_SET_BYTE:
        if (op.index < 8)
        {
            uint8_t allowed = static_cast<uint8_t>((allowedMask >> (op.index * 8)) & 0xFF);
            if (allowed == 0)
                return false;
            frame.data[op.index] = (frame.data[op.index] & ~allowed) | (op.value & allowed);
            return true;
        }
        return false;
    case OP_OR_BYTE:
        if (op.index < 8)
        {
            uint8_t allowed = static_cast<uint8_t>((allowedMask >> (op.index * 8)) & 0xFF);
            if (allowed == 0)
                return false;
            frame.data[op.index] |= allowed;
            return true;
        }
        return false;
    case OP_AND_BYTE:
        if (op.index < 8)
        {
            uint8_t allowed = static_cast<uint8_t>((allowedMask >> (op.index * 8)) & 0xFF);
            if (allowed == 0)
                return false;
            frame.data[op.index] &= static_cast<uint8_t>(~allowed);
            return true;
        }
        return false;
    case OP_CHECKSUM:
        return allowedMask == pluginOpWriteMask(op);
    case OP_COUNTER:
        if (allowedMask == pluginOpWriteMask(op))
        {
            pluginApplyCounter(frame, op);
            return true;
        }
        return false;
    case OP_EMIT_PERIODIC:
        return false;
    }
    return false;
}

static void pluginAdvanceRuleCounters(CanFrame &frame, const PluginRule &rule)
{
    bool checksum = false;
    for (uint8_t o = 0; o < rule.opCount; o++)
    {
        const PluginOp &op = rule.ops[o];
        if (op.type == OP_COUNTER)
            pluginApplyCounter(frame, op);
        else if (op.type == OP_CHECKSUM)
            checksum = true;
    }
    if (checksum)
        frame.data[7] = computeVehicleChecksum(frame);
}

static void pluginAdvanceCounters(CanFrame &frame, const PluginOp *counterOps,
                                  uint8_t counterOpCount, bool checksum)
{
    for (uint8_t i = 0; i < counterOpCount; i++)
        pluginApplyCounter(frame, counterOps[i]);
    if (checksum)
        frame.data[7] = computeVehicleChecksum(frame);
}

static bool pluginFrameChanged(const CanFrame &a, const CanFrame &b)
{
    if (a.id != b.id || a.dlc != b.dlc)
        return true;

    uint8_t dlc = (a.dlc <= 8) ? a.dlc : 8;
    for (uint8_t i = 0; i < dlc; i++)
    {
        if (a.data[i] != b.data[i])
            return true;
    }
    return false;
}

static bool pluginHasEnabledPeriodicEmit()
{
    for (uint8_t p = 0; p < pluginCount; p++)
    {
        if (!pluginStore[p].enabled)
            continue;
        for (uint8_t r = 0; r < pluginStore[p].ruleCount; r++)
        {
            const PluginRule &rule = pluginStore[p].rules[r];
            if (rule.canId != 2047 || !pluginRuleMuxIncludes(rule, 3) || !rule.sendAfter)
                continue;
            for (uint8_t o = 0; o < rule.opCount; o++)
            {
                if (rule.ops[o].type == OP_EMIT_PERIODIC)
                    return true;
            }
        }
    }
    return false;
}

static void pluginCachePeriodicEmit(const CanFrame &frame, uint16_t intervalMs, bool gtwSilent,
                                    const PluginOp *counterOps, uint8_t counterOpCount,
                                    bool checksum, unsigned long now)
{
    bool wasActive = pluginPeriodicEmit.active;
    bool wasGtwSilent = pluginPeriodicEmit.gtwSilent;
    PluginGtwUdsMachine preservedUds = pluginPeriodicEmit.uds;

    pluginPeriodicEmit.active = true;
    pluginPeriodicEmit.gtwSilent = gtwSilent;
    pluginPeriodicEmit.checksum = checksum;
    pluginPeriodicEmit.frame = frame;
    pluginPeriodicEmit.counterOpCount = counterOpCount;
    pluginPeriodicEmit.intervalMs = pluginClampPeriodicInterval(intervalMs);
    pluginPeriodicEmit.nextFrameAt = now + pluginPeriodicEmit.intervalMs;

    // Preserve UDS state across repeated caching so we don't restart the
    // handshake every time the periodic frame is refreshed.
    if (wasActive && wasGtwSilent && gtwSilent)
    {
        pluginPeriodicEmit.uds = preservedUds;
        pluginPeriodicEmit.uds.bus = frame.bus;
    }
    else
    {
        pluginPeriodicEmit.uds = {};
        pluginPeriodicEmit.uds.bus = frame.bus;
        if (gtwSilent)
        {
            pluginPeriodicEmit.uds.retryAfterMs = PLUGIN_GTW_UDS_RETRY_BACKOFF_MS;
            pluginPeriodicEmit.uds.nextActionAt = now;
            pluginPeriodicEmit.uds.stateEnteredAt = now;
        }
    }

    for (uint8_t i = 0; i < counterOpCount && i < PLUGIN_OPS_MAX; i++)
        pluginPeriodicEmit.counterOps[i] = counterOps[i];
}

static void pluginEmitPeriodicTick(CanDriver &driver, unsigned long now)
{
    if (pluginsLocked || !pluginPeriodicEmit.active)
        return;
    if (!pluginHasEnabledPeriodicEmit())
    {
        pluginResetPeriodicEmit();
        return;
    }

    if (pluginPeriodicEmit.gtwSilent)
        pluginGtwUdsTick(pluginPeriodicEmit.uds, driver, now);

    if ((long)(now - pluginPeriodicEmit.nextFrameAt) < 0)
        return;

    driver.send(pluginPeriodicEmit.frame);
    pluginAdvanceCounters(pluginPeriodicEmit.frame, pluginPeriodicEmit.counterOps,
                          pluginPeriodicEmit.counterOpCount, pluginPeriodicEmit.checksum);
    pluginPeriodicEmit.nextFrameAt = now + pluginPeriodicEmit.intervalMs;
}

static bool pluginProcessFrame(const CanFrame &original, CanDriver &driver)
{
    if (pluginsLocked || pluginCount == 0)
        return false;

    // Intercept GTW UDS responses so the silencing state machine can
    // progress. These frames are not subject to rule-based rewriting.
    if (original.id == PLUGIN_GTW_UDS_RESPONSE_ID && pluginPeriodicEmit.active &&
        pluginPeriodicEmit.gtwSilent)
    {
        if (pluginPeriodicEmit.uds.bus != CAN_BUS_ANY && original.bus != CAN_BUS_ANY &&
            (pluginPeriodicEmit.uds.bus & original.bus) == 0)
            return false;
        pluginGtwUdsHandleResponse(pluginPeriodicEmit.uds, original, millis());
        return true;
    }

    bool processed = false;
    bool sendRequested = false;
    bool checksumPending = false;
    bool emitPeriodicRequested = false;
    bool emitPeriodicGtwSilent = false;
    uint16_t emitPeriodicIntervalMs = PLUGIN_PERIODIC_INTERVAL_MAX_MS;
    PluginOp counterOps[PLUGIN_OPS_MAX];
    uint8_t counterOpCount = 0;
    uint64_t claimed = 0;
    CanFrame modified = original;

    for (uint8_t p = 0; p < pluginCount; p++)
    {
        if (!pluginStore[p].enabled)
            continue;
        uint64_t pluginTouched = 0;
        for (uint8_t r = 0; r < pluginStore[p].ruleCount; r++)
        {
            const PluginRule &rule = pluginStore[p].rules[r];
            if (rule.canId != original.id)
                continue;

            if (!pluginRuleMatchesBus(rule, original) || !pluginRuleMatchesMux(rule, original))
                continue;

            processed = true;
            if (!rule.sendAfter)
                continue;

            sendRequested = true;
            for (uint8_t o = 0; o < rule.opCount; o++)
            {
                const PluginOp &op = rule.ops[o];
                if (op.type == OP_EMIT_PERIODIC)
                {
                    emitPeriodicRequested = true;
                    emitPeriodicGtwSilent |= op.gtwSilent;
                    if (op.intervalMs < emitPeriodicIntervalMs)
                        emitPeriodicIntervalMs = op.intervalMs;
                    continue;
                }
                uint64_t opMask = pluginOpWriteMask(op);
                uint64_t allowedMask = opMask & ~claimed;
                if (op.type == OP_CHECKSUM)
                {
                    if (allowedMask == opMask && pluginApplyOpMasked(modified, op, allowedMask))
                    {
                        pluginTouched |= opMask;
                        checksumPending = true;
                    }
                    continue;
                }
                if (op.type == OP_COUNTER)
                {
                    if (opMask != 0 && allowedMask == opMask && pluginApplyOpMasked(modified, op, allowedMask))
                    {
                        pluginTouched |= opMask;
                        if (counterOpCount < PLUGIN_OPS_MAX)
                            counterOps[counterOpCount++] = op;
                    }
                    continue;
                }

                if (allowedMask != 0 && pluginApplyOpMasked(modified, op, allowedMask))
                    pluginTouched |= allowedMask;
            }
        }
        claimed |= pluginTouched;
    }

    if (sendRequested)
    {
        if (checksumPending)
            modified.data[7] = computeVehicleChecksum(modified);
        if (pluginBeforeSend && pluginBeforeSend(modified, original) && checksumPending)
            modified.data[7] = computeVehicleChecksum(modified);
        bool cachePeriodic = emitPeriodicRequested && original.id == 2047 && (original.data[0] & 0x07) == 3;
        CanFrame periodicFrame = modified;
        if (pluginFrameChanged(original, modified))
        {
            uint8_t replayCount = original.id == 2047 ? pluginGetReplayCount() : 1;
            CanFrame replayFrame = modified;
            for (uint8_t i = 0; i < replayCount; i++)
            {
                driver.send(replayFrame);
                if (i + 1 >= replayCount)
                    continue;
                pluginAdvanceCounters(replayFrame, counterOps, counterOpCount, checksumPending);
            }
            periodicFrame = replayFrame;
            pluginAdvanceCounters(periodicFrame, counterOps, counterOpCount, checksumPending);
        }
        if (cachePeriodic)
            pluginCachePeriodicEmit(periodicFrame, emitPeriodicIntervalMs, emitPeriodicGtwSilent,
                                    counterOps, counterOpCount, checksumPending, millis());
    }

    return processed;
}

// ── FILTER MERGING ──────────────────────────────────────────────

static uint8_t pluginGetFilterIds(uint32_t *ids, uint8_t maxIds)
{
    uint8_t count = 0;
    for (uint8_t p = 0; p < pluginCount; p++)
    {
        if (!pluginStore[p].enabled)
            continue;
        for (uint8_t i = 0; i < pluginStore[p].filterIdCount; i++)
        {
            if (count >= maxIds)
                return count;
            bool dup = false;
            for (uint8_t j = 0; j < count; j++)
            {
                if (ids[j] == pluginStore[p].filterIds[i])
                {
                    dup = true;
                    break;
                }
            }
            if (!dup)
                ids[count++] = pluginStore[p].filterIds[i];
        }
    }
    return count;
}

#endif
