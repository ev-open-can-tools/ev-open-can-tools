#include "platform/espidf_runtime.h"

#ifdef ESP_PLATFORM

#include <sys/stat.h>
#include <exception>
#include <limits>
#include <new>
#include <driver/uart.h>
#include <soc/soc_caps.h>
#if SOC_USB_SERIAL_JTAG_SUPPORTED
#include <driver/usb_serial_jtag.h>
#endif

SerialClass Serial;
SPIFFSClass SPIFFS;
WiFiClass WiFi;
UpdateClass Update;
ESPClass ESP;
ArduinoOTAClass ArduinoOTA;

static constexpr const char *kCompatTag = "idf_compat";
static constexpr const char *kSpiffsBase = "/spiffs";
static std::atomic<bool> sntpStarted{false};
static SemaphoreHandle_t sntpMutex = xSemaphoreCreateMutex();

static bool ensureSntpStarted()
{
    if (sntpStarted)
        return true;
    if (!sntpMutex || xSemaphoreTake(sntpMutex, portMAX_DELAY) != pdTRUE)
        return false;
    if (!sntpStarted)
    {
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        esp_err_t err = esp_netif_sntp_init(&config);
        if (err == ESP_OK || err == ESP_ERR_INVALID_STATE)
            sntpStarted = true;
    }
    bool started = sntpStarted;
    xSemaphoreGive(sntpMutex);
    return started;
}

static bool tlsClockValid()
{
    constexpr time_t kEarliestTrustedTime = 1704067200; // 2024-01-01 UTC
    return std::time(nullptr) >= kEarliestTrustedTime;
}

static bool waitForTlsClock()
{
    if (tlsClockValid())
        return true;
    if (!ensureSntpStarted())
        return false;
    esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));
    return tlsClockValid();
}

static void configureWifiLogLevels()
{
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("wifi_init", ESP_LOG_WARN);
    esp_log_level_set("phy_init", ESP_LOG_WARN);
    esp_log_level_set("esp_netif_lwip", ESP_LOG_WARN);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_WARN);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("pp", ESP_LOG_WARN);
    esp_log_level_set("net80211", ESP_LOG_WARN);
}

void SerialClass::begin(unsigned long baud)
{
    if (!mutex_)
        mutex_ = xSemaphoreCreateMutex();
    if (installed_)
        return;
#if SOC_USB_SERIAL_JTAG_SUPPORTED
    usb_serial_jtag_driver_config_t usbConfig = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    esp_err_t usbErr = usb_serial_jtag_driver_install(&usbConfig);
    if (usbErr == ESP_OK || usbErr == ESP_ERR_INVALID_STATE)
    {
        installed_ = true;
        useUsbSerialJtag_ = true;
        return;
    }
#endif
    uart_config_t uartConfig = {};
    uartConfig.baud_rate = static_cast<int>(baud ? baud : 115200);
    uartConfig.data_bits = UART_DATA_8_BITS;
    uartConfig.parity = UART_PARITY_DISABLE;
    uartConfig.stop_bits = UART_STOP_BITS_1;
    uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uartConfig.source_clk = UART_SCLK_DEFAULT;
    esp_err_t paramErr = uart_param_config(UART_NUM_0, &uartConfig);
    esp_err_t installErr = uart_is_driver_installed(UART_NUM_0)
                               ? ESP_OK
                               : uart_driver_install(UART_NUM_0, 512, 512, 0, nullptr, 0);
    installed_ = paramErr == ESP_OK && installErr == ESP_OK;
    useUsbSerialJtag_ = false;
}

size_t SerialClass::writeTransport(const uint8_t *data, size_t len, TickType_t waitTicks)
{
    if (!data || len == 0)
        return 0;
    if (!installed_)
        return std::fwrite(data, 1, len, stdout);
    int written = 0;
#if SOC_USB_SERIAL_JTAG_SUPPORTED
    if (useUsbSerialJtag_)
        written = usb_serial_jtag_write_bytes(data, len, waitTicks);
    else
#endif
        written = uart_write_bytes(UART_NUM_0, data, len);
    return written > 0 ? static_cast<size_t>(written) : 0;
}

void SerialClass::writeText(const char *data, size_t len)
{
    if (!data || protocolMode())
        return;
    if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) != pdTRUE)
        return;
    writeTransport(reinterpret_cast<const uint8_t *>(data), len, pdMS_TO_TICKS(20));
    if (mutex_)
        xSemaphoreGive(mutex_);
}

void SerialClass::print(const String &value) { writeText(value.c_str(), value.length()); }
void SerialClass::print(const char *value)
{
    const char *text = value ? value : "";
    writeText(text, std::strlen(text));
}
void SerialClass::println(const String &value)
{
    print(value);
    writeText("\n", 1);
}
void SerialClass::println(const char *value)
{
    print(value);
    writeText("\n", 1);
}
void SerialClass::println(int value) { printf("%d\n", value); }
void SerialClass::println(unsigned long value) { printf("%lu\n", value); }

int SerialClass::printf(const char *format, ...)
{
    if (!format || protocolMode())
        return 0;
    char buffer[512];
    va_list args;
    va_start(args, format);
    int result = std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (result > 0)
        writeText(buffer, std::min(static_cast<size_t>(result), sizeof(buffer) - 1));
    return result;
}

size_t SerialClass::write(uint8_t value) { return write(&value, 1); }
size_t SerialClass::write(const uint8_t *data, size_t len)
{
    if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) != pdTRUE)
        return 0;
    size_t written = writeTransport(data, len, pdMS_TO_TICKS(20));
    if (mutex_)
        xSemaphoreGive(mutex_);
    return written;
}

