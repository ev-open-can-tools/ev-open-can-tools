#pragma once

#ifdef ESP_PLATFORM

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdarg>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <driver/gpio.h>
#include <esp_app_desc.h>
#include <esp_event.h>
#include <esp_http_client.h>
#include <esp_http_server.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_netif_sntp.h>
#include <esp_ota_ops.h>
#include <esp_spiffs.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <nvs.h>
#include <nvs_flash.h>

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef PSTR
#define PSTR(s) (s)
#endif

#ifndef HEX
#define HEX 16
#endif

#ifndef HIGH
#define HIGH 1
#endif
#ifndef LOW
#define LOW 0
#endif
#ifndef OUTPUT
#define OUTPUT 1
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 2
#endif

#ifndef HTTP_CODE_OK
#define HTTP_CODE_OK 200
#endif

#ifndef HTTPC_STRICT_FOLLOW_REDIRECTS
#define HTTPC_STRICT_FOLLOW_REDIRECTS 1
#endif
#ifndef HTTPC_FORCE_FOLLOW_REDIRECTS
#define HTTPC_FORCE_FOLLOW_REDIRECTS 2
#endif

#ifndef WIFI_AP
#define WIFI_AP WIFI_MODE_AP
#endif
#ifndef WIFI_AP_STA
#define WIFI_AP_STA WIFI_MODE_APSTA
#endif

#ifndef UPDATE_SIZE_UNKNOWN
#define UPDATE_SIZE_UNKNOWN 0
#endif

using std::max;
using std::min;

enum HTTPUploadStatus
{
    UPLOAD_FILE_START,
    UPLOAD_FILE_WRITE,
    UPLOAD_FILE_END,
    UPLOAD_FILE_ABORTED,
};

enum wl_status_t
{
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6,
};

class String
{
public:
    String() = default;
    String(const char *value) : value_(value ? value : "") {}
    String(const std::string &value) : value_(value) {}
    String(char value) : value_(1, value) {}
    String(bool value) : value_(value ? "1" : "0") {}
    String(int value) : value_(std::to_string(value)) {}
    String(unsigned int value) : value_(std::to_string(value)) {}
    String(long value) : value_(std::to_string(value)) {}
    String(unsigned long value) : value_(std::to_string(value)) {}
    String(long long value) : value_(std::to_string(value)) {}
    String(unsigned long long value) : value_(std::to_string(value)) {}
    String(float value, int decimals = 2) { assignFloat(value, decimals); }
    String(double value, int decimals = 2) { assignFloat(value, decimals); }
    String(int value, int base) { assignInt(value, base); }
    String(unsigned int value, int base) { assignUInt(value, base); }
    String(long value, int base) { assignInt(value, base); }
    String(unsigned long value, int base) { assignUInt(value, base); }

    const char *c_str() const { return value_.c_str(); }
    size_t length() const { return value_.length(); }
    bool isEmpty() const { return value_.empty(); }
    void reserve(size_t size) { value_.reserve(size); }
    char charAt(size_t index) const { return index < value_.size() ? value_[index] : '\0'; }
    char operator[](size_t index) const { return charAt(index); }
    int toInt() const { return std::atoi(value_.c_str()); }
    operator std::string() const { return value_; }
    int read() const
    {
        if (readOffset_ >= value_.size())
            return -1;
        return static_cast<unsigned char>(value_[readOffset_++]);
    }
    size_t write(uint8_t c)
    {
        value_ += static_cast<char>(c);
        return 1;
    }
    size_t write(const uint8_t *s, size_t n)
    {
        value_.append(reinterpret_cast<const char *>(s), n);
        return n;
    }

    int indexOf(const char *needle) const
    {
        size_t pos = value_.find(needle ? needle : "");
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }

    int indexOf(char needle) const
    {
        size_t pos = value_.find(needle);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }

    bool startsWith(const char *prefix) const
    {
        std::string p(prefix ? prefix : "");
        return value_.rfind(p, 0) == 0;
    }

    bool endsWith(const char *suffix) const
    {
        std::string s(suffix ? suffix : "");
        return value_.size() >= s.size() && value_.compare(value_.size() - s.size(), s.size(), s) == 0;
    }

    String substring(size_t start) const
    {
        if (start >= value_.size())
            return "";
        return value_.substr(start);
    }

