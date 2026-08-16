#pragma once

// BLE (NimBLE) app interface.
//
// Exposes a small GATT service so a phone app can talk to the device over
// Bluetooth LE, mirroring the HTTP dashboard. Gated by -DBLE_APP (only wired
// on BT-capable envs) and compiled out of native builds.
//
// GATT layout (Nordic-UART style):
//   Service   e7c10001-9a3f-4bd2-b1a7-0c0ffee00001
//   Command   e7c10002-...  WRITE       app -> device, newline-framed JSON
//   Response  e7c10003-...  NOTIFY      device -> app, newline-framed JSON
//
// A written command is a JSON object {"cmd":"...","args":{...}} terminated by
// '\n'. Writes are accumulated until that newline arrives, so a command longer
// than one ATT write is simply split by the client -- but a SINGLE write may not
// exceed 255 bytes (see the flat buffer in bleCmdWriteCb).
//
// The reply is fetched by paged READs of the Response characteristic, because a
// GATT attribute is capped at 512 bytes and replies can be larger.
//
// Both characteristics require an encrypted, authenticated link (LE Secure
// Connections + passkey + bonding), so an unpaired peer can connect but cannot
// read or write anything. State-changing commands rely on that.

#if defined(BLE_APP) && !defined(NATIVE_BUILD) && defined(CONFIG_BT_NIMBLE_ENABLED)

#include <string.h>
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

// Provided by the dashboard (include order: app.h pulls the dashboard first).
static String dashBuildBleStatusJson();

static const char *kBleTag = "ble";
static const char *kBleDeviceName = "EVCANTool";

// 128-bit UUIDs, stored least-significant-byte first (NimBLE convention).
static const ble_uuid128_t bleSvcUuid = BLE_UUID128_INIT(
    0x01, 0x00, 0xe0, 0xfe, 0x0f, 0x0c, 0xa7, 0xb1,
    0xd2, 0x4b, 0x3f, 0x9a, 0x01, 0x00, 0xc1, 0xe7);
static const ble_uuid128_t bleCmdUuid = BLE_UUID128_INIT(
    0x01, 0x00, 0xe0, 0xfe, 0x0f, 0x0c, 0xa7, 0xb1,
    0xd2, 0x4b, 0x3f, 0x9a, 0x02, 0x00, 0xc1, 0xe7);
static const ble_uuid128_t bleRespUuid = BLE_UUID128_INIT(
    0x01, 0x00, 0xe0, 0xfe, 0x0f, 0x0c, 0xa7, 0xb1,
    0xd2, 0x4b, 0x3f, 0x9a, 0x03, 0x00, 0xc1, 0xe7);

static uint16_t bleRespHandle = 0;
static uint16_t bleConnHandle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t bleMtu = 23;
static uint8_t bleOwnAddrType = 0;
static String bleCmdBuf;

static void bleStartAdvertising();

// ---- send: one-shot frame injection ---------------------------------------
//
// One app button maps to a short burst of frames written as a single command.
// Deliberately absent: a per-frame delay (it would block the NimBLE host task
// for its duration) and extended 29-bit ids (CanFrame carries no extended flag
// and the drivers reject id > 0x7FF anyway).

// Bounds the burst so a malformed or hostile payload cannot exhaust the host
// task stack. 16 frames * sizeof(CanFrame) is a few hundred bytes.
static const size_t kBleSendMaxFrames = 16;

static String bleReject(const char *error, const char *reason, int index = -1)
{
    String j = "{\"ok\":false,\"error\":\"";
    j += error;
    j += "\"";
    if (reason)
    {
        j += ",\"reason\":\"";
        j += reason;
        j += "\"";
    }
    if (index >= 0)
    {
        j += ",\"index\":";
        j += index;
    }
    j += "}";
    return j;
}

// Which sub-gate of dashInjectionActive() is closed, so the app can tell the
// user *why* a button did nothing instead of just failing.
static const char *bleInjectionBlockReason()
{
    if (!canActive)
        return "injection disabled";
    if (!appInjectionReady())
        return "warming up";
    if (!dashApInjectionAllowed())
        return "ap gate";
    if (!dashSummonOnlyInjectionAllowed())
        return "summon-only gate";
    return nullptr;
}