int SerialClass::available()
{
    if (!installed_)
        return 0;
#if SOC_USB_SERIAL_JTAG_SUPPORTED
    if (useUsbSerialJtag_)
    {
        if (pendingRxValid_)
            return 1;
        uint8_t byte = 0;
        int count = usb_serial_jtag_read_bytes(&byte, 1, 0);
        if (count == 1)
        {
            pendingRxByte_ = byte;
            pendingRxValid_ = true;
            return 1;
        }
        return 0;
    }
#endif
    size_t buffered = 0;
    return uart_get_buffered_data_len(UART_NUM_0, &buffered) == ESP_OK
               ? static_cast<int>(buffered)
               : 0;
}

int SerialClass::read()
{
    if (!installed_)
        return -1;
    uint8_t byte = 0;
    int count = 0;
#if SOC_USB_SERIAL_JTAG_SUPPORTED
    if (useUsbSerialJtag_)
    {
        if (pendingRxValid_)
        {
            pendingRxValid_ = false;
            return pendingRxByte_;
        }
        count = usb_serial_jtag_read_bytes(&byte, 1, 0);
    }
    else
#endif
        count = uart_read_bytes(UART_NUM_0, &byte, 1, 0);
    return count == 1 ? byte : -1;
}

int SerialClass::availableForWrite() const
{
    if (!installed_ || !connected())
        return 0;
    return 256;
}

bool SerialClass::connected() const
{
#if SOC_USB_SERIAL_JTAG_SUPPORTED
    if (useUsbSerialJtag_)
        return usb_serial_jtag_is_connected();
#endif
    return installed_;
}

void SerialClass::setProtocolMode(bool enabled)
{
    protocolMode_.store(enabled, std::memory_order_relaxed);
    esp_log_level_set("*", enabled ? ESP_LOG_NONE : ESP_LOG_WARN);
    if (!enabled)
        configureWifiLogLevels();
}

static bool urlDecode(const std::string &input, std::string &out)
{
    out.clear();
    out.reserve(input.size());
    auto hexValue = [](char c) -> int
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < input.size(); i++)
    {
        if (input[i] == '\0')
            return false;
        if (input[i] == '+')
        {
            out += ' ';
        }
        else if (input[i] == '%' && i + 2 < input.size())
        {
            int high = hexValue(input[i + 1]);
            int low = hexValue(input[i + 2]);
            if (high < 0 || low < 0 || (high == 0 && low == 0))
                return false;
            out += static_cast<char>((high << 4) | low);
            i += 2;
        }
        else if (input[i] == '%')
            return false;
        else
        {
            out += input[i];
        }
    }
    return true;
}

static bool requestOriginAllowed(httpd_req_t *req)
{
    size_t originLen = httpd_req_get_hdr_value_len(req, "Origin");
    if (originLen == 0)
        return true;
    size_t hostLen = httpd_req_get_hdr_value_len(req, "Host");
    if (originLen > 512 || hostLen == 0 || hostLen > 255)
        return false;
    std::vector<char> origin(originLen + 1, 0);
    std::vector<char> host(hostLen + 1, 0);
    if (httpd_req_get_hdr_value_str(req, "Origin", origin.data(), origin.size()) != ESP_OK ||
        httpd_req_get_hdr_value_str(req, "Host", host.data(), host.size()) != ESP_OK)
        return false;
    return std::string(origin.data()) == std::string("http://") + host.data();
}

void delay(uint32_t ms)
{
    TickType_t ticks = pdMS_TO_TICKS(ms);
    if (ms > 0 && ticks == 0)
        ticks = 1;
    vTaskDelay(ticks);
}

void yield()
{
    taskYIELD();
}

uint32_t millis()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

void pinMode(uint8_t pin, uint8_t mode)
{
    if (!GPIO_IS_VALID_GPIO(pin) || (mode == OUTPUT && !GPIO_IS_VALID_OUTPUT_GPIO(pin)))
        return;
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = mode == OUTPUT ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
    cfg.pull_up_en = mode == INPUT_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);
}

void digitalWrite(uint8_t pin, uint8_t value)
{
    if (GPIO_IS_VALID_OUTPUT_GPIO(pin))
        gpio_set_level(static_cast<gpio_num_t>(pin), value ? 1 : 0);
}

int digitalRead(uint8_t pin)
{
    return GPIO_IS_VALID_GPIO(pin) ? gpio_get_level(static_cast<gpio_num_t>(pin)) : LOW;
}

bool Preferences::begin(const char *name, bool readOnly)
{
    end();
    readOnly_ = readOnly;
    writeOk_ = true;
    dirty_ = false;
    esp_err_t err = nvs_open(name, readOnly ? NVS_READONLY : NVS_READWRITE, &handle_);
    open_ = err == ESP_OK;
    return open_;
}

bool Preferences::end()
{
    bool ok = open_ && writeOk_;
    if (ok && !readOnly_ && dirty_)
        ok = nvs_commit(handle_) == ESP_OK;
    if (open_)
        nvs_close(handle_);
    handle_ = 0;
    open_ = false;
    writeOk_ = true;
    dirty_ = false;
    return ok;
}

bool Preferences::isKey(const char *key)
{
    if (!open_)
        return false;
    size_t required = 0;
    esp_err_t strErr = nvs_get_str(handle_, key, nullptr, &required);
    if (strErr == ESP_OK || strErr == ESP_ERR_NVS_INVALID_LENGTH)
        return true;
    uint8_t value = 0;
    if (nvs_get_u8(handle_, key, &value) == ESP_OK)
        return true;
    int8_t signedValue = 0;
    return nvs_get_i8(handle_, key, &signedValue) == ESP_OK;
}

int8_t Preferences::getChar(const char *key, int8_t defaultValue)
{
    int8_t value = defaultValue;
    if (open_)
        nvs_get_i8(handle_, key, &value);
    return value;
}

