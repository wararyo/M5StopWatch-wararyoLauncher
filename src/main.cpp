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
#include "host/HostRenderer.h"
#include "host/HostApplication.h"
#include "hal/M5Hal.h"
#include "multifirm/MultiFirmAdapter.h"
#include "features/home/HomeDataSource.h"
#include "storage/NvsBackend.h"
#ifdef LAUNCHER_RENDER_METRICS
#include "host/RenderDiagnostics.h"
#endif
#ifdef LAUNCHER_DRAIN_LOG
#include "power/DrainLog.h"
#endif
#ifdef LAUNCHER_USB_DIAG
#include "power/UsbDiag.h"
#endif
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
#include "multifirm/FakeSlotService.h"
#endif

// Work 8-3. Building with 240 restores the fixed-frequency behaviour of work 7.
#ifndef LAUNCHER_CPU_MIN_MHZ
#define LAUNCHER_CPU_MIN_MHZ 80
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
    launcher::beginPowerManagement(240, LAUNCHER_CPU_MIN_MHZ);
    // The stored level is applied once NVS has been read; this only keeps the
    // boot screen visible until then.
    M5.Display.setBrightness(launcher::Settings{}.brightness);

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
    // Static: the data source below keeps a reference to it for the whole run.
    static launcher::M5Hal hal;
    // Keep framebuffer metadata / font cache objects off the 8KiB UI stack.
    static launcher::HostRenderer renderer(M5.Display);
    if (!renderer.begin()) { std::printf("[Renderer] initialization failed\n"); return; }
    // Declared for both builds so the settings screen exists either way. The
    // diagnostics build never begins it, so it touches no RTC and reports the
    // clock as unset instead of writing one.
    static launcher::TimeService timeService;
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    // Injected slots: the render check has to draw states a device cannot be
    // put into safely, and it must not read flash behind the UI task.
    static launcher::FakeSlotService slots;
    slots.set(1, launcher::SlotStatus::Ready, "RenderCheck", "1.0.0");
    slots.set(2, launcher::SlotStatus::Empty);
    slots.set(3, launcher::SlotStatus::Invalid, nullptr, nullptr, 0x105);
    launcher::runRepaintCheck(renderer, M5.Display, slots.catalog);
    static launcher::DiagnosticDataSource data;
    std::printf("[RenderDiag] synthetic JST time / battery / slots; no RTC or flash read\n");
#else
    timeService.begin(hal);
    static launcher::HomeDataSource data(hal, timeService);
    static launcher::MultiFirmAdapter slots;
    slots.begin();
#endif
    static launcher::NvsBackend nvs;
    nvs.begin();
    static launcher::SettingsStore settingsStore;
    settingsStore.begin(nvs);
    hal.setBrightness(settingsStore.get().brightness);
    // The application's services, screens and runtime, wired once. Static like
    // the renderer, so it stays off the UI stack.
    static launcher::HostApplication application(hal, renderer, data, M5.Display.width(), M5.Display.height());
    application.bindSettings(settingsStore, timeService);
    application.bindSlots(slots);
    application.bindHome(renderer);
    application.setInfo(app->project_name, app->version, esp_get_idf_version());
    auto& runtime = application.runtime();
    logHeap("ui-internal", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    logHeap("ui-psram", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    hal.beginInputWake();
    runtime.begin();
    launcher::beginRuntimeDiagnostics();
#ifdef LAUNCHER_DRAIN_LOG
    launcher::beginDrainLog(); // After nvs.begin(): the record lives in NVS.
#endif
    while (true) {
        runtime.step();
        launcher::runtimeDiagnostics();
#ifdef LAUNCHER_DRAIN_LOG
        launcher::drainLog(hal.now(), runtime.power());
#endif
#ifdef LAUNCHER_USB_DIAG
        launcher::usbDiag(hal.now(), runtime.power());
#endif
#ifdef LAUNCHER_RENDER_METRICS
        launcher::recordLoop();
        launcher::reportRenderDiagnostics(renderer, hal.now());
#endif
        runtime.wait();
    }
}
