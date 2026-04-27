#include <unity.h>

#include "can_frame_types.h"
#include "drivers/mock_driver.h"

static unsigned long fakeMillis = 1000;
static unsigned long millis()
{
    return fakeMillis;
}

#include "plugin_engine.h"

void setUp()
{
    pluginCount = 0;
    pluginsLocked = false;
    pluginResetPeriodicEmit();
    fakeMillis = 1000;
}

void tearDown() {}

static void installPlugin(const char *json)
{
    PluginData plugin = {};
    TEST_ASSERT_TRUE(pluginParseJson(json, plugin));
    TEST_ASSERT_TRUE(pluginInsert(pluginCount, plugin));
}

static void startSilentHandshakeToSeedRequest(MockDriver &driver)
{
    installPlugin(R"JSON({
      "name":"silent isotp",
      "rules":[{
        "id":2047,
        "bus":"VEH",
        "mux":3,
        "ops":[{"type":"emit_periodic","interval":100,"gtw_silent":true}]
      }]
    })JSON");

    CanFrame gtw = {.id = 2047};
    gtw.bus = CAN_BUS_VEH;
    gtw.data[0] = 3;
    TEST_ASSERT_TRUE(pluginProcessFrame(gtw, driver));

    pluginEmitPeriodicTick(driver, fakeMillis);
    TEST_ASSERT_EQUAL_size_t(1, driver.sent.size());

    CanFrame sessionResponse = {.id = PLUGIN_GTW_UDS_RESPONSE_ID, .dlc = 8};
    sessionResponse.bus = CAN_BUS_VEH;
    sessionResponse.data[0] = 0x02;
    sessionResponse.data[1] = 0x50;
    sessionResponse.data[2] = 0x03;
    TEST_ASSERT_TRUE(pluginProcessFrame(sessionResponse, driver));

    pluginEmitPeriodicTick(driver, fakeMillis);
    TEST_ASSERT_EQUAL_size_t(2, driver.sent.size());
}

void test_filter_ids_keep_sixteen_rule_ids_plus_uds_ids_when_custom_key_available()
{
    PluginData plugin = {};
    const char *json = R"JSON({
      "name":"filters",
      "rules":[
        {"id":100,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":101,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":102,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":103,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":104,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":105,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":106,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":107,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":108,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":109,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":110,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":111,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":112,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":113,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":114,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":2047,"mux":3,"ops":[{"type":"emit_periodic","interval":100,"gtw_silent":true}]}
      ]
    })JSON";

    TEST_ASSERT_TRUE(pluginParseJson(json, plugin));
    TEST_ASSERT_EQUAL_UINT8(16, plugin.ruleCount);
    TEST_ASSERT_EQUAL_UINT8(18, plugin.filterIdCount);

    bool sawUdsRequest = false;
    bool sawUdsResponse = false;
    bool sawGtw = false;
    for (uint8_t i = 0; i < plugin.filterIdCount; i++)
    {
        sawUdsRequest |= plugin.filterIds[i] == PLUGIN_GTW_UDS_REQUEST_ID;
        sawUdsResponse |= plugin.filterIds[i] == PLUGIN_GTW_UDS_RESPONSE_ID;
        sawGtw |= plugin.filterIds[i] == 2047;
    }
    TEST_ASSERT_TRUE(sawUdsRequest);
    TEST_ASSERT_TRUE(sawUdsResponse);
    TEST_ASSERT_TRUE(sawGtw);
}

void test_gtw_silent_uds_requests_use_cached_frame_bus()
{
    installPlugin(R"JSON({
      "name":"silent bus",
      "rules":[{
        "id":2047,
        "bus":"VEH",
        "mux":3,
        "ops":[{"type":"emit_periodic","interval":100,"gtw_silent":true}]
      }]
    })JSON");

    MockDriver driver;
    CanFrame gtw = {.id = 2047};
    gtw.bus = CAN_BUS_VEH;
    gtw.data[0] = 3;
    TEST_ASSERT_TRUE(pluginProcessFrame(gtw, driver));
    TEST_ASSERT_EQUAL_size_t(0, driver.sent.size());

    pluginEmitPeriodicTick(driver, fakeMillis);
    TEST_ASSERT_EQUAL_size_t(1, driver.sent.size());
    TEST_ASSERT_EQUAL_UINT32(PLUGIN_GTW_UDS_REQUEST_ID, driver.sent[0].id);
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_VEH, driver.sent[0].bus);
}

