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

void test_mux_mask_matches_full_byte_mux()
{
    installPlugin(R"JSON({
      "name":"full mux",
      "rules":[{
        "id":2047,
        "mux":3,
        "mux_mask":255,
        "ops":[
          {"type":"set_byte","byte":0,"val":3,"mask":255},
          {"type":"set_bit","bit":12,"val":1}
        ]
      }]
    })JSON");

    MockDriver driver;
    CanFrame wrongMux = {.id = 2047};
    wrongMux.data[0] = 0x83;
    TEST_ASSERT_FALSE(pluginProcessFrame(wrongMux, driver));
    TEST_ASSERT_EQUAL_size_t(0, driver.sent.size());

    CanFrame rightMux = {.id = 2047};
    rightMux.data[0] = 0x03;
    TEST_ASSERT_TRUE(pluginProcessFrame(rightMux, driver));
    TEST_ASSERT_EQUAL_size_t(1, driver.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x03, driver.sent[0].data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x10, driver.sent[0].data[1] & 0x10);
}

void test_bus_pin_matches_known_bus_and_allows_unknown_bus()
{
    installPlugin(R"JSON({
      "name":"bus pin",
      "rules":[{
        "id":1021,
        "bus":"VEH",
        "mux":0,
        "ops":[
          {"type":"set_byte","byte":0,"val":0,"mask":7},
          {"type":"set_bit","bit":38,"val":1}
        ]
      }]
    })JSON");

    MockDriver driver;

    CanFrame chFrame = {.id = 1021};
    chFrame.bus = CAN_BUS_CH;
    TEST_ASSERT_FALSE(pluginProcessFrame(chFrame, driver));
    TEST_ASSERT_EQUAL_size_t(0, driver.sent.size());

    CanFrame unknownFrame = {.id = 1021};
    unknownFrame.bus = CAN_BUS_ANY;
    TEST_ASSERT_TRUE(pluginProcessFrame(unknownFrame, driver));
    TEST_ASSERT_EQUAL_size_t(1, driver.sent.size());

    CanFrame vehFrame = {.id = 1021};
    vehFrame.bus = CAN_BUS_VEH;
    TEST_ASSERT_TRUE(pluginProcessFrame(vehFrame, driver));
    TEST_ASSERT_EQUAL_size_t(2, driver.sent.size());
}

void test_filter_ids_keep_sixteen_rule_ids_when_gtw_silent_is_disabled_without_key()
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
    TEST_ASSERT_EQUAL_UINT8(16, plugin.filterIdCount);
}

void test_gtw_silent_is_disabled_without_custom_key()
{
    PluginData plugin = {};
    TEST_ASSERT_TRUE(pluginParseJson(R"JSON({
      "name":"silent",
      "rules":[{
        "id":2047,
        "mux":3,
        "ops":[{"type":"emit_periodic","interval":100,"gtw_silent":true}]
      }]
    })JSON",
                                     plugin));

    TEST_ASSERT_EQUAL_UINT8(1, plugin.ruleCount);
    TEST_ASSERT_EQUAL_UINT8(1, plugin.rules[0].opCount);
    TEST_ASSERT_FALSE(plugin.rules[0].ops[0].gtwSilent);
    TEST_ASSERT_EQUAL_UINT8(1, plugin.filterIdCount);
    TEST_ASSERT_EQUAL_UINT32(2047, plugin.filterIds[0]);
}

void test_byte_match_gates_0x370_counter_duplicate_plugin()
{
    installPlugin(R"JSON({
      "name":"370 duplicate",
      "rules":[{
        "id":880,
        "match_byte":4,
        "match_mask":192,
        "match_val":0,
        "ops":[
          {"type":"set_byte","byte":3,"val":182},
          {"type":"set_bit","bit":38,"val":1},
          {"type":"counter","byte":6,"mask":15,"step":1},
          {"type":"checksum"}
        ]
      }]
    })JSON");

    MockDriver driver;
    CanFrame frame = {.id = 880};
    frame.data[0] = 0x10;
    frame.data[1] = 0x20;
    frame.data[2] = 0x30;
    frame.data[3] = 0x40;
    frame.data[4] = 0x01;
    frame.data[5] = 0x50;
    frame.data[6] = 0x2F;
    frame.data[7] = 0x00;

    TEST_ASSERT_TRUE(pluginProcessFrame(frame, driver));
    TEST_ASSERT_EQUAL_size_t(1, driver.sent.size());
    TEST_ASSERT_EQUAL_UINT32(880, driver.sent[0].id);
    TEST_ASSERT_EQUAL_HEX8(0xB6, driver.sent[0].data[3]);
    TEST_ASSERT_EQUAL_HEX8(0x41, driver.sent[0].data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x20, driver.sent[0].data[6]);
    TEST_ASSERT_EQUAL_HEX8(computeVehicleChecksum(driver.sent[0]), driver.sent[0].data[7]);

    CanFrame ignored = frame;
    ignored.data[4] = 0x40;
    TEST_ASSERT_FALSE(pluginProcessFrame(ignored, driver));
    TEST_ASSERT_EQUAL_size_t(1, driver.sent.size());

    ignored.data[4] = 0x80;
    TEST_ASSERT_FALSE(pluginProcessFrame(ignored, driver));
    TEST_ASSERT_EQUAL_size_t(1, driver.sent.size());
}