uint8_t Preferences::getUChar(const char *key, uint8_t defaultValue)
{
    uint8_t value = defaultValue;
    if (open_)
        nvs_get_u8(handle_, key, &value);
    return value;
}

bool Preferences::getBool(const char *key, bool defaultValue)
{
    uint8_t value = defaultValue ? 1 : 0;
    if (open_)
        nvs_get_u8(handle_, key, &value);
    return value != 0;
}

String Preferences::getString(const char *key, const char *defaultValue)
{
    if (!open_)
        return defaultValue;
    size_t len = 0;
    if (nvs_get_str(handle_, key, nullptr, &len) != ESP_OK || len == 0 || len > 4096)
        return defaultValue;
    std::vector<char> buf;
    try
    {
        buf.resize(len);
    }
    catch (const std::bad_alloc &)
    {
        return defaultValue;
    }
    if (nvs_get_str(handle_, key, buf.data(), &len) != ESP_OK)
        return defaultValue;
    return buf.data();
}

bool Preferences::putChar(const char *key, int8_t value)
{
    if (open_ && !readOnly_)
    {
        bool ok = nvs_set_i8(handle_, key, value) == ESP_OK;
        writeOk_ &= ok;
        dirty_ |= ok;
        return ok;
    }
    return false;
}

bool Preferences::putUChar(const char *key, uint8_t value)
{
    if (open_ && !readOnly_)
    {
        bool ok = nvs_set_u8(handle_, key, value) == ESP_OK;
        writeOk_ &= ok;
        dirty_ |= ok;
        return ok;
    }
    return false;
}

bool Preferences::putBool(const char *key, bool value)
{
    return putUChar(key, value ? 1 : 0);
}

bool Preferences::putString(const char *key, const String &value)
{
    if (open_ && !readOnly_)
    {
        bool ok = nvs_set_str(handle_, key, value.c_str()) == ESP_OK;
        writeOk_ &= ok;
        dirty_ |= ok;
        return ok;
    }
    return false;
}

bool Preferences::remove(const char *key)
{
    if (open_ && !readOnly_)
    {
        esp_err_t err = nvs_erase_key(handle_, key);
        bool ok = err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND;
        writeOk_ &= ok;
        dirty_ |= err == ESP_OK;
        return ok;
    }
    return false;
}

File::File(const std::string &path, const char *mode)
{
    std::string full = path.rfind(kSpiffsBase, 0) == 0 ? path : std::string(kSpiffsBase) + path;
    fp_ = std::fopen(full.c_str(), mode);
    name_ = path;
}

File::File(DIR *dir, std::string basePath) : dir_(dir), basePath_(std::move(basePath)) {}

void File::moveFrom(File &other)
{
    fp_ = other.fp_;
    dir_ = other.dir_;
    basePath_ = std::move(other.basePath_);
    name_ = std::move(other.name_);
    writeError_ = other.writeError_;
    other.fp_ = nullptr;
    other.dir_ = nullptr;
    other.writeError_ = false;
}

bool File::close()
{
    bool ok = !writeError_;
    if (fp_)
        ok = std::fclose(fp_) == 0 && ok;
    if (dir_)
        ok = closedir(dir_) == 0 && ok;
    fp_ = nullptr;
    dir_ = nullptr;
    writeError_ = false;
    return ok;
}

size_t File::write(const uint8_t *buf, size_t len)
{
    size_t written = fp_ ? std::fwrite(buf, 1, len, fp_) : 0;
    writeError_ |= written != len;
    return written;
}

size_t File::read(uint8_t *buf, size_t len)
{
    return fp_ ? std::fread(buf, 1, len, fp_) : 0;
}

String File::readString()
{
    if (!fp_)
        return "";
    constexpr size_t kMaxFileText = 32 * 1024 + 1;
    std::string out;
    char buf[256];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), fp_)) > 0)
    {
        if (out.size() + n > kMaxFileText)
        {
            out.resize(kMaxFileText, '\0');
            break;
        }
        out.append(buf, n);
    }
    return out;
}

