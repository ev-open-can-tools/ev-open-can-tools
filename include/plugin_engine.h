#pragma once

#if defined(ESP32_DASHBOARD) && (!defined(NATIVE_BUILD) || defined(PLUGIN_ENGINE_NATIVE_TEST))

#include <ArduinoJson.h>
#if defined(NATIVE_BUILD)
#include <string>
using String = std::string;
#elif defined(ESP_PLATFORM)
#include "platform/espidf_runtime.h"
#include <freertos/semphr.h>
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
#define PLUGIN_GTW_UDS_SEED_MAX 5            // ISO-TP single frame: SID + subfunction + up to 5 seed/key bytes

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

struct PluginRuleDiagnostics
{
    uint32_t matchCount;
    uint32_t changedCount;
    uint32_t sendOkCount;
    uint32_t sendFailCount;
    unsigned long lastMatchMs;
    unsigned long lastSendMs;
    uint8_t lastOriginal[8];
    uint8_t lastModified[8];
};

struct PluginRule
{
    uint32_t canId;
    int16_t mux; // -1 = match any mux
    uint8_t muxMask;
    uint8_t matchByte;
    uint8_t matchMask; // 0 = no byte match
    uint8_t matchValue;
    uint8_t busMask;
    PluginOp ops[PLUGIN_OPS_MAX];
    uint8_t opCount;
    bool sendAfter;
    PluginRuleDiagnostics diag;
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
static void (*pluginDiagnosticsLog)(const char *message) = nullptr;

class PluginLockGuard
{
public:
    explicit PluginLockGuard(bool wait = true)
    {
#ifdef ESP_PLATFORM
        initialize();
        if (pluginMutex_)
            locked_ = xSemaphoreTakeRecursive(pluginMutex_, wait ? portMAX_DELAY : 0) == pdTRUE;
#else
        (void)wait;
        locked_ = true;
#endif
    }

    ~PluginLockGuard()
    {
#ifdef ESP_PLATFORM
        if (locked_)
            xSemaphoreGiveRecursive(pluginMutex_);
#endif
    }

    explicit operator bool() const { return locked_; }

    static bool initialize()
    {
#ifdef ESP_PLATFORM
        if (!pluginMutex_)
            pluginMutex_ = xSemaphoreCreateRecursiveMutex();
        return pluginMutex_ != nullptr;
#else
        return true;
#endif
    }

private:
#ifdef ESP_PLATFORM
    inline static SemaphoreHandle_t pluginMutex_ = nullptr;
#endif
    bool locked_ = false;
};

static uint8_t pluginClampReplayCount(int32_t count)
{
    if (count < 1)
        return 1;
    if (count > PLUGIN_REPLAY_COUNT_MAX)
        return PLUGIN_REPLAY_COUNT_MAX;
    return static_cast<uint8_t>(count);
}

static uint8_t pluginReplayCount = pluginClampReplayCount(PLUGIN_REPLAY_COUNT);

static void pluginResetDiagnostics();

static void pluginSetReplayCount(int32_t count)
{
    PluginLockGuard guard;
    pluginReplayCount = pluginClampReplayCount(count);
}

static void pluginSetDiagnosticsLogger(void (*logger)(const char *message))
{
    pluginDiagnosticsLog = logger;
}

static void pluginLogDiagnosticsEvent(const char *message)
{
    if (pluginDiagnosticsLog)
        pluginDiagnosticsLog(message);
}

static uint8_t pluginGetReplayCount()
{
    PluginLockGuard guard;
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
    PluginLockGuard guard;
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
    if (len < 1 || len > 7 || frame.dlc < static_cast<uint8_t>(len + 1))
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
        if (!driver.send(pluginMakeUdsRequest(payload, sizeof(payload), m.bus)))
        {
            pluginGtwUdsFail(m, 0xFD, now);
            break;
        }
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
            if (!driver.send(pluginMakeUdsRequest(payload, sizeof(payload), m.bus)))
            {
                pluginGtwUdsFail(m, 0xFD, now);
                break;
            }
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
            if (!driver.send(pluginMakeUdsRequest(payload, 2 + keyLen, m.bus)))
            {
                pluginGtwUdsFail(m, 0xFD, now);
                break;
            }
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
            if (!driver.send(pluginMakeUdsRequest(payload, sizeof(payload), m.bus)))
            {
                pluginGtwUdsFail(m, 0xFD, now);
                break;
            }
            m.nextActionAt = now + PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS;
        }
        else if ((long)(now - m.stateEnteredAt) >= PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS)
            pluginGtwUdsFail(m, 0xFF, now);
        break;