void test_or_and_byte_ops_apply_expected_values()
{
    installPlugin(R"JSON({
      "name":"byte ops",
      "rules":[{
        "id":777,
        "ops":[
          {"type":"or_byte","byte":1,"val":10},
          {"type":"and_byte","byte":2,"val":240}
        ]
      }]
    })JSON");

    MockDriver driver;
    CanFrame frame = {.id = 777};
    frame.dlc = 8;
    frame.data[1] = 0x50;
    frame.data[2] = 0xFF;

    TEST_ASSERT_TRUE(pluginProcessFrame(frame, driver));
    TEST_ASSERT_EQUAL_size_t(1, driver.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x5A, driver.sent[0].data[1]);
    TEST_ASSERT_EQUAL_HEX8(0xF0, driver.sent[0].data[2]);
}

void test_hw3_fsd_activation_rule_reports_diagnostics()
{
    installPlugin(R"JSON({
      "name":"FSD Activation HW3",
      "rules":[{
        "id":1021,
        "mux":0,
        "match_byte":5,
        "match_mask":255,
        "match_val":1,
        "ops":[{"type":"set_bit","bit":46,"val":1}]
      }]
    })JSON");

    MockDriver driver;
    CanFrame frame = {.id = 1021};
    frame.dlc = 8;
    frame.data[0] = 0x00;
    frame.data[1] = 0x00;
    frame.data[2] = 0x00;
    frame.data[3] = 0xD0;
    frame.data[4] = 0x20;
    frame.data[5] = 0x01;
    frame.data[6] = 0x02;
    frame.data[7] = 0x80;

    TEST_ASSERT_TRUE(pluginProcessFrame(frame, driver));
    TEST_ASSERT_EQUAL_size_t(1, driver.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x41, driver.sent[0].data[5]);

    PluginRule &rule = pluginStore[0].rules[0];
    TEST_ASSERT_EQUAL_UINT32(1, rule.diag.matchCount);
    TEST_ASSERT_EQUAL_UINT32(1, rule.diag.changedCount);
    TEST_ASSERT_EQUAL_UINT32(1, rule.diag.sendOkCount);
    TEST_ASSERT_EQUAL_UINT32(0, rule.diag.sendFailCount);
    TEST_ASSERT_EQUAL_HEX8(0x01, rule.diag.lastOriginal[5]);
    TEST_ASSERT_EQUAL_HEX8(0x41, rule.diag.lastModified[5]);
}

void test_invalid_plugin_fields_fail_closed()
{
    PluginData plugin = {};
    TEST_ASSERT_FALSE(pluginParseJson(
        R"JSON({"name":"wrapped byte","rules":[{"id":100,"ops":[{"type":"set_byte","byte":256,"val":1}]}]})JSON",
        plugin));
    TEST_ASSERT_FALSE(pluginParseJson(
        R"JSON({"name":"unknown op","rules":[{"id":100,"ops":[{"type":"typo","byte":0,"val":1}]}]})JSON",
        plugin));
    TEST_ASSERT_FALSE(pluginParseJson(
        R"JSON({"name":"bad bus","rules":[{"id":100,"bus":"VEHICLE","ops":[{"type":"set_bit","bit":0}]}]})JSON",
        plugin));
    TEST_ASSERT_FALSE(pluginParseJson(
        R"JSON({"name":"bad periodic","rules":[{"id":100,"mux":3,"ops":[{"type":"emit_periodic","interval":100}]}]})JSON",
        plugin));
}

void test_operation_outside_frame_dlc_is_not_sent()
{
    installPlugin(R"JSON({
      "name":"short frame",
      "rules":[{"id":321,"ops":[{"type":"set_byte","byte":7,"val":1}]}]
    })JSON");

    MockDriver driver;
    CanFrame frame = {.id = 321};
    frame.dlc = 4;
    TEST_ASSERT_TRUE(pluginProcessFrame(frame, driver));
    TEST_ASSERT_EQUAL_size_t(0, driver.sent.size());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_mux_mask_matches_full_byte_mux);
    RUN_TEST(test_bus_pin_matches_known_bus_and_allows_unknown_bus);
    RUN_TEST(test_filter_ids_keep_sixteen_rule_ids_when_gtw_silent_is_disabled_without_key);
    RUN_TEST(test_gtw_silent_is_disabled_without_custom_key);
    RUN_TEST(test_byte_match_gates_0x370_counter_duplicate_plugin);
    RUN_TEST(test_or_and_byte_ops_apply_expected_values);
    RUN_TEST(test_hw3_fsd_activation_rule_reports_diagnostics);
    RUN_TEST(test_invalid_plugin_fields_fail_closed);
    RUN_TEST(test_operation_outside_frame_dlc_is_not_sent);
    return UNITY_END();
}
