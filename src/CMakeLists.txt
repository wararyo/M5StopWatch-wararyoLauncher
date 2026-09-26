idf_component_register(SRCS "main.cpp" "input/InputController.cpp"
    "ui/rendering/Element.cpp" "ui/rendering/Renderer.cpp"
    "ui/graphics/Text.cpp" "ui/graphics/VlwFont.cpp"
    "ui/list/ListController.cpp" "ui/list/ListView.cpp"
    "ui/overlays/ToastLayer.cpp" "ui/overlays/StatsOverlay.cpp"
    "host/ScreenManager.cpp" "host/HostRuntime.cpp" "host/HostRenderer.cpp" "host/RenderDiagnostics.cpp"
    "hal/M5Hal.cpp" "hal/InputWake.cpp" "power/DrainLog.cpp" "power/UsbDiag.cpp"
    "services/TimeService.cpp" "features/home/HomeDataSource.cpp" "services/StopwatchService.cpp"
    "storage/SettingsStore.cpp" "storage/NvsBackend.cpp"
    "features/home/HomeLayer.cpp" "features/home/faces/DigitalWatchFace.cpp"
    "features/launcher/AppIcons.cpp" "features/launcher/AppListLayer.cpp" "features/launcher/LauncherController.cpp"
    "features/settings/SettingsScreen.cpp" "features/settings/SettingsLayer.cpp"
    "features/external/ExternalAppScreen.cpp" "features/external/ExternalLayer.cpp"
    "features/stopwatch/StopwatchFormat.cpp" "features/stopwatch/StopwatchScreen.cpp" "features/stopwatch/StopwatchLayer.cpp"
    "features/stopwatch/StopwatchBackgroundInfo.cpp" "features/background/BackgroundInfoHub.cpp"
    "multifirm/MultiFirmAdapter.cpp" INCLUDE_DIRS "."
    # Subset produced by tools/build_font.py; the full 2.3MiB VLW is not embedded.
    # App list icon masks produced by tools/build_icons.py from icons/*.png.
    # The linker symbols (_binary_<name>_start) come from the file names alone,
    # so they do not change with these directories.
    EMBED_FILES "ui/graphics/fonts/GenShinGothicMedium28.vlw" "features/launcher/icons/AppIcons.bin"
    REQUIRES app_update esp_app_format nvs_flash esp_psram spi_flash esp_hw_support esp_timer esp_driver_usb_serial_jtag esp_driver_gpio esp_pm)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