    case GTW_UDS_ACTIVE:
    {
        const uint8_t payload[] = {0x3E, 0x00}; // TesterPresent
        if (!driver.send(pluginMakeUdsRequest(payload, sizeof(payload), m.bus)))
        {
            pluginGtwUdsFail(m, 0xFD, now);
            break;
        }
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
    size_t expectedLen = strlen(expected);
    if (len != expectedLen)
        return false;
    for (uint8_t i = 0; i < len; i++)
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

static bool pluginBusMaskForToken(const char *token, uint8_t len, uint8_t &mask)
{
    if (len == 0 || pluginBusTokenEquals(token, len, "ANY"))
        mask = CAN_BUS_ANY;
    if (pluginBusTokenEquals(token, len, "CH"))
        mask = CAN_BUS_CH;
    else if (pluginBusTokenEquals(token, len, "VEH"))
        mask = CAN_BUS_VEH;
    else if (pluginBusTokenEquals(token, len, "PARTY"))
        mask = CAN_BUS_PARTY;
    else if (len != 0 && !pluginBusTokenEquals(token, len, "ANY"))
        return false;
    return true;
}

static bool pluginParseBusString(const char *bus, uint8_t &mask)
{
    if (!bus)
    {
        mask = CAN_BUS_ANY;
        return true;
    }

    mask = CAN_BUS_ANY;
    bool sawToken = false;
    bool sawAny = false;
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
            uint8_t tokenMask = CAN_BUS_ANY;
            if (!pluginBusMaskForToken(token, len, tokenMask))
                return false;
            sawToken = true;
            sawAny |= tokenMask == CAN_BUS_ANY;
            if (!sawAny)
                mask |= tokenMask;
            token = nullptr;
            len = 0;
        }
        if (c == '\0')
            break;
    }
    if (!sawToken)
        return false;
    if (sawAny)
        mask = CAN_BUS_ANY;
    return true;
}