void test_gtw_silent_key_request_uses_configured_xor_byte()
{
    installPlugin(R"JSON({
      "name":"silent key",
      "rules":[{
        "id":2047,
        "bus":"VEH",
        "mux":3,
        "ops":[{"type":"emit_periodic","interval":100,"gtw_silent":true}]
      }]
    })JSON");

    MockDriver driver;
    CanFrame gtw = {.id = 2047};
    gtw.bus = CAN_BUS_VEH;
    gtw.data[0] = 3;
    TEST_ASSERT_TRUE(pluginProcessFrame(gtw, driver));

    pluginEmitPeriodicTick(driver, fakeMillis);
    TEST_ASSERT_EQUAL_size_t(1, driver.sent.size());

    CanFrame sessionResponse = {.id = PLUGIN_GTW_UDS_RESPONSE_ID, .dlc = 8};
    sessionResponse.bus = CAN_BUS_VEH;
    sessionResponse.data[0] = 0x02;
    sessionResponse.data[1] = 0x50;
    sessionResponse.data[2] = 0x03;
    TEST_ASSERT_TRUE(pluginProcessFrame(sessionResponse, driver));

    pluginEmitPeriodicTick(driver, fakeMillis);
    TEST_ASSERT_EQUAL_size_t(2, driver.sent.size());

    CanFrame seedResponse = {.id = PLUGIN_GTW_UDS_RESPONSE_ID, .dlc = 8};
    seedResponse.bus = CAN_BUS_VEH;
    seedResponse.data[0] = 0x05;
    seedResponse.data[1] = 0x67;
    seedResponse.data[2] = 0x01;
    seedResponse.data[3] = 0x00;
    seedResponse.data[4] = 0x35;
    seedResponse.data[5] = 0xFF;
    TEST_ASSERT_TRUE(pluginProcessFrame(seedResponse, driver));

    pluginEmitPeriodicTick(driver, fakeMillis);
    TEST_ASSERT_EQUAL_size_t(3, driver.sent.size());
    TEST_ASSERT_EQUAL_UINT32(PLUGIN_GTW_UDS_REQUEST_ID, driver.sent[2].id);
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_VEH, driver.sent[2].bus);
    TEST_ASSERT_EQUAL_HEX8(0x05, driver.sent[2].data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x27, driver.sent[2].data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, driver.sent[2].data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00 ^ PLUGIN_GTW_UDS_KEY_READY, driver.sent[2].data[3]);
    TEST_ASSERT_EQUAL_HEX8(0x35 ^ PLUGIN_GTW_UDS_KEY_READY, driver.sent[2].data[4]);
    TEST_ASSERT_EQUAL_HEX8(0xFF ^ PLUGIN_GTW_UDS_KEY_READY, driver.sent[2].data[5]);
}

void test_gtw_silent_timeout_records_failed_step()
{
    installPlugin(R"JSON({
      "name":"silent timeout",
      "rules":[{
        "id":2047,
        "bus":"VEH",
        "mux":3,
        "ops":[{"type":"emit_periodic","interval":100,"gtw_silent":true}]
      }]
    })JSON");

    MockDriver driver;
    CanFrame gtw = {.id = 2047};
    gtw.bus = CAN_BUS_VEH;
    gtw.data[0] = 3;
    TEST_ASSERT_TRUE(pluginProcessFrame(gtw, driver));

    pluginEmitPeriodicTick(driver, fakeMillis);
    fakeMillis += PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS;
    pluginEmitPeriodicTick(driver, fakeMillis);

    TEST_ASSERT_EQUAL_UINT8(GTW_UDS_FAILED, pluginPeriodicEmit.uds.state);
    TEST_ASSERT_EQUAL_UINT8(GTW_UDS_SESSION_REQ, pluginPeriodicEmit.uds.lastFailedState);
    TEST_ASSERT_EQUAL_HEX8(0xFF, pluginPeriodicEmit.uds.lastNrc);
    TEST_ASSERT_EQUAL_STRING("failed", pluginGtwUdsStateName(pluginPeriodicEmit.uds.state));
    TEST_ASSERT_EQUAL_STRING("session_req", pluginGtwUdsStateName(pluginPeriodicEmit.uds.lastFailedState));
    TEST_ASSERT_EQUAL_STRING("timeout", pluginGtwUdsNrcName(pluginPeriodicEmit.uds.lastNrc));
}

