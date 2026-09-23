idf_component_register(SRCS "main.cpp" "input/InputController.cpp"
    "ui/Element.cpp" "ui/DigitalWatchFace.cpp" "ui/Text.cpp" "ui/Renderer.cpp"
    "ui/VlwFont.cpp" "ui/IconSet.cpp" "ui/StatsOverlay.cpp"
    "ui/RenderDiagnostics.cpp" "app/ScreenManager.cpp" "app/AppRuntime.cpp" "hal/M5Hal.cpp" "power/DrainLog.cpp"
    "services/TimeService.cpp" "services/LauncherData.cpp"
    "storage/SettingsStore.cpp" "storage/NvsBackend.cpp"
    "apps/SettingsScreen.cpp" "ui/SettingsLayer.cpp"
    "apps/ExternalAppScreen.cpp" "ui/ExternalLayer.cpp"
    "services/StopwatchService.cpp" "apps/StopwatchScreen.cpp" "ui/StopwatchLayer.cpp"
    "multifirm/MultiFirmAdapter.cpp" INCLUDE_DIRS "."
    # Subset produced by tools/build_font.py; the full 2.3MiB VLW is not embedded.
    # App list icon masks produced by tools/build_icons.py from icons/*.png.
    EMBED_FILES "ui/fonts/GenShinGothicMedium28.vlw" "ui/icons/AppIcons.bin"
    REQUIRES app_update esp_app_format nvs_flash esp_psram spi_flash esp_hw_support esp_timer esp_driver_usb_serial_jtag)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