size_t File::print(const String &value)
{
    return write(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
}

size_t File::print(unsigned long value)
{
    if (!fp_)
        return 0;
    int written = std::fprintf(fp_, "%lu", value);
    if (written < 0)
    {
        writeError_ = true;
        return 0;
    }
    return static_cast<size_t>(written);
}

size_t File::println()
{
    uint8_t newline = '\n';
    return write(&newline, 1);
}

size_t File::println(const String &value)
{
    size_t written = print(value);
    return written + println();
}

File File::openNextFile()
{
    if (!dir_)
        return {};
    dirent *entry = readdir(dir_);
    while (entry && entry->d_name[0] == '.')
        entry = readdir(dir_);
    if (!entry)
        return {};
    std::string rel = basePath_ == "/" ? std::string("/") + entry->d_name : basePath_ + "/" + entry->d_name;
    return File(rel, "r");
}

bool SPIFFSClass::begin(bool formatOnFail)
{
    esp_vfs_spiffs_conf_t conf = {};
    conf.base_path = kSpiffsBase;
    conf.partition_label = nullptr;
    conf.max_files = 8;
    conf.format_if_mount_failed = formatOnFail;
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    return err == ESP_OK || err == ESP_ERR_INVALID_STATE;
}

File SPIFFSClass::open(const String &path, const char *mode)
{
    if (path == "/")
        return File(opendir(kSpiffsBase), "/");
    return File(path.c_str(), mode);
}

bool SPIFFSClass::exists(const String &path)
{
    std::string full = std::string(kSpiffsBase) + path.c_str();
    struct stat st = {};
    return stat(full.c_str(), &st) == 0;
}

bool SPIFFSClass::remove(const String &path)
{
    std::string full = std::string(kSpiffsBase) + path.c_str();
    return std::remove(full.c_str()) == 0;
}

bool SPIFFSClass::rename(const String &from, const String &to)
{
    std::string fullFrom = std::string(kSpiffsBase) + from.c_str();
    std::string fullTo = std::string(kSpiffsBase) + to.c_str();
    return std::rename(fullFrom.c_str(), fullTo.c_str()) == 0;
}

bool WiFiClass::ensure()
{
    if (initialized_)
        return true;
    configureWifiLogLevels();
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return false;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return false;
    if (!apNetif_)
        apNetif_ = esp_netif_create_default_wifi_ap();
    if (!staNetif_)
        staNetif_ = esp_netif_create_default_wifi_sta();
    if (!apNetif_ || !staNetif_)
        return false;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return false;
    initialized_ = true;
    ensureSntpStarted();
    return true;
}

void WiFiClass::mode(wifi_mode_t modeValue)
{
    if (!ensure())
        return;
    esp_wifi_set_mode(modeValue);
    esp_wifi_start();
}

bool WiFiClass::softAPConfig(IPAddress local, IPAddress gateway, IPAddress subnet)
{
    if (!ensure())
        return false;
    esp_netif_ip_info_t ip = {};
    ip.ip = local.raw();
    ip.gw = gateway.raw();
    ip.netmask = subnet.raw();
    esp_netif_dhcps_stop(apNetif_);
    esp_err_t err = esp_netif_set_ip_info(apNetif_, &ip);
    esp_netif_dhcps_start(apNetif_);
    return err == ESP_OK;
}

bool WiFiClass::softAP(const char *ssid, const char *pass, int channelValue, int hidden, int maxConn)
{
    if (!ensure())
        return false;
    wifi_config_t cfg = {};
    std::snprintf(reinterpret_cast<char *>(cfg.ap.ssid), sizeof(cfg.ap.ssid), "%s", ssid ? ssid : "");
    std::snprintf(reinterpret_cast<char *>(cfg.ap.password), sizeof(cfg.ap.password), "%s", pass ? pass : "");
    cfg.ap.ssid_len = std::strlen(reinterpret_cast<const char *>(cfg.ap.ssid));
    cfg.ap.channel = channelValue;
    cfg.ap.max_connection = maxConn;
    cfg.ap.ssid_hidden = hidden;
    cfg.ap.authmode = pass && std::strlen(pass) >= 8 ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN;
    esp_err_t configErr = esp_wifi_set_config(WIFI_IF_AP, &cfg);
    esp_err_t startErr = esp_wifi_start();
    return configErr == ESP_OK && (startErr == ESP_OK || startErr == ESP_ERR_INVALID_STATE);
}

void WiFiClass::begin(const char *ssid, const char *pass)
{
    if (!ensure())
        return;
    wifi_ap_record_t ap = {};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
        esp_wifi_disconnect();
    wifi_config_t cfg = {};
    std::snprintf(reinterpret_cast<char *>(cfg.sta.ssid), sizeof(cfg.sta.ssid), "%s", ssid ? ssid : "");
    std::snprintf(reinterpret_cast<char *>(cfg.sta.password), sizeof(cfg.sta.password), "%s", pass ? pass : "");
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_start();
    esp_wifi_connect();
}

wl_status_t WiFiClass::status()
{
    if (!ensure())
        return WL_DISCONNECTED;
    wifi_ap_record_t ap = {};
    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? WL_CONNECTED : WL_DISCONNECTED;
}

void WiFiClass::disconnect(bool wifioff, bool)
{
    if (!initialized_)
        return;
    esp_wifi_disconnect();
    if (wifioff)
        esp_wifi_stop();
}

bool WiFiClass::config(IPAddress local, IPAddress gateway, IPAddress subnet, IPAddress dns)
{
    if (!ensure())
        return false;
    if (static_cast<uint32_t>(local) == INADDR_NONE)
    {
        esp_err_t err = esp_netif_dhcpc_start(staNetif_);
        return err == ESP_OK || err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED;
    }

    esp_netif_dhcpc_stop(staNetif_);
    esp_netif_ip_info_t ip = {};
    ip.ip = local.raw();
    ip.gw = gateway.raw();
    ip.netmask = subnet.raw();
    if (esp_netif_set_ip_info(staNetif_, &ip) != ESP_OK)
    {
        esp_netif_dhcpc_start(staNetif_);
        return false;
    }
    if (static_cast<uint32_t>(dns) != 0)
    {
        esp_netif_dns_info_t dnsInfo = {};
        dnsInfo.ip.type = ESP_IPADDR_TYPE_V4;
        dnsInfo.ip.u_addr.ip4 = dns.raw();
        if (esp_netif_set_dns_info(staNetif_, ESP_NETIF_DNS_MAIN, &dnsInfo) != ESP_OK)
        {
            esp_netif_dhcpc_start(staNetif_);
            return false;
        }
    }
    return true;
}

IPAddress WiFiClass::localIP()
{
    if (!ensure())
        return IPAddress();
    esp_netif_ip_info_t ip = {};
    esp_netif_get_ip_info(staNetif_, &ip);
    return IPAddress(ip.ip.addr);
}

IPAddress WiFiClass::softAPIP()
{
    if (!ensure())
        return 0;
    esp_netif_ip_info_t ip = {};
    esp_netif_get_ip_info(apNetif_, &ip);
    return IPAddress(ip.ip.addr);
}

int WiFiClass::softAPgetStationNum()
{
    if (!ensure())
        return 0;
    wifi_sta_list_t list = {};
    return esp_wifi_ap_get_sta_list(&list) == ESP_OK ? list.num : 0;
}

int WiFiClass::scanNetworks(bool, bool, bool, uint32_t)
{
    if (!ensure())
        return 0;
    scanRecords_.clear();
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) == ESP_OK && mode == WIFI_MODE_AP)
        esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (esp_wifi_scan_start(nullptr, true) != ESP_OK)
        return 0;
    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    scanRecords_.resize(count);
    if (count)
        esp_wifi_scan_get_ap_records(&count, scanRecords_.data());
    scanRecords_.resize(count);
    return static_cast<int>(scanRecords_.size());
}