void test_gtw_silent_negative_response_records_failed_step_and_nrc_name()
{
    installPlugin(R"JSON({
      "name":"silent nrc",
      "rules":[{
        "id":2047,
        "bus":"VEH",
        "mux":3,
        "ops":[{"type":"emit_periodic","interval":100,"gtw_silent":true}]
      }]
    })JSON");

    MockDriver driver;
    CanFrame gtw = {.id = 2047};
    gtw.bus = CAN_BUS_VEH;
    gtw.data[0] = 3;
    TEST_ASSERT_TRUE(pluginProcessFrame(gtw, driver));

    pluginEmitPeriodicTick(driver, fakeMillis);

    CanFrame sessionResponse = {.id = PLUGIN_GTW_UDS_RESPONSE_ID, .dlc = 8};
    sessionResponse.bus = CAN_BUS_VEH;
    sessionResponse.data[0] = 0x02;
    sessionResponse.data[1] = 0x50;
    sessionResponse.data[2] = 0x03;
    TEST_ASSERT_TRUE(pluginProcessFrame(sessionResponse, driver));

    pluginEmitPeriodicTick(driver, fakeMillis);

    CanFrame seedResponse = {.id = PLUGIN_GTW_UDS_RESPONSE_ID, .dlc = 8};
    seedResponse.bus = CAN_BUS_VEH;
    seedResponse.data[0] = 0x03;
    seedResponse.data[1] = 0x67;
    seedResponse.data[2] = 0x01;
    seedResponse.data[3] = 0x00;
    TEST_ASSERT_TRUE(pluginProcessFrame(seedResponse, driver));

    pluginEmitPeriodicTick(driver, fakeMillis);

    CanFrame invalidKey = {.id = PLUGIN_GTW_UDS_RESPONSE_ID, .dlc = 8};
    invalidKey.bus = CAN_BUS_VEH;
    invalidKey.data[0] = 0x03;
    invalidKey.data[1] = 0x7F;
    invalidKey.data[2] = 0x27;
    invalidKey.data[3] = 0x35;
    TEST_ASSERT_TRUE(pluginProcessFrame(invalidKey, driver));

    TEST_ASSERT_EQUAL_UINT8(GTW_UDS_FAILED, pluginPeriodicEmit.uds.state);
    TEST_ASSERT_EQUAL_UINT8(GTW_UDS_KEY_SENT, pluginPeriodicEmit.uds.lastFailedState);
    TEST_ASSERT_EQUAL_HEX8(0x35, pluginPeriodicEmit.uds.lastNrc);
    TEST_ASSERT_EQUAL_STRING("key_sent", pluginGtwUdsStateName(pluginPeriodicEmit.uds.lastFailedState));
    TEST_ASSERT_EQUAL_STRING("invalid_key", pluginGtwUdsNrcName(pluginPeriodicEmit.uds.lastNrc));
}