    String substring(size_t start, size_t end) const
    {
        if (start >= value_.size() || end <= start)
            return "";
        return value_.substr(start, std::min(end, value_.size()) - start);
    }

    void toLowerCase()
    {
        std::transform(value_.begin(), value_.end(), value_.begin(), [](unsigned char c)
                       { return std::tolower(c); });
    }

    void toUpperCase()
    {
        std::transform(value_.begin(), value_.end(), value_.begin(), [](unsigned char c)
                       { return std::toupper(c); });
    }

    void replace(const char *from, const char *to)
    {
        std::string f(from ? from : "");
        std::string t(to ? to : "");
        if (f.empty())
            return;
        size_t pos = 0;
        while ((pos = value_.find(f, pos)) != std::string::npos)
        {
            value_.replace(pos, f.length(), t);
            pos += t.length();
        }
    }

    String &operator=(const char *value)
    {
        value_ = value ? value : "";
        return *this;
    }

    String &operator+=(const String &other)
    {
        value_ += other.value_;
        return *this;
    }

    String &operator+=(const char *other)
    {
        value_ += other ? other : "";
        return *this;
    }

    String &operator+=(char other)
    {
        value_ += other;
        return *this;
    }

    template <typename T>
    String &operator+=(T value)
    {
        value_ += String(value).value_;
        return *this;
    }

    friend String operator+(const String &lhs, const String &rhs) { return String(lhs.value_ + rhs.value_); }
    friend String operator+(const char *lhs, const String &rhs) { return String(lhs) + rhs; }
    friend bool operator==(const String &lhs, const String &rhs) { return lhs.value_ == rhs.value_; }
    friend bool operator!=(const String &lhs, const String &rhs) { return !(lhs == rhs); }
    friend bool operator==(const String &lhs, const char *rhs) { return lhs.value_ == (rhs ? rhs : ""); }
    friend bool operator!=(const String &lhs, const char *rhs) { return !(lhs == rhs); }

private:
    template <typename T>
    void assignFloat(T value, int decimals)
    {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%.*f", decimals, static_cast<double>(value));
        value_ = buf;
    }

    template <typename T>
    void assignInt(T value, int base)
    {
        if (base == HEX)
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%lx", static_cast<long>(value));
            value_ = buf;
        }
        else
            value_ = std::to_string(value);
    }

    template <typename T>
    void assignUInt(T value, int base)
    {
        if (base == HEX)
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%lx", static_cast<unsigned long>(value));
            value_ = buf;
        }
        else
            value_ = std::to_string(value);
    }

    std::string value_;
    mutable size_t readOffset_ = 0;
};

class SerialClass
{
public:
    void begin(unsigned long baud);
    explicit operator bool() const { return true; }
    void println(const String &value);
    void println(const char *value);
    void println(int value);
    void println(unsigned long value);
    void print(const String &value);
    void print(const char *value);
    int printf(const char *fmt, ...);
    size_t write(uint8_t value);
    size_t write(const uint8_t *data, size_t len);
    int available();
    int read();
    int availableForWrite() const;
    bool connected() const;
    void setProtocolMode(bool enabled);
    bool protocolMode() const { return protocolMode_.load(std::memory_order_relaxed); }

private:
    size_t writeTransport(const uint8_t *data, size_t len, TickType_t waitTicks);
    void writeText(const char *data, size_t len);

    bool installed_ = false;
    bool useUsbSerialJtag_ = false;
    std::atomic<bool> protocolMode_{false};
    SemaphoreHandle_t mutex_ = nullptr;
    uint8_t pendingRxByte_ = 0;
    bool pendingRxValid_ = false;
};

extern SerialClass Serial;

class IPAddress
{
public:
    IPAddress() = default;
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { addr_.addr = ESP_IP4TOADDR(a, b, c, d); }
    IPAddress(uint32_t addr) { addr_.addr = addr; }
    bool fromString(const String &value)
    {
        addr_.addr = esp_ip4addr_aton(value.c_str());
        return addr_.addr != 0 || value == "0.0.0.0";
    }
    String toString() const
    {
        char buf[16];
        esp_ip4addr_ntoa(&addr_, buf, sizeof(buf));
        return buf;
    }
    esp_ip4_addr_t raw() const { return addr_; }
    operator uint32_t() const { return addr_.addr; }

private:
    esp_ip4_addr_t addr_{};
};