String WiFiClass::SSID(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= scanRecords_.size())
        return "";
    return reinterpret_cast<const char *>(scanRecords_[index].ssid);
}

String WiFiClass::SSID()
{
    wifi_ap_record_t ap = {};
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK)
        return "";
    return reinterpret_cast<const char *>(ap.ssid);
}

int32_t WiFiClass::RSSI(int index)
{
    return (index >= 0 && static_cast<size_t>(index) < scanRecords_.size()) ? scanRecords_[index].rssi : 0;
}

wifi_auth_mode_t WiFiClass::encryptionType(int index)
{
    return (index >= 0 && static_cast<size_t>(index) < scanRecords_.size()) ? scanRecords_[index].authmode : WIFI_AUTH_OPEN;
}

int32_t WiFiClass::channel(int index)
{
    return (index >= 0 && static_cast<size_t>(index) < scanRecords_.size()) ? scanRecords_[index].primary : 0;
}

void WiFiClass::scanDelete()
{
    scanRecords_.clear();
}

size_t WiFiClient::readBytes(uint8_t *buf, size_t len)
{
    if (client_)
    {
        uint32_t started = millis();
        while (millis() - started < timeoutMs_)
        {
            int read = esp_http_client_read(client_, reinterpret_cast<char *>(buf), len);
            if (read > 0)
                return static_cast<size_t>(read);
            if (read < 0 || esp_http_client_is_complete_data_received(client_))
                return 0;
            vTaskDelay(1);
        }
        return 0;
    }
    size_t available = data_.size() - std::min(offset_, data_.size());
    size_t count = std::min(len, available);
    if (count)
    {
        std::memcpy(buf, data_.data() + offset_, count);
        offset_ += count;
    }
    return count;
}

bool WiFiClient::connected() const
{
    if (client_)
        return !esp_http_client_is_complete_data_received(client_);
    return offset_ < data_.size();
}

bool HTTPClient::begin(WiFiClientSecure &, const String &url)
{
    end();
    headers_.clear();
    url_ = url;
    response_ = "";
    return true;
}

void HTTPClient::addHeader(const char *name, const char *value)
{
    if (name && *name)
        headers_.push_back({name, value ? value : ""});
}

int HTTPClient::GET()
{
    end();
    bool requireHttps = url_.startsWith("https://");
    if (requireHttps && !waitForTlsClock())
        return -1;
    esp_http_client_config_t cfg = {};
    cfg.url = url_.c_str();
    cfg.timeout_ms = timeoutMs_;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.disable_auto_redirect = true;
    client_ = esp_http_client_init(&cfg);
    if (!client_)
        return -1;

    for (const auto &header : headers_)
        esp_http_client_set_header(client_, header.first.c_str(), header.second.c_str());

    constexpr int kMaxRedirects = 10;
    for (int redirect = 0;; redirect++)
    {
        esp_err_t err = esp_http_client_open(client_, 0);
        if (err != ESP_OK)
        {
            end();
            return -1;
        }
        int64_t fetchedLength = esp_http_client_fetch_headers(client_);
        if (fetchedLength < 0)
        {
            end();
            return -1;
        }
        int status = esp_http_client_get_status_code(client_);
        bool redirected = status >= 300 && status < 400;
        if (!redirected || !followRedirects_)
        {
            int64_t length = esp_http_client_get_content_length(client_);
            contentLength_ = esp_http_client_is_chunked_response(client_) ||
                                     length > std::numeric_limits<int>::max()
                                 ? -1
                                 : static_cast<int>(length);
            stream_.attach(client_, timeoutMs_);
            return status;
        }
        if (redirect >= kMaxRedirects || esp_http_client_set_redirection(client_) != ESP_OK)
        {
            end();
            return -1;
        }
        if (requireHttps)
        {
            char redirectedUrl[2048] = {};
            if (esp_http_client_get_url(client_, redirectedUrl, sizeof(redirectedUrl)) != ESP_OK ||
                std::strncmp(redirectedUrl, "https://", 8) != 0)
            {
                end();
                return -1;
            }
        }
        esp_http_client_close(client_);
    }
}

String HTTPClient::getString()
{
    if (!client_)
        return response_;
    constexpr size_t kMaxTextResponse = 256 * 1024;
    if (contentLength_ > static_cast<int>(kMaxTextResponse))
        return "";
    try
    {
        std::string body;
        if (contentLength_ > 0)
            body.reserve(static_cast<size_t>(contentLength_));
        uint8_t buf[512];
        size_t read = 0;
        while ((read = stream_.readBytes(buf, sizeof(buf))) > 0)
        {
            if (body.size() + read > kMaxTextResponse)
            {
                response_ = "";
                return response_;
            }
            body.append(reinterpret_cast<const char *>(buf), read);
            yield();
        }
        response_ = body;
    }
    catch (const std::bad_alloc &)
    {
        response_ = "";
    }
    return response_;
}

WiFiClient *HTTPClient::getStreamPtr()
{
    return &stream_;
}

void HTTPClient::end()
{
    stream_.attach(nullptr);
    if (client_)
    {
        esp_http_client_close(client_);
        esp_http_client_cleanup(client_);
        client_ = nullptr;
    }
    response_ = "";
    contentLength_ = -1;
}

UpdateClass::UpdateClass() : mutex_(xSemaphoreCreateRecursiveMutex())
{
    if (!mutex_)
        setError("OTA mutex unavailable");
}