static bool pluginParseBus(JsonVariant value, uint8_t &mask)
{
    if (value.isNull())
    {
        mask = CAN_BUS_ANY;
        return true;
    }
    if (value.is<uint8_t>())
    {
        uint8_t raw = value.as<uint8_t>();
        mask = raw & (CAN_BUS_CH | CAN_BUS_VEH | CAN_BUS_PARTY);
        return raw == mask;
    }
    if (value.is<const char *>())
        return pluginParseBusString(value.as<const char *>(), mask);
    if (value.is<JsonArray>())
    {
        JsonArray values = value.as<JsonArray>();
        if (values.size() == 0)
            return false;
        mask = CAN_BUS_ANY;
        bool sawAny = false;
        for (JsonVariant item : values)
        {
            if (item.isNull())
                return false;
            uint8_t itemMask = CAN_BUS_ANY;
            if (!pluginParseBus(item, itemMask))
                return false;
            sawAny |= itemMask == CAN_BUS_ANY;
            if (!sawAny)
                mask |= itemMask;
        }
        if (sawAny)
            mask = CAN_BUS_ANY;
        return true;
    }
    return false;
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

static bool pluginRuleMatchesByte(const PluginRule &rule, const CanFrame &frame)
{
    if (rule.matchMask == 0)
        return true;
    if (rule.matchByte >= frame.dlc || rule.matchByte >= 8)
        return false;
    return (frame.data[rule.matchByte] & rule.matchMask) == (rule.matchValue & rule.matchMask);
}

static bool pluginRuleMuxIncludes(const PluginRule &rule, uint8_t mux)
{
    if (rule.mux < 0)
        return false;
    uint8_t mask = rule.muxMask ? rule.muxMask : pluginDefaultMuxMask(rule.mux);
    return (static_cast<uint8_t>(rule.mux) & mask) == (mux & mask);
}

static bool pluginReadByte(JsonVariant value, uint8_t defaultValue, uint8_t &out)
{
    if (value.isNull())
    {
        out = defaultValue;
        return true;
    }
    if (!value.is<int>())
        return false;
    int parsed = value.as<int>();
    if (parsed < 0 || parsed > 255)
        return false;
    out = static_cast<uint8_t>(parsed);
    return true;
}

static bool pluginReadBitValue(JsonVariant value, uint8_t defaultValue, uint8_t &out)
{
    if (value.isNull())
    {
        out = defaultValue;
        return true;
    }
    if (value.is<bool>())
    {
        out = value.as<bool>() ? 1 : 0;
        return true;
    }
    if (!value.is<int>())
        return false;
    int parsed = value.as<int>();
    if (parsed < 0 || parsed > 1)
        return false;
    out = static_cast<uint8_t>(parsed);
    return true;
}

static bool pluginReadBool(JsonVariant value, bool defaultValue, bool &out)
{
    if (value.isNull())
    {
        out = defaultValue;
        return true;
    }
    if (!value.is<bool>())
        return false;
    out = value.as<bool>();
    return true;
}

static bool pluginParseJson(const String &json, PluginData &out)
{
    if (json.length() > 32 * 1024)
        return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err)
        return false;

    const char *name = doc["name"] | "Unknown";
    const char *version = doc["version"] | "1.0";
    const char *author = doc["author"] | "";
    if (*name == '\0' || *version == '\0' || strlen(name) >= sizeof(out.name) ||
        strlen(version) >= sizeof(out.version) ||
        strlen(author) >= sizeof(out.author))
        return false;
    for (const char *text : {name, version, author})
    {
        for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; p++)
        {
            if (*p < 0x20 || *p == 0x7F)
                return false;
        }
    }
    strlcpy(out.name, name, sizeof(out.name));
    strlcpy(out.version, version, sizeof(out.version));
    strlcpy(out.author, author, sizeof(out.author));
    out.filename[0] = '\0';
    out.sourceUrl[0] = '\0';
    out.enabled = true;
    out.priority = 0;
    out.ruleCount = 0;
    out.filterIdCount = 0;

    JsonArray rules = doc["rules"];
    if (!rules || rules.size() == 0 || rules.size() > PLUGIN_RULES_MAX)
        return false;

    for (JsonObject rule : rules)
    {
        if (out.ruleCount >= PLUGIN_RULES_MAX)
            break;
        PluginRule &r = out.rules[out.ruleCount];
        r = {};
        JsonVariant canIdValue = rule["id"];
        if (!canIdValue.is<uint32_t>())
            return false;
        r.canId = canIdValue.as<uint32_t>();
        if (r.canId > 0x7FF)
            return false;
        JsonVariant muxValue = rule["mux"];
        if (!muxValue.isNull() && !muxValue.is<int>())
            return false;
        int mux = muxValue.isNull() ? -1 : muxValue.as<int>();
        if (mux < -1 || mux > 255)
            return false;
        r.mux = static_cast<int16_t>(mux);
        JsonVariant muxMaskValue = rule["mux_mask"];
        if (muxMaskValue.isNull())
            muxMaskValue = rule["muxMask"];
        if (!pluginReadByte(muxMaskValue, pluginDefaultMuxMask(r.mux), r.muxMask))
            return false;
        if (r.mux >= 0 && r.muxMask == 0)
            r.muxMask = pluginDefaultMuxMask(r.mux);
        if (!pluginParseBus(rule["bus"], r.busMask))
            return false;
        r.matchByte = 0;
        r.matchMask = 0;
        r.matchValue = 0;
        JsonVariant match = rule["match"];
        if (match.is<JsonObject>())
        {
            if (!pluginReadByte(match["byte"], 0, r.matchByte) ||
                !pluginReadByte(match["mask"], 0xFF, r.matchMask))
                return false;
            JsonVariant matchValue = match["val"];
            if (matchValue.isNull())
                matchValue = match["value"];
            if (!pluginReadByte(matchValue, 0, r.matchValue))
                return false;
        }
        JsonVariant matchByte = rule["match_byte"];
        if (matchByte.isNull())
            matchByte = rule["matchByte"];
        JsonVariant matchMask = rule["match_mask"];
        if (matchMask.isNull())
            matchMask = rule["matchMask"];
        JsonVariant matchValue = rule["match_val"];
        if (matchValue.isNull())
            matchValue = rule["match_value"];
        if (matchValue.isNull())
            matchValue = rule["matchValue"];
        if (!pluginReadByte(matchByte, r.matchByte, r.matchByte) ||
            !pluginReadByte(matchMask, r.matchMask, r.matchMask) ||
            !pluginReadByte(matchValue, r.matchValue, r.matchValue))
            return false;
        if (r.matchByte > 7 && r.matchMask != 0)
            return false;
        if (!pluginReadBool(rule["send"], true, r.sendAfter))
            return false;
        r.opCount = 0;

        JsonArray ops = rule["ops"];
        if (!ops || ops.size() == 0 || ops.size() > PLUGIN_OPS_MAX)
            return false;
        bool hasPeriodicEmit = false;
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
                    if (!pluginReadByte(op["bit"], 0, o.index) ||
                        !pluginReadBitValue(op["val"], 1, o.value))
                        return false;
                }
                else if (strcmp(type, "set_byte") == 0)
                {
                    o.type = OP_SET_BYTE;
                    if (!pluginReadByte(op["byte"], 0, o.index) ||
                        !pluginReadByte(op["mask"], 0xFF, o.mask) ||
                        !pluginReadByte(op["val"], 0, o.value))
                        return false;
                }
                else if (strcmp(type, "or_byte") == 0)
                {
                    o.type = OP_OR_BYTE;
                    if (!pluginReadByte(op["byte"], 0, o.index) ||
                        !pluginReadByte(op["val"], 0, o.value))
                        return false;
                }
                else if (strcmp(type, "and_byte") == 0)
                {
                    o.type = OP_AND_BYTE;
                    if (!pluginReadByte(op["byte"], 0, o.index) ||
                        !pluginReadByte(op["val"], 0xFF, o.value))
                        return false;
                }
                else if (strcmp(type, "checksum") == 0)
                {
                    o.type = OP_CHECKSUM;
                }
                else if (strcmp(type, "counter") == 0)
                {
                    o.type = OP_COUNTER;
                    if (!pluginReadByte(op["byte"], 0, o.index) ||
                        !pluginReadByte(op["mask"], 0x0F, o.mask) ||
                        !pluginReadByte(op["step"], 1, o.value) || o.value == 0)
                        return false;
                }
                else if (strcmp(type, "emit_periodic") == 0)
                {
                    o.type = OP_EMIT_PERIODIC;
                    JsonVariant intervalValue = op["interval"];
                    if (!intervalValue.isNull() && !intervalValue.is<int>())
                        return false;
                    int interval = intervalValue.isNull() ? PLUGIN_PERIODIC_INTERVAL_DEFAULT_MS
                                                          : intervalValue.as<int>();
                    if (interval < PLUGIN_PERIODIC_INTERVAL_MIN_MS ||
                        interval > PLUGIN_PERIODIC_INTERVAL_MAX_MS)
                        return false;
                    o.intervalMs = static_cast<uint16_t>(interval);
                    bool requestedSilent = false;
                    if (!pluginReadBool(op["gtw_silent"], false, requestedSilent))
                        return false;
                    if (!requestedSilent && !op["silent"].isNull() &&
                        !pluginReadBool(op["silent"], false, requestedSilent))
                        return false;
                    o.gtwSilent = requestedSilent && pluginGtwSilentSupported();
                    hasPeriodicEmit = true;

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
                    return false;
                }
                if (o.type == OP_SET_BIT && o.index >= 64)
                    return false;
                if ((o.type == OP_SET_BYTE || o.type == OP_OR_BYTE || o.type == OP_AND_BYTE ||
                     o.type == OP_COUNTER) &&
                    o.index >= 8)
                    return false;
                if (o.type == OP_COUNTER)
                {
                    uint8_t shift = 0;
                    while (shift < 8 && (o.mask & (1U << shift)) == 0)
                        shift++;
                    uint8_t shiftedMask = o.mask >> shift;
                    if (o.mask == 0 || (shiftedMask & static_cast<uint8_t>(shiftedMask + 1)) != 0)
                        return false;
                }
                r.opCount++;
            }
        }
        if (hasPeriodicEmit &&
            (r.canId != 2047 || !pluginRuleMuxIncludes(r, 3) || !r.sendAfter))
            return false;

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
    const String tempPath = "/.plugin.tmp";
    const String targetPath = pluginFilePath(filename);
    const String backupPath = String("/b_") + filename;
    File f = SPIFFS.open(tempPath, "w");
    if (!f)
        return false;
    bool ok = f.print(json) == json.length();
    ok = f.close() && ok;
    bool hadTarget = ok && SPIFFS.exists(targetPath);
    if (ok && hadTarget)
    {
        if (SPIFFS.exists(backupPath))
            SPIFFS.remove(backupPath);
        ok = SPIFFS.rename(targetPath, backupPath);
    }
    if (ok)
    {
        ok = SPIFFS.rename(tempPath, targetPath);
        if (!ok && hadTarget)
            SPIFFS.rename(backupPath, targetPath);
    }
    if (ok && hadTarget)
        SPIFFS.remove(backupPath);
    if (!ok)
        SPIFFS.remove(tempPath);
    return ok;
}