static bool bleParseHexNibble(char c, uint8_t &out)
{
    if (c >= '0' && c <= '9')
        out = (uint8_t)(c - '0');
    else if (c >= 'a' && c <= 'f')
        out = (uint8_t)(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F')
        out = (uint8_t)(c - 'A' + 10);
    else
        return false;
    return true;
}

// "0011AABB" -> bytes. Rejects odd lengths, non-hex and payloads over 8 bytes.
static bool bleParseHexPayload(const char *hex, uint8_t *out, uint8_t &len)
{
    if (!hex)
        return false;
    size_t n = strlen(hex);
    if (n == 0 || (n & 1) != 0 || n > 16)
        return false;
    len = (uint8_t)(n / 2);
    for (uint8_t i = 0; i < len; ++i)
    {
        uint8_t hi = 0, lo = 0;
        if (!bleParseHexNibble(hex[i * 2], hi) || !bleParseHexNibble(hex[i * 2 + 1], lo))
            return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

// An 11-bit id, either as a number (993) or as a hex string ("0x3E1" / "3E1").
static bool bleParseCanId(JsonVariantConst v, uint32_t &out)
{
    if (v.is<const char *>())
    {
        const char *s = v.as<const char *>();
        if (!s || !*s)
            return false;
        char *end = nullptr;
        unsigned long parsed = strtoul(s, &end, 16);
        if (!end || *end != '\0')
            return false;
        out = (uint32_t)parsed;
        return true;
    }
    if (v.is<int>() || v.is<unsigned int>() || v.is<long>())
    {
        long parsed = v.as<long>();
        if (parsed < 0)
            return false;
        out = (uint32_t)parsed;
        return true;
    }
    return false;
}

static String bleHandleSend(JsonObjectConst args)
{
    if (!dashDriver)
        return bleReject("no driver", nullptr);
    // Same gates as automatic injection: a phone button must not be able to put
    // frames on the bus in a state where the firmware would refuse to itself.
    if (const char *blocked = bleInjectionBlockReason())
        return bleReject("gated", blocked);

    JsonArrayConst frames = args["frames"];
    if (frames.isNull() || frames.size() == 0)
        return bleReject("bad args", "no frames");
    if (frames.size() > kBleSendMaxFrames)
        return bleReject("bad args", "too many frames");

    // Validate every frame before sending any, so a typo in the last frame does
    // not leave a half-sent burst on the bus.
    CanFrame parsed[kBleSendMaxFrames];
    int count = 0;
    for (JsonObjectConst f : frames)
    {
        uint32_t id = 0;
        if (!bleParseCanId(f["id"], id))
            return bleReject("bad frame", "id must be a number or hex string", count);
        if (id > 0x7FF)
            return bleReject("bad frame", "id above 11-bit range", count);
        uint8_t dlc = 0;
        if (!bleParseHexPayload(f["data"], parsed[count].data, dlc))
            return bleReject("bad frame", "data must be 2-16 hex digits", count);
        long bus = f["bus"] | (long)CAN_BUS_DEFAULT;
        if (bus < 0 || bus > 0xFF)
            return bleReject("bad frame", "bus out of range", count);
        parsed[count].id = id;
        parsed[count].dlc = dlc;
        parsed[count].bus = (uint8_t)bus;
        ++count;
    }

    int sent = 0;
    while (sent < count && dashDriver->send(parsed[sent]))
        ++sent;

    String j = "{\"ok\":";
    j += (sent == count) ? "true" : "false";
    j += ",\"sent\":";
    j += sent;
    if (sent != count)
        j += ",\"error\":\"tx failed\"";
    j += "}";
    return j;
}

// ---- config: the dashboard's settings, over BLE ---------------------------

// Feeds a BLE command's args into the shared ctrlApplyConfig. Values arrive as
// JSON types but the validators take strings, so numbers and booleans are
// rendered into the forms dashParseLong/dashParseBool accept ("1"/"0" for
// booleans -- nothing else is valid there).
struct BleConfigArgs : ConfigArgs
{
    JsonObjectConst args;

    explicit BleConfigArgs(JsonObjectConst a) : args(a) {}

    bool has(const char *name) const override
    {
        return !args[name].isNull();
    }

    String get(const char *name) const override
    {
        JsonVariantConst v = args[name];
        if (v.is<bool>())
            return v.as<bool>() ? String("1") : String("0");
        if (v.is<long>())
            return String(v.as<long>());
        const char *s = v.as<const char *>();
        return s ? String(s) : String("");
    }
};

// Live runtime figures: what the car and the link are actually doing. Separate
// from `status` so the frequently polled reply stays small.
static String bleBuildStatsJson()
{
    uint32_t now = millis();
    DashApGateSnapshot gate = dashApGateSnapshot();
    String j = "{\"ok\":true";
    j += ",\"uptimeS\":";
    j += (unsigned long)(now / 1000);
    j += ",\"canFrames\":";
    j += (unsigned long)RuntimeDiagnostics::canFrames.load(std::memory_order_relaxed);
    j += ",\"canAgeMs\":";
    j += (unsigned long)RuntimeDiagnostics::canAgeMs(now);
    j += ",\"txOk\":";
    j += (unsigned long)RuntimeDiagnostics::txOk.load(std::memory_order_relaxed);
    j += ",\"txFail\":";
    j += (unsigned long)RuntimeDiagnostics::txFail.load(std::memory_order_relaxed);
    j += ",\"freeHeap\":";
    j += (unsigned long)esp_get_free_heap_size();
    // The vehicle state the injection gates key off, so the app can explain a
    // "gated" rejection instead of just reporting it.
    j += ",\"gateEnabled\":";
    j += gate.enabled ? "true" : "false";
    j += ",\"gateAllowed\":";
    j += gate.allowed ? "true" : "false";
    j += ",\"apActive\":";
    j += gate.apActive ? "true" : "false";
    j += ",\"parked\":";
    j += gate.parked ? "true" : "false";
    j += ",\"summoning\":";
    j += gate.summoning ? "true" : "false";
    j += ",\"gateReason\":\"";
    j += gate.reason;
    j += "\"}";
    return j;
}

// Transport-neutral command dispatch. Seed of the shared command core that a
// future HTTP /api endpoint can reuse.
static String bleDispatchCommand(JsonObjectConst root)
{
    const char *cmd = root["cmd"] | "";
    if (strcmp(cmd, "status") == 0)
        return dashBuildBleStatusJson();
    if (strcmp(cmd, "ping") == 0)
        return String("{\"ok\":true,\"pong\":true}");
    if (strcmp(cmd, "send") == 0)
        return bleHandleSend(root["args"]);
    if (strcmp(cmd, "stats") == 0)
        return bleBuildStatsJson();
    if (strcmp(cmd, "config") == 0)
    {
        // No args means read; any args mean apply them. Same call the dashboard
        // makes, so validation cannot differ between the two transports.
        JsonObjectConst args = root["args"];
        if (args.isNull() || args.size() == 0)
        {
            String j = "{\"ok\":true,\"config\":";
            j += ctrlBuildConfigJson();
            j += "}";
            return j;
        }
        BleConfigArgs source(args);
        ConfigResult result = ctrlApplyConfig(source);
        if (result.status != 200)
            return result.body;
        // Echo the stored state back: a request may be partially applied (an
        // unknown key is simply not read), and clamping happens inside.
        String j = "{\"ok\":true,\"config\":";
        j += ctrlBuildConfigJson();
        j += "}";
        return j;
    }
    if (strcmp(cmd, "inject") == 0)
    {
        // The master injection switch, otherwise reachable only from the web
        // dashboard -- which is unreachable in BLE mode, leaving the app unable
        // to clear its own most common "gated" rejection.
        JsonVariantConst on = root["args"]["on"];
        if (!on.is<bool>())
            return bleReject("bad args", "on must be true or false");
        dashSetCanActive(on.as<bool>(), "ble");
        String j = "{\"ok\":true,\"inject\":";
        j += canActive ? "true" : "false";
        j += "}";
        return j;
    }
    if (strcmp(cmd, "wifi_mode") == 0)
    {
        // Switch back to the WiFi dashboard (clears the BLE-mode flag + reboots).
        dashSetBleMode(false);
        return String("{\"ok\":true,\"reboot\":true}");
    }
    return String("{\"ok\":false,\"error\":\"unknown cmd\"}");
}

// The most recent reply, fetched by the client via paged READs of the Response
// characteristic. A single GATT attribute value is capped at 512 bytes by the
// spec, and replies (status, config, backups) can be larger, so each read
// returns an 8-byte little-endian header (uint32 total, uint32 offset) followed
// by up to kBleReadPage bytes of the reply. The client reads repeatedly,
// advancing until it has `total` bytes. Reliable at any MTU.
static const uint32_t kBleReadPage = 400;
static String bleRespBuffer = "{}";
static uint32_t bleRespCursor = 0;

static void bleStoreResponse(const String &payload)
{
    bleRespBuffer = payload;
    bleRespCursor = 0;
}

static int bleCmdWriteCb(uint16_t /*conn_handle*/, uint16_t /*attr_handle*/,
                         struct ble_gatt_access_ctxt *ctxt, void * /*arg*/)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
        return BLE_ATT_ERR_UNLIKELY;

    char buf[256];
    uint16_t outLen = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &outLen) != 0)
        return BLE_ATT_ERR_UNLIKELY;
    buf[outLen] = '\0';
    bleCmdBuf += buf;

    // Guard against unbounded growth from a peer that never sends a newline.
    if (bleCmdBuf.length() > 4096)
        bleCmdBuf = "";

    int nl;
    while ((nl = bleCmdBuf.indexOf('\n')) >= 0)
    {
        String cmd = bleCmdBuf.substring(0, nl);
        bleCmdBuf = bleCmdBuf.substring(nl + 1);
        if (!cmd.length())
            continue;
        // Parse once. Passing c_str() (const) avoids ArduinoJson's in-place
        // parse mutating the String. Trailing '\r' is tolerated as whitespace.
        JsonDocument doc;
        if (deserializeJson(doc, cmd.c_str()))
        {
            bleStoreResponse(String("{\"ok\":false,\"error\":\"bad json\"}"));
            continue;
        }
        // {"cmd":"next"} pages through the current reply without replacing it.
        if (strcmp(doc["cmd"] | "", "next") == 0)
        {
            uint32_t total = (uint32_t)bleRespBuffer.length();
            bleRespCursor = (bleRespCursor + kBleReadPage < total)
                                ? bleRespCursor + kBleReadPage
                                : total;
            continue;
        }
        bleStoreResponse(bleDispatchCommand(doc.as<JsonObjectConst>()));
    }
    return 0;
}