void UpdateClass::setError(const char *message)
{
    error_ = true;
    std::snprintf(errorText_, sizeof(errorText_), "%s", message ? message : "OTA error");
}

void UpdateClass::lock()
{
    if (mutex_)
        xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
}

void UpdateClass::unlock()
{
    if (mutex_)
        xSemaphoreGiveRecursive(mutex_);
}

String UpdateClass::errorString()
{
    lock();
    try
    {
        String result(errorText_);
        unlock();
        return result;
    }
    catch (...)
    {
        unlock();
        throw;
    }
}

bool UpdateClass::begin(size_t size)
{
    if (!mutex_)
        return false;
    lock();
    if (running_)
    {
        unlock();
        return false;
    }
    abort();
    partition_ = esp_ota_get_next_update_partition(nullptr);
    if (!partition_)
    {
        setError("No OTA partition");
        unlock();
        return false;
    }
    if (size != UPDATE_SIZE_UNKNOWN && size > partition_->size)
    {
        setError("Image exceeds OTA partition");
        partition_ = nullptr;
        unlock();
        return false;
    }
    esp_err_t err = esp_ota_begin(partition_, size == UPDATE_SIZE_UNKNOWN ? OTA_SIZE_UNKNOWN : size, &handle_);
    if (err != ESP_OK)
    {
        setError(esp_err_to_name(err));
        unlock();
        return false;
    }
    running_ = true;
    finished_ = false;
    error_ = false;
    errorText_[0] = '\0';
    unlock();
    return true;
}

size_t UpdateClass::write(const uint8_t *buf, size_t len)
{
    lock();
    if (!running_)
    {
        unlock();
        return 0;
    }
    esp_err_t err = esp_ota_write(handle_, buf, len);
    if (err != ESP_OK)
    {
        setError(esp_err_to_name(err));
        unlock();
        return 0;
    }
    unlock();
    return len;
}

size_t UpdateClass::writeStream(WiFiClient &stream)
{
    uint8_t buf[1024];
    size_t total = 0;
    size_t n = 0;
    while ((n = stream.readBytes(buf, sizeof(buf))) > 0)
    {
        size_t written = write(buf, n);
        total += written;
        if (written != n)
            break;
    }
    return total;
}

bool UpdateClass::end(bool)
{
    lock();
    if (!running_)
    {
        unlock();
        return false;
    }
    esp_err_t err = esp_ota_end(handle_);
    running_ = false;
    if (err != ESP_OK)
    {
        setError(esp_err_to_name(err));
        unlock();
        return false;
    }
    err = esp_ota_set_boot_partition(partition_);
    if (err != ESP_OK)
    {
        setError(esp_err_to_name(err));
        unlock();
        return false;
    }
    finished_ = true;
    unlock();
    return true;
}

void UpdateClass::abort()
{
    lock();
    if (running_)
        esp_ota_abort(handle_);
    running_ = false;
    finished_ = false;
    handle_ = 0;
    partition_ = nullptr;
    unlock();
}

void WebServer::on(const char *uriValue, http_method methodValue, Handler handler)
{
    routes_.push_back({uriValue, methodValue, std::move(handler), nullptr});
}

void WebServer::on(const char *uriValue, http_method methodValue, Handler handler, Handler uploadHandler)
{
    routes_.push_back({uriValue, methodValue, std::move(handler), std::move(uploadHandler)});
}

void WebServer::begin()
{
    if (handle_)
        return;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = port_;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.max_uri_handlers = 16;
    cfg.stack_size = 16384;
    cfg.recv_wait_timeout = 10;
    cfg.send_wait_timeout = 10;
    if (httpd_start(&handle_, &cfg) != ESP_OK)
        return;
    httpd_uri_t get = {};
    get.uri = "/*";
    get.method = HTTP_GET;
    get.handler = &WebServer::dispatch;
    get.user_ctx = this;
    if (httpd_register_uri_handler(handle_, &get) != ESP_OK)
    {
        httpd_stop(handle_);
        handle_ = nullptr;
        return;
    }
    httpd_uri_t post = get;
    post.method = HTTP_POST;
    if (httpd_register_uri_handler(handle_, &post) != ESP_OK)
    {
        httpd_stop(handle_);
        handle_ = nullptr;
    }
}

esp_err_t WebServer::dispatch(httpd_req_t *req)
{
    auto *self = static_cast<WebServer *>(req->user_ctx);
    return self ? self->handle(req) : ESP_FAIL;
}

const WebServer::Route *WebServer::findRoute(const char *uriValue, httpd_method_t methodValue) const
{
    for (const auto &route : routes_)
    {
        if (route.method == methodValue && route.uri == uriValue)
            return &route;
    }
    return nullptr;
}

bool WebServer::parseArgs(const std::string &query)
{
    size_t start = 0;
    while (start < query.size())
    {
        size_t end = query.find('&', start);
        if (end == std::string::npos)
            end = query.size();
        size_t eq = query.find('=', start);
        if (eq != std::string::npos && eq < end)
        {
            std::string key, value;
            if (!urlDecode(query.substr(start, eq - start), key) || key.empty() ||
                !urlDecode(query.substr(eq + 1, end - eq - 1), value))
                return false;
            args_[key] = value.c_str();
        }
        start = end + 1;
    }
    return true;
}

