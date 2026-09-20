#include <M5Unified.h>
#include <multifirm_host.h>
#include <esp_app_desc.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_private/esp_clk.h>
#include <esp_psram.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>
#include <cstdio>
#include <cstring>
#include "app/AppRuntime.h"
#include "hal/M5Hal.h"
#include "ui/Renderer.h"
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
#include "ui/RenderDiagnostics.h"
#endif

#if !defined(MULTIFIRM_HOST) || MULTIFIRM_HOST != 1
#error "The product firmware must be built as a MultiFirm host"
#endif

namespace {
void logHeap(const char* name, uint32_t caps) {
    std::printf("[Memory] %s total=%u free=%u largest=%u bytes\n", name,
                unsigned(heap_caps_get_total_size(caps)),
                unsigned(heap_caps_get_free_size(caps)),
                unsigned(heap_caps_get_largest_free_block(caps)));
}
}

extern "C" void app_main() {
    auto cfg = M5.config();
    cfg.internal_imu = false;
    cfg.internal_rtc = true;
    cfg.internal_mic = false;
    cfg.internal_spk = false;
    cfg.output_power = false;
    cfg.clear_display = true;
    M5.begin(cfg);
    M5.Display.setBrightness(90);

    const auto* app = esp_app_get_description();
    std::printf("[Launcher] firmware=%s version=%s idf=%s\n",
                app->project_name, app->version, esp_get_idf_version());
    std::printf("[Board] recognized=%d display=%dx%d cpu=%dMHz core=%d tick=%dHz\n",
                int(M5.getBoard()), int(M5.Display.width()), int(M5.Display.height()),
                esp_clk_cpu_freq() / 1000000, int(xPortGetCoreID()), CONFIG_FREERTOS_HZ);
    uint32_t flashSize = 0;
    const auto flashResult = esp_flash_get_size(nullptr, &flashSize);
    std::printf("[Memory] flash_query=%s flash=%lu psram=%u bytes\n",
                esp_err_to_name(flashResult), static_cast<unsigned long>(flashSize),
                unsigned(esp_psram_get_size()));
    std::printf("[Config] flash=DIO/80MHz psram=Octal/80MHz cache_instruction=%dKiB "
                "cache_data=%dKiB cache_line=%dB\n",
                CONFIG_ESP32S3_INSTRUCTION_CACHE_SIZE / 1024, CONFIG_ESP32S3_DATA_CACHE_SIZE / 1024,
                CONFIG_ESP32S3_DATA_CACHE_LINE_SIZE);
    logHeap("internal", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    logHeap("psram", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    const auto* running = esp_ota_get_running_partition();
    const bool layoutOk = multifirm::host::isMultiFirmLayout();
    const bool hostOk = running && running->type == ESP_PARTITION_TYPE_APP &&
        running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 &&
        running->address == 0x20000 && running->size == 0x400000 &&
        std::strcmp(running->label, "ota_0") == 0;
    std::printf("[MultiFirm] layout=%s running=%s address=0x%lx size=0x%lx host=%s\n",
                layoutOk ? "valid" : "MISMATCH", running ? running->label : "unknown",
                running ? static_cast<unsigned long>(running->address) : 0UL,
                running ? static_cast<unsigned long>(running->size) : 0UL,
                hostOk ? "valid" : "MISMATCH");

    std::printf("[Launcher] startup complete; starting single-task runtime\n");
    launcher::M5Hal hal;
    // Keep framebuffer metadata / font cache objects off the 8KiB UI stack.
    static launcher::Renderer renderer(M5.Display);
    if (!renderer.begin()) { std::printf("[Renderer] initialization failed\n"); return; }
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    launcher::runRepaintCheck(renderer, M5.Display);
    static launcher::DiagnosticDataSource data;
    std::printf("[RenderDiag] synthetic JST time / battery; no RTC read\n");
#else
    static launcher::DisplayDataSource data; // unavailable until task 4
#endif
    launcher::AppRuntime runtime(hal, renderer, data, M5.Display.width(), M5.Display.height());
    logHeap("ui-internal", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    logHeap("ui-psram", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    runtime.begin();
    launcher::beginRuntimeDiagnostics();
    while (true) {
        runtime.step();
        launcher::runtimeDiagnostics();
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
        launcher::reportRenderDiagnostics(renderer, hal.now());
#endif
        runtime.wait();
    }
}