static void pluginRecoverInterruptedSaves()
{
    String backups[PLUGIN_MAX];
    uint8_t backupCount = 0;
    {
        File root = SPIFFS.open("/");
        File f = root.openNextFile();
        while (f && backupCount < PLUGIN_MAX)
        {
            String name = f.name();
            if (name.startsWith("/"))
                name = name.substring(1);
            if (name.startsWith("b_") && name.endsWith(".json"))
                backups[backupCount++] = name;
            f = root.openNextFile();
        }
    }

    for (uint8_t i = 0; i < backupCount; i++)
    {
        String backupPath = "/" + backups[i];
        String targetPath = "/p_" + backups[i].substring(2);
        if (SPIFFS.exists(targetPath))
            SPIFFS.remove(backupPath);
        else
            SPIFFS.rename(backupPath, targetPath);
    }
    if (SPIFFS.exists("/.plugin.tmp"))
        SPIFFS.remove("/.plugin.tmp");
}

static void pluginLoadAll()
{
    PluginLockGuard guard;
    pluginRecoverInterruptedSaves();
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
                p.enabled = false;
                // Store just the user-facing filename (without /p_ prefix)
                String userFilename = name.substring(2); // remove "p_"
                strlcpy(p.filename, userFilename.c_str(), sizeof(p.filename));
                p.priority = pluginCount;
                pluginCount++;
            }
        }
        f = root.openNextFile();
    }

    pluginResetDiagnostics();
    pluginsLocked = false;
}

