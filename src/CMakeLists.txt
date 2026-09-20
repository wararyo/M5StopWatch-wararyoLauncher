idf_component_register(SRCS "main.cpp" "input/InputController.cpp"
    "ui/Element.cpp" "ui/DigitalWatchFace.cpp" "ui/Text.cpp" "ui/Renderer.cpp"
    "ui/VlwFont.cpp"
    "ui/RenderDiagnostics.cpp" "app/ScreenManager.cpp" "app/AppRuntime.cpp" "hal/M5Hal.cpp" INCLUDE_DIRS "."
    # Subset produced by tools/build_font.py; the full 2.3MiB VLW is not embedded.
    EMBED_FILES "ui/fonts/GenShinGothicMedium28.vlw"
    REQUIRES app_update esp_app_format esp_psram spi_flash esp_hw_support esp_timer esp_driver_usb_serial_jtag)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