static int bleRespAccessCb(uint16_t /*conn*/, uint16_t /*attr*/,
                           struct ble_gatt_access_ctxt *ctxt, void * /*arg*/)
{
    // Reads return the next page of the last command's reply (header + slice).
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
    {
        uint32_t total = (uint32_t)bleRespBuffer.length();
        uint32_t off = bleRespCursor > total ? total : bleRespCursor;
        uint32_t remaining = total - off;
        uint32_t n = remaining < kBleReadPage ? remaining : kBleReadPage;
        uint8_t hdr[8] = {
            (uint8_t)total, (uint8_t)(total >> 8), (uint8_t)(total >> 16), (uint8_t)(total >> 24),
            (uint8_t)off, (uint8_t)(off >> 8), (uint8_t)(off >> 16), (uint8_t)(off >> 24)};
        if (os_mbuf_append(ctxt->om, hdr, sizeof(hdr)) != 0)
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        if (n > 0 &&
            os_mbuf_append(ctxt->om, bleRespBuffer.c_str() + off, n) != 0)
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        // Do NOT advance the cursor here: a single logical read is assembled by
        // the client from several ATT Read Blob requests, each of which calls
        // this callback, so the page must stay stable. The client advances to
        // the next page with a {"cmd":"next"} write.
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// Partial designated initializers leave later members zero, which is intended;
// silence -Werror=missing-field-initializers (NimBLE's own examples do the same).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
static const struct ble_gatt_svc_def bleGattSvcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &bleSvcUuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &bleCmdUuid.u,
                .access_cb = bleCmdWriteCb,
                // Encrypted + authenticated: only a peer that paired with the
                // passkey (bonded) can send commands. Locks out third parties.
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC |
                         BLE_GATT_CHR_F_WRITE_AUTHEN,
            },
            {
                .uuid = &bleRespUuid.u,
                .access_cb = bleRespAccessCb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY |
                         BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_READ_AUTHEN,
                .val_handle = &bleRespHandle,
            },
            {0},
        },
    },
    {0},
};
#pragma GCC diagnostic pop