void delay(uint32_t ms);
void yield();
uint32_t millis();
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
int digitalRead(uint8_t pin);

class Preferences
{
public:
    ~Preferences() { end(); }
    bool begin(const char *name, bool readOnly = false);
    bool end();
    bool isKey(const char *key);
    int8_t getChar(const char *key, int8_t defaultValue = 0);
    uint8_t getUChar(const char *key, uint8_t defaultValue = 0);
    bool getBool(const char *key, bool defaultValue = false);
    String getString(const char *key, const char *defaultValue = "");
    bool putChar(const char *key, int8_t value);
    bool putUChar(const char *key, uint8_t value);
    bool putBool(const char *key, bool value);
    bool putString(const char *key, const String &value);
    bool remove(const char *key);

private:
    nvs_handle_t handle_ = 0;
    bool open_ = false;
    bool readOnly_ = false;
    bool writeOk_ = true;
    bool dirty_ = false;
};

class File
{
public:
    File() = default;
    File(const std::string &path, const char *mode);
    File(DIR *dir, std::string basePath);
    File(File &&other) noexcept { moveFrom(other); }
    File &operator=(File &&other) noexcept
    {
        close();
        moveFrom(other);
        return *this;
    }
    File(const File &) = delete;
    File &operator=(const File &) = delete;
    ~File() { close(); }

    explicit operator bool() const { return fp_ || dir_; }
    bool close();
    size_t write(const uint8_t *buf, size_t len);
    size_t read(uint8_t *buf, size_t len);
    String readString();
    size_t print(const String &value);
    size_t print(unsigned long value);
    size_t println();
    size_t println(const String &value);
    File openNextFile();
    const char *name() const { return name_.c_str(); }
    bool hasError() const { return writeError_; }

private:
    void moveFrom(File &other);

    FILE *fp_ = nullptr;
    DIR *dir_ = nullptr;
    std::string basePath_;
    std::string name_;
    bool writeError_ = false;
};

class SPIFFSClass
{
public:
    bool begin(bool formatOnFail = false);
    File open(const String &path, const char *mode = "r");
    bool exists(const String &path);
    bool remove(const String &path);
    bool rename(const String &from, const String &to);
};

extern SPIFFSClass SPIFFS;

class WiFiClass
{
public:
    void mode(wifi_mode_t mode);
    void persistent(bool) {}
    void setSleep(bool) {}
    bool softAPConfig(IPAddress local, IPAddress gateway, IPAddress subnet);
    bool softAP(const char *ssid, const char *pass, int channel, int hidden, int maxConn);
    void begin(const char *ssid, const char *pass);
    wl_status_t status();
    void disconnect(bool wifioff = false, bool eraseap = false);
    bool config(IPAddress local, IPAddress gateway, IPAddress subnet, IPAddress dns);
    IPAddress localIP();
    IPAddress softAPIP();
    int softAPgetStationNum();
    int scanNetworks(bool async = false, bool hidden = false, bool passive = false, uint32_t maxMsPerChan = 300);
    String SSID(int index);
    String SSID();
    int32_t RSSI(int index);
    wifi_auth_mode_t encryptionType(int index);
    int32_t channel(int index);
    void scanDelete();

private:
    bool ensure();

    bool initialized_ = false;
    esp_netif_t *apNetif_ = nullptr;
    esp_netif_t *staNetif_ = nullptr;
    std::vector<wifi_ap_record_t> scanRecords_;
};

extern WiFiClass WiFi;

class WiFiClient
{
public:
    WiFiClient() = default;
    explicit WiFiClient(std::string data) : data_(std::move(data)) {}
    size_t readBytes(uint8_t *buf, size_t len);
    bool connected() const;

private:
    friend class HTTPClient;
    void attach(esp_http_client_handle_t client, uint32_t timeoutMs = 15000)
    {
        client_ = client;
        timeoutMs_ = timeoutMs;
    }

    std::string data_;
    size_t offset_ = 0;
    esp_http_client_handle_t client_ = nullptr;
    uint32_t timeoutMs_ = 15000;
};

