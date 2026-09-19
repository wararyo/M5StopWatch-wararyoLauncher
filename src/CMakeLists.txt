idf_component_register(
    SRCS
        "main.cpp"
        "app/LauncherApp.cpp"
        "input/InputController.cpp"
        "power/PowerManager.cpp"
        "time/TimeService.cpp"
        "ui/DigitalWatchFace.cpp"
        "ui/Renderer.cpp"
    INCLUDE_DIRS "."
)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)