bool WebServer::readPostBody(httpd_req_t *req)
{
    if (req->content_len == 0)
        return true;
    constexpr size_t kMaxPostBody = 64 * 1024;
    if (req->content_len > kMaxPostBody)
        return false;
    std::string body(req->content_len, '\0');
    size_t offset = 0;
    uint32_t lastDataAt = millis();
    while (offset < body.size())
    {
        int got = httpd_req_recv(req, body.data() + offset, body.size() - offset);
        if (got == HTTPD_SOCK_ERR_TIMEOUT)
        {
            if (millis() - lastDataAt >= 15000)
                return false;
            continue;
        }
        if (got <= 0)
            return false;
        offset += got;
        lastDataAt = millis();
    }
    body.resize(offset);
    if (body.find('\0') != std::string::npos)
        return false;
    args_["plain"] = body.c_str();
    char contentType[96] = {};
    static constexpr char kFormContentType[] = "application/x-www-form-urlencoded";
    if (httpd_req_get_hdr_value_str(req, "Content-Type", contentType, sizeof(contentType)) == ESP_OK &&
        std::strncmp(contentType, kFormContentType, sizeof(kFormContentType) - 1) == 0 && !parseArgs(body))
        return false;
    return true;
}

bool WebServer::readUpload(httpd_req_t *req, const Route &route)
{
    char sizeHeader[24] = {};
    if (httpd_req_get_hdr_value_str(req, "X-File-Size", sizeHeader, sizeof(sizeHeader)) != ESP_OK)
        return false;
    for (const char *p = sizeHeader; *p; p++)
    {
        if (*p < '0' || *p > '9')
            return false;
    }
    char *end = nullptr;
    unsigned long long parsedSize = std::strtoull(sizeHeader, &end, 10);
    if (!end || *end != '\0' || parsedSize == 0 || parsedSize > std::numeric_limits<size_t>::max())
        return false;
    size_t fileSize = static_cast<size_t>(parsedSize);
    constexpr size_t kMaxMultipartOverhead = 16 * 1024;
    const esp_partition_t *partition = esp_ota_get_next_update_partition(nullptr);
    if (!partition || fileSize > partition->size || req->content_len < fileSize ||
        req->content_len - fileSize > kMaxMultipartOverhead)
        return false;

    char filename[128] = {};
    httpd_req_get_hdr_value_str(req, "X-File-Name", filename, sizeof(filename));
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(filename); *p; p++)
    {
        if (*p < 0x20 || *p == 0x7F)
            return false;
    }
    bool finalCallbackStarted = false;
    try
    {
        upload_.filename = filename;
        upload_.totalSize = fileSize;
        upload_.status = UPLOAD_FILE_START;
        route.uploadHandler();

        uint8_t buf[1024];
        size_t requestRead = 0;
        size_t fileRead = 0;
        bool foundBody = false;
        uint32_t lastDataAt = millis();
        std::string prefix;
        prefix.reserve(2048);
        while (requestRead < req->content_len)
        {
            size_t wanted = std::min(sizeof(buf), req->content_len - requestRead);
            int got = httpd_req_recv(req, reinterpret_cast<char *>(buf), wanted);
            if (got == HTTPD_SOCK_ERR_TIMEOUT)
            {
                if (millis() - lastDataAt >= 15000)
                    break;
                continue;
            }
            if (got <= 0)
                break;
            requestRead += static_cast<size_t>(got);
            lastDataAt = millis();

            if (!foundBody)
            {
                prefix.append(reinterpret_cast<const char *>(buf), static_cast<size_t>(got));
                size_t marker = prefix.find("\r\n\r\n");
                if (marker == std::string::npos)
                {
                    if (prefix.size() > 2048)
                        break;
                    continue;
                }
                foundBody = true;
                marker += 4;
                size_t available = prefix.size() - marker;
                size_t chunk = std::min(available, fileSize - fileRead);
                if (chunk)
                {
                    upload_.status = UPLOAD_FILE_WRITE;
                    upload_.buf = reinterpret_cast<uint8_t *>(prefix.data() + marker);
                    upload_.currentSize = chunk;
                    route.uploadHandler();
                    fileRead += chunk;
                }
                prefix.clear();
                continue;
            }

            size_t chunk = std::min(static_cast<size_t>(got), fileSize - fileRead);
            if (chunk)
            {
                upload_.status = UPLOAD_FILE_WRITE;
                upload_.buf = buf;
                upload_.currentSize = chunk;
                route.uploadHandler();
                fileRead += chunk;
            }
        }

        upload_.buf = nullptr;
        upload_.currentSize = 0;
        upload_.status = foundBody && fileRead == fileSize ? UPLOAD_FILE_END : UPLOAD_FILE_ABORTED;
        finalCallbackStarted = true;
        route.uploadHandler();
        return upload_.status == UPLOAD_FILE_END;
    }
    catch (...)
    {
        if (!finalCallbackStarted)
        {
            upload_.buf = nullptr;
            upload_.currentSize = 0;
            upload_.status = UPLOAD_FILE_ABORTED;
            route.uploadHandler();
        }
        throw;
    }
}

