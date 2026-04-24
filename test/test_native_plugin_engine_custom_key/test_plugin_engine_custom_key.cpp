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
    TEST_ASSERT_EQUAL_HEX8(0x35, driver.sent[2].data[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00, driver.sent[2].data[4]);
    TEST_ASSERT_EQUAL_HEX8(0xCA, driver.sent[2].data[5]);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_filter_ids_keep_sixteen_rule_ids_plus_uds_ids_when_custom_key_available);
    RUN_TEST(test_gtw_silent_uds_requests_use_cached_frame_bus);
    RUN_TEST(test_gtw_silent_key_request_uses_configured_xor_byte);
    return UNITY_END();
}
