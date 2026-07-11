idf_component_register(
    SRCS
        "main.cpp"
        "espidf_runtime.cpp"
    INCLUDE_DIRS
        "../include"
        ".."
    REQUIRES
        app_update
        esp_http_client
        esp_http_server
        esp_https_ota
        esp_netif
        esp_wifi
        nvs_flash
        mbedtls
        spi_flash
        spiffs
        driver
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