esp_err_t WebServer::handle(httpd_req_t *req)
{
    requestCount_.fetch_add(1, std::memory_order_relaxed);
    currentReq_ = req;
    try
    {
        std::string routeUri = req->uri;
        size_t queryOffset = routeUri.find('?');
        if (queryOffset != std::string::npos)
            routeUri.resize(queryOffset);
        currentUri_ = routeUri;
        args_.clear();
        headers_.clear();
        upload_ = {};

        size_t qlen = httpd_req_get_url_query_len(req);
        if (qlen > 0)
        {
            std::vector<char> query(qlen + 1, 0);
            if (httpd_req_get_url_query_str(req, query.data(), query.size()) != ESP_OK || !parseArgs(query.data()))
            {
                send(400, "text/plain", "Invalid query string");
                currentReq_ = nullptr;
                return ESP_OK;
            }
        }
        const Route *route = findRoute(routeUri.c_str(), static_cast<httpd_method_t>(req->method));
        if (!route || !route->handler)
        {
            send(404, "text/plain", "Not found");
            currentReq_ = nullptr;
            return ESP_OK;
        }
        if (req->method == HTTP_POST && !requestOriginAllowed(req))
        {
            send(403, "text/plain", "Cross-origin request rejected");
            currentReq_ = nullptr;
            return ESP_OK;
        }
        if (req->method == HTTP_POST)
        {
            bool readOk = route->uploadHandler ? readUpload(req, *route) : readPostBody(req);
            if (!readOk)
            {
                int status = !route->uploadHandler && req->content_len > 64 * 1024 ? 413 : 400;
                send(status, "text/plain", "Invalid request body");
                currentReq_ = nullptr;
                return ESP_OK;
            }
        }
        route->handler();
        currentReq_ = nullptr;
        return ESP_OK;
    }
    catch (const std::bad_alloc &)
    {
        send(500, "text/plain", "Out of memory");
        currentReq_ = nullptr;
        return ESP_OK;
    }
    catch (const std::exception &)
    {
        send(500, "text/plain", "Request failed");
        currentReq_ = nullptr;
        return ESP_OK;
    }
    catch (...)
    {
        send(500, "text/plain", "Request failed");
        currentReq_ = nullptr;
        return ESP_OK;
    }
}

bool WebServer::hasArg(const char *name) const
{
    return args_.find(name ? name : "") != args_.end();
}

String WebServer::arg(const char *name) const
{
    auto it = args_.find(name ? name : "");
    return it == args_.end() ? String("") : it->second;
}

void WebServer::applyHeaders()
{
    if (!currentReq_)
        return;
    for (const auto &header : headers_)
        httpd_resp_set_hdr(currentReq_, header.first.c_str(), header.second.c_str());
}

void WebServer::send(int code, const char *type, const String &body)
{
    sendRaw(code, type, body.c_str(), body.length());
}

void WebServer::sendRaw(int code, const char *type, const char *body, size_t len)
{
    if (!currentReq_)
        return;
    responseBytes_.fetch_add(len, std::memory_order_relaxed);
    uint32_t previousMax = maxResponseBytes_.load(std::memory_order_relaxed);
    while (len > previousMax &&
           !maxResponseBytes_.compare_exchange_weak(previousMax, static_cast<uint32_t>(len),
                                                    std::memory_order_relaxed))
    {
    }
    const char *status = "500 Internal Server Error";
    switch (code)
    {
    case 200:
        status = "200 OK";
        break;
    case 202:
        status = "202 Accepted";
        break;
    case 400:
        status = "400 Bad Request";
        break;
    case 401:
        status = "401 Unauthorized";
        break;
    case 403:
        status = "403 Forbidden";
        break;
    case 404:
        status = "404 Not Found";
        break;
    case 413:
        status = "413 Payload Too Large";
        break;
    case 500:
        status = "500 Internal Server Error";
        break;
    case 502:
        status = "502 Bad Gateway";
        break;
    case 503:
        status = "503 Service Unavailable";
        break;
    }
    httpd_resp_set_status(currentReq_, status);
    httpd_resp_set_type(currentReq_, type);
    applyHeaders();

    constexpr size_t kChunk = 4096;
    if (!body || len == 0)
    {
        httpd_resp_send(currentReq_, nullptr, 0);
        return;
    }
    if (len <= kChunk)
    {
        httpd_resp_send(currentReq_, body, len);
        return;
    }
    size_t offset = 0;
    while (offset < len)
    {
        size_t n = std::min(kChunk, len - offset);
        if (httpd_resp_send_chunk(currentReq_, body + offset, n) != ESP_OK)
            return;
        offset += n;
    }
    httpd_resp_send_chunk(currentReq_, nullptr, 0);
}

void WebServer::sendHeader(const char *name, const char *value)
{
    headers_.push_back({name ? name : "", value ? value : ""});
}

void WebServer::streamFile(File &file, const char *type)
{
    if (!currentReq_)
        return;
    httpd_resp_set_type(currentReq_, type);
    applyHeaders();
    uint8_t buf[512];
    size_t n = 0;
    while ((n = file.read(buf, sizeof(buf))) > 0)
        httpd_resp_send_chunk(currentReq_, reinterpret_cast<const char *>(buf), n);
    httpd_resp_send_chunk(currentReq_, nullptr, 0);
}

static std::string basicAuthValue(const char *user, const char *pass)
{
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string input = std::string(user ? user : "") + ":" + (pass ? pass : "");
    std::string out = "Basic ";
    for (size_t i = 0; i < input.size(); i += 3)
    {
        uint32_t value = static_cast<uint8_t>(input[i]) << 16;
        if (i + 1 < input.size())
            value |= static_cast<uint8_t>(input[i + 1]) << 8;
        if (i + 2 < input.size())
            value |= static_cast<uint8_t>(input[i + 2]);
        out += alphabet[(value >> 18) & 0x3F];
        out += alphabet[(value >> 12) & 0x3F];
        out += i + 1 < input.size() ? alphabet[(value >> 6) & 0x3F] : '=';
        out += i + 2 < input.size() ? alphabet[value & 0x3F] : '=';
    }
    return out;
}

bool WebServer::authenticate(const char *user, const char *pass)
{
    if (!currentReq_)
        return false;
    size_t len = httpd_req_get_hdr_value_len(currentReq_, "Authorization");
    if (len == 0 || len > 512)
        return false;
    std::vector<char> value(len + 1, 0);
    if (httpd_req_get_hdr_value_str(currentReq_, "Authorization", value.data(), value.size()) != ESP_OK)
        return false;
    return basicAuthValue(user, pass) == value.data();
}

void WebServer::requestAuthentication()
{
    sendHeader("WWW-Authenticate", "Basic realm=\"ev-open-can-tools\"");
    send(401, "text/plain", "Authentication required");
}

#endif