class WiFiClientSecure : public WiFiClient
{
public:
    void setInsecure() {}
};

class HTTPClient
{
public:
    ~HTTPClient() { end(); }
    bool begin(WiFiClientSecure &, const String &url);
    void setFollowRedirects(int mode) { followRedirects_ = mode != 0; }
    void setTimeout(uint32_t ms) { timeoutMs_ = ms; }
    void addHeader(const char *name, const char *value);
    int GET();
    String getString();
    int getSize() const { return contentLength_; }
    WiFiClient *getStreamPtr();
    void end();

private:
    String url_;
    String response_;
    WiFiClient stream_;
    std::vector<std::pair<std::string, std::string>> headers_;
    esp_http_client_handle_t client_ = nullptr;
    int contentLength_ = -1;
    uint32_t timeoutMs_ = 15000;
    bool followRedirects_ = false;
};

class UpdateClass
{
public:
    UpdateClass();
    bool begin(size_t size);
    size_t write(const uint8_t *buf, size_t len);
    size_t writeStream(WiFiClient &stream);
    bool end(bool evenIfRemaining = false);
    void abort();
    bool hasError() const { return error_; }
    String errorString();
    bool isRunning() const { return running_; }
    bool isFinished() const { return finished_; }

private:
    void setError(const char *message);
    void lock();
    void unlock();

    const esp_partition_t *partition_ = nullptr;
    esp_ota_handle_t handle_ = 0;
    std::atomic<bool> running_{false};
    std::atomic<bool> finished_{false};
    std::atomic<bool> error_{false};
    char errorText_[64] = {};
    SemaphoreHandle_t mutex_ = nullptr;
};

extern UpdateClass Update;

struct HTTPUpload
{
    HTTPUploadStatus status = UPLOAD_FILE_ABORTED;
    String filename;
    uint8_t *buf = nullptr;
    size_t currentSize = 0;
    size_t totalSize = 0;
};

class WebServer
{
public:
    using Handler = std::function<void()>;

    explicit WebServer(uint16_t port) : port_(port) {}
    void on(const char *uri, http_method method, Handler handler);
    void on(const char *uri, http_method method, Handler handler, Handler uploadHandler);
    void begin();
    bool started() const { return handle_ != nullptr; }
    void handleClient() {}
    bool hasArg(const char *name) const;
    String arg(const char *name) const;
    String uri() const { return currentUri_; }
    void send(int code, const char *type, const String &body);
    void send(int code, const char *type, const char *body) { sendRaw(code, type, body, body ? std::strlen(body) : 0); }
    void send_P(int code, const char *type, const char *body) { sendRaw(code, type, body, body ? std::strlen(body) : 0); }
    void sendRaw(int code, const char *type, const char *body, size_t len);
    void sendHeader(const char *name, const char *value);
    void streamFile(File &file, const char *type);
    bool authenticate(const char *user, const char *pass);
    void requestAuthentication();
    HTTPUpload &upload() { return upload_; }

private:
    struct Route
    {
        String uri;
        http_method method;
        Handler handler;
        Handler uploadHandler;
    };

    static esp_err_t dispatch(httpd_req_t *req);
    esp_err_t handle(httpd_req_t *req);
    bool parseArgs(const std::string &query);
    bool readPostBody(httpd_req_t *req);
    bool readUpload(httpd_req_t *req, const Route &route);
    const Route *findRoute(const char *uri, httpd_method_t method) const;
    void applyHeaders();

    uint16_t port_;
    httpd_handle_t handle_ = nullptr;
    httpd_req_t *currentReq_ = nullptr;
    String currentUri_;
    std::map<std::string, String> args_;
    std::vector<std::pair<std::string, std::string>> headers_;
    std::vector<Route> routes_;
    HTTPUpload upload_;
};

class ESPClass
{
public:
    void restart() { esp_restart(); }
};

extern ESPClass ESP;

using ota_error_t = int;

class ArduinoOTAClass
{
public:
    void setHostname(const char *) {}
    void setPassword(const char *) {}
    template <typename T>
    void onStart(T) {}
    template <typename T>
    void onEnd(T) {}
    template <typename T>
    void onError(T) {}
    void begin() {}
    void handle() {}
};

extern ArduinoOTAClass ArduinoOTA;

#endif
