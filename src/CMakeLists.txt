idf_component_register(SRCS "main.cpp" INCLUDE_DIRS "."
    REQUIRES app_update esp_app_format esp_psram spi_flash esp_hw_support)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