static int bleGapEvent(struct ble_gap_event *event, void * /*arg*/)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0)
        {
            bleConnHandle = event->connect.conn_handle;
            bleMtu = 23;
            // The central initiates the ATT MTU exchange; we answer with the
            // preferred MTU set in bleServiceSetup. bleMtu is updated in the
            // BLE_GAP_EVENT_MTU event below.
        }
        else
        {
            bleStartAdvertising();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        bleConnHandle = BLE_HS_CONN_HANDLE_NONE;
        bleCmdBuf = "";
        bleStartAdvertising();
        return 0;
    case BLE_GAP_EVENT_MTU:
        bleMtu = event->mtu.value;
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        bleStartAdvertising();
        return 0;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        // The central asks us to "display" the passkey; inject the dashboard PIN
        // so pairing is MITM-protected without a real screen on the device.
        if (event->passkey.params.action == BLE_SM_IOACT_DISP)
        {
            struct ble_sm_io pk;
            memset(&pk, 0, sizeof(pk));
            pk.action = BLE_SM_IOACT_DISP;
            pk.passkey = dashBlePasskey;
            ble_sm_inject_io(event->passkey.conn_handle, &pk);
        }
        return 0;
    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(kBleTag, "encryption change; status=%d", event->enc_change.status);
        return 0;
    default:
        return 0;
    }
}