void test_gtw_silent_reassembles_multiframe_seed_and_segments_key_request()
{
    MockDriver driver;
    startSilentHandshakeToSeedRequest(driver);

    CanFrame seedFirstFrame = {.id = PLUGIN_GTW_UDS_RESPONSE_ID, .dlc = 8};
    seedFirstFrame.bus = CAN_BUS_VEH;
    seedFirstFrame.data[0] = 0x10;
    seedFirstFrame.data[1] = 0x08;
    seedFirstFrame.data[2] = 0x67;
    seedFirstFrame.data[3] = 0x01;
    seedFirstFrame.data[4] = 0x01;
    seedFirstFrame.data[5] = 0x02;
    seedFirstFrame.data[6] = 0x03;
    seedFirstFrame.data[7] = 0x04;
    TEST_ASSERT_TRUE(pluginProcessFrame(seedFirstFrame, driver));
    TEST_ASSERT_EQUAL_size_t(3, driver.sent.size());
    TEST_ASSERT_EQUAL_UINT32(PLUGIN_GTW_UDS_REQUEST_ID, driver.sent[2].id);
    TEST_ASSERT_EQUAL_HEX8(0x30, driver.sent[2].data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, driver.sent[2].data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, driver.sent[2].data[2]);

    CanFrame seedConsecutive = {.id = PLUGIN_GTW_UDS_RESPONSE_ID, .dlc = 8};
    seedConsecutive.bus = CAN_BUS_VEH;
    seedConsecutive.data[0] = 0x21;
    seedConsecutive.data[1] = 0x05;
    seedConsecutive.data[2] = 0x06;
    TEST_ASSERT_TRUE(pluginProcessFrame(seedConsecutive, driver));

    TEST_ASSERT_EQUAL_UINT8(GTW_UDS_KEY_SENT, pluginPeriodicEmit.uds.state);
    TEST_ASSERT_EQUAL_UINT8(6, pluginPeriodicEmit.uds.seedLen);
    for (uint8_t i = 0; i < pluginPeriodicEmit.uds.seedLen; i++)
        TEST_ASSERT_EQUAL_HEX8(i + 1, pluginPeriodicEmit.uds.seed[i]);

    pluginEmitPeriodicTick(driver, fakeMillis);
    TEST_ASSERT_EQUAL_size_t(4, driver.sent.size());
    TEST_ASSERT_EQUAL_UINT32(PLUGIN_GTW_UDS_REQUEST_ID, driver.sent[3].id);
    TEST_ASSERT_EQUAL_HEX8(0x10, driver.sent[3].data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x08, driver.sent[3].data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x27, driver.sent[3].data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x02, driver.sent[3].data[3]);
    TEST_ASSERT_EQUAL_HEX8(0x01 ^ PLUGIN_GTW_UDS_KEY_READY, driver.sent[3].data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x02 ^ PLUGIN_GTW_UDS_KEY_READY, driver.sent[3].data[5]);
    TEST_ASSERT_EQUAL_HEX8(0x03 ^ PLUGIN_GTW_UDS_KEY_READY, driver.sent[3].data[6]);
    TEST_ASSERT_EQUAL_HEX8(0x04 ^ PLUGIN_GTW_UDS_KEY_READY, driver.sent[3].data[7]);

    CanFrame flowControl = {.id = PLUGIN_GTW_UDS_RESPONSE_ID, .dlc = 8};
    flowControl.bus = CAN_BUS_VEH;
    flowControl.data[0] = 0x30;
    TEST_ASSERT_TRUE(pluginProcessFrame(flowControl, driver));
    TEST_ASSERT_EQUAL_size_t(5, driver.sent.size());
    TEST_ASSERT_EQUAL_UINT32(PLUGIN_GTW_UDS_REQUEST_ID, driver.sent[4].id);
    TEST_ASSERT_EQUAL_HEX8(0x21, driver.sent[4].data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x05 ^ PLUGIN_GTW_UDS_KEY_READY, driver.sent[4].data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x06 ^ PLUGIN_GTW_UDS_KEY_READY, driver.sent[4].data[2]);
}

void test_gtw_silent_multiframe_seed_timeout_records_seed_step()
{
    MockDriver driver;
    startSilentHandshakeToSeedRequest(driver);

    CanFrame seedFirstFrame = {.id = PLUGIN_GTW_UDS_RESPONSE_ID, .dlc = 8};
    seedFirstFrame.bus = CAN_BUS_VEH;
    seedFirstFrame.data[0] = 0x10;
    seedFirstFrame.data[1] = 0x08;
    seedFirstFrame.data[2] = 0x67;
    seedFirstFrame.data[3] = 0x01;
    seedFirstFrame.data[4] = 0x01;
    seedFirstFrame.data[5] = 0x02;
    seedFirstFrame.data[6] = 0x03;
    seedFirstFrame.data[7] = 0x04;
    TEST_ASSERT_TRUE(pluginProcessFrame(seedFirstFrame, driver));

    fakeMillis += PLUGIN_GTW_UDS_RESPONSE_TIMEOUT_MS;
    pluginEmitPeriodicTick(driver, fakeMillis);

    TEST_ASSERT_EQUAL_UINT8(GTW_UDS_FAILED, pluginPeriodicEmit.uds.state);
    TEST_ASSERT_EQUAL_UINT8(GTW_UDS_SEED_REQ, pluginPeriodicEmit.uds.lastFailedState);
    TEST_ASSERT_EQUAL_HEX8(0xFF, pluginPeriodicEmit.uds.lastNrc);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_filter_ids_keep_sixteen_rule_ids_plus_uds_ids_when_custom_key_available);
    RUN_TEST(test_gtw_silent_uds_requests_use_cached_frame_bus);
    RUN_TEST(test_gtw_silent_key_request_uses_configured_xor_byte);
    RUN_TEST(test_gtw_silent_timeout_records_failed_step);
    RUN_TEST(test_gtw_silent_negative_response_records_failed_step_and_nrc_name);
    RUN_TEST(test_gtw_silent_reassembles_multiframe_seed_and_segments_key_request);
    RUN_TEST(test_gtw_silent_multiframe_seed_timeout_records_seed_step);
    return UNITY_END();
}
