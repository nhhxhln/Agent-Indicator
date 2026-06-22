idf_component_register(
    SRCS
        "app_main.c"
        "rgb_panel.c"
        "patterns.c"
        "link_spi.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd driver esp_psram
)