static void bleStartAdvertising()
{
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)kBleDeviceName;
    fields.name_len = strlen(kBleDeviceName);
    fields.name_is_complete = 1;
    if (ble_gap_adv_set_fields(&fields) != 0)
        return;

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ble_gap_adv_start(bleOwnAddrType, NULL, BLE_HS_FOREVER, &adv_params,
                      bleGapEvent, NULL);
}

static void bleOnSync()
{
    if (ble_hs_util_ensure_addr(0) != 0)
        return;
    if (ble_hs_id_infer_auto(0, &bleOwnAddrType) != 0)
        return;
    bleStartAdvertising();
}

static void bleOnReset(int reason)
{
    ESP_LOGW(kBleTag, "BLE stack reset; reason=%d", reason);
}

static void bleHostTask(void * /*param*/)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void bleServiceSetup()
{
    if (nimble_port_init() != ESP_OK)
    {
        ESP_LOGE(kBleTag, "nimble_port_init failed");
        return;
    }
    ble_hs_cfg.sync_cb = bleOnSync;
    ble_hs_cfg.reset_cb = bleOnReset;

    // Security: LE Secure Connections with a passkey (MITM) and bonding. The
    // device "displays" the passkey (we inject the dashboard-configured PIN);
    // the user types it into the app. Bonds persist in NVS.
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_att_set_preferred_mtu(247);
    ble_svc_gap_init();
    ble_svc_gatt_init();

    if (ble_gatts_count_cfg(bleGattSvcs) != 0 ||
        ble_gatts_add_svcs(bleGattSvcs) != 0)
    {
        ESP_LOGE(kBleTag, "GATT registration failed");
        return;
    }
    ble_svc_gap_device_name_set(kBleDeviceName);
    nimble_port_freertos_init(bleHostTask);
}

#endif // BLE_APP && !NATIVE_BUILD