static bool pluginRemove(uint8_t index)
{
    PluginLockGuard guard;
    if (index >= pluginCount)
        return false;
    pluginsLocked = true;

    if (!SPIFFS.remove(pluginFilePath(pluginStore[index].filename)))
    {
        pluginsLocked = false;
        return false;
    }

    for (uint8_t i = index; i < pluginCount - 1; i++)
    {
        pluginStore[i] = pluginStore[i + 1];
        pluginStore[i].priority = i;
    }
    pluginCount--;
    pluginResetDiagnostics();

    pluginsLocked = false;
    return true;
}
#endif

static void pluginNormalizePriorities()
{
    for (uint8_t i = 0; i < pluginCount; i++)
        pluginStore[i].priority = i;
}

static void pluginResetDiagnostics()
{
    PluginLockGuard guard;
    for (uint8_t p = 0; p < pluginCount; p++)
    {
        for (uint8_t r = 0; r < pluginStore[p].ruleCount; r++)
            pluginStore[p].rules[r].diag = {};
    }
}

static void pluginSortByPriority()
{
    PluginLockGuard guard;
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
    PluginLockGuard guard;
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
    pluginResetDiagnostics();
    pluginsLocked = false;
    return true;
}

static bool pluginMove(uint8_t from, uint8_t to)
{
    PluginLockGuard guard;
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
    pluginResetDiagnostics();
    pluginsLocked = false;
    return true;
}

static int pluginFindByName(const char *name)
{
    PluginLockGuard guard;
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

static void pluginCopyDiagFrame(uint8_t *out, const CanFrame &frame)
{
    for (uint8_t i = 0; i < 8; i++)
        out[i] = frame.data[i];
}

static void pluginNoteRuleMatched(PluginRule &rule, unsigned long now)
{
    uint32_t before = rule.diag.matchCount;
    rule.diag.matchCount++;
    rule.diag.lastMatchMs = now;
    if (before == 0)
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "[PLG] Rule match CAN 0x%03lX mux %d",
                 static_cast<unsigned long>(rule.canId), static_cast<int>(rule.mux));
        pluginLogDiagnosticsEvent(msg);
    }
}

static void pluginNoteRuleChanged(PluginRule &rule, const CanFrame &original, const CanFrame &modified)
{
    rule.diag.changedCount++;
    pluginCopyDiagFrame(rule.diag.lastOriginal, original);
    pluginCopyDiagFrame(rule.diag.lastModified, modified);
}

static void pluginNoteRuleSend(PluginRule &rule, bool ok, unsigned long now)
{
    rule.diag.lastSendMs = now;
    if (ok)
    {
        rule.diag.sendOkCount++;
        return;
    }

    uint32_t before = rule.diag.sendFailCount;
    rule.diag.sendFailCount++;
    if (before == 0)
    {
        char msg[68];
        snprintf(msg, sizeof(msg), "[PLG] Rule send failed CAN 0x%03lX mux %d",
                 static_cast<unsigned long>(rule.canId), static_cast<int>(rule.mux));
        pluginLogDiagnosticsEvent(msg);
    }
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
    PluginLockGuard guard(false);
    if (!guard)
        return;
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

    if (driver.send(pluginPeriodicEmit.frame))
        pluginAdvanceCounters(pluginPeriodicEmit.frame, pluginPeriodicEmit.counterOps,
                              pluginPeriodicEmit.counterOpCount, pluginPeriodicEmit.checksum);
    pluginPeriodicEmit.nextFrameAt = now + pluginPeriodicEmit.intervalMs;
}

static bool pluginProcessFrame(const CanFrame &original, CanDriver &driver)
{
    PluginLockGuard guard(false);
    if (!guard)
        return false;
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
    PluginRule *changedRules[PLUGIN_MAX * PLUGIN_RULES_MAX];
    uint8_t changedRuleCount = 0;
    uint64_t claimed = 0;
    uint8_t dlc = original.dlc <= 8 ? original.dlc : 8;
    uint64_t validWriteMask = dlc == 8 ? UINT64_MAX : (dlc == 0 ? 0 : (1ULL << (dlc * 8)) - 1);
    CanFrame modified = original;
    unsigned long now = millis();

    for (uint8_t p = 0; p < pluginCount; p++)
    {
        if (!pluginStore[p].enabled)
            continue;
        uint64_t pluginTouched = 0;
        for (uint8_t r = 0; r < pluginStore[p].ruleCount; r++)
        {
            PluginRule &rule = pluginStore[p].rules[r];
            if (rule.canId != original.id)
                continue;

            if (!pluginRuleMatchesBus(rule, original) || !pluginRuleMatchesMux(rule, original) ||
                !pluginRuleMatchesByte(rule, original))
                continue;

            processed = true;
            pluginNoteRuleMatched(rule, now);
            if (!rule.sendAfter)
                continue;

            sendRequested = true;
            bool ruleTouched = false;
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
                uint64_t opMask = pluginOpWriteMask(op) & validWriteMask;
                uint64_t allowedMask = opMask & ~claimed;
                if (op.type == OP_CHECKSUM)
                {
                    if (allowedMask == opMask && pluginApplyOpMasked(modified, op, allowedMask))
                    {
                        pluginTouched |= opMask;
                        checksumPending = true;
                        ruleTouched = true;
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
                        ruleTouched = true;
                    }
                    continue;
                }

                if (allowedMask != 0 && pluginApplyOpMasked(modified, op, allowedMask))
                {
                    pluginTouched |= allowedMask;
                    ruleTouched = true;
                }
            }
            if (ruleTouched && changedRuleCount < (PLUGIN_MAX * PLUGIN_RULES_MAX))
                changedRules[changedRuleCount++] = &rule;
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
            for (uint8_t i = 0; i < changedRuleCount; i++)
                pluginNoteRuleChanged(*changedRules[i], original, modified);
            uint8_t replayCount = original.id == 2047 ? pluginGetReplayCount() : 1;
            CanFrame replayFrame = modified;
            bool lastSendOk = false;
            for (uint8_t i = 0; i < replayCount; i++)
            {
                bool sendOk = driver.send(replayFrame);
                lastSendOk = sendOk;
                unsigned long sendNow = millis();
                for (uint8_t ruleIndex = 0; ruleIndex < changedRuleCount; ruleIndex++)
                    pluginNoteRuleSend(*changedRules[ruleIndex], sendOk, sendNow);
                if (!sendOk || i + 1 >= replayCount)
                    continue;
                pluginAdvanceCounters(replayFrame, counterOps, counterOpCount, checksumPending);
            }
            periodicFrame = replayFrame;
            if (lastSendOk)
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
    PluginLockGuard guard;
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
