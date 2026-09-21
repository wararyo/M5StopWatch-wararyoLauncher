#include "NvsBackend.h"
#include <nvs.h>
#include <nvs_flash.h>
#include <cstdio>
namespace launcher {
namespace {
constexpr const char* Namespace = "launcher";
constexpr const char* Key = "config";
}
bool NvsBackend::begin() {
    const auto err = nvs_flash_init();
    // Deliberately no nvs_flash_erase() on NO_FREE_PAGES / NEW_VERSION_FOUND:
    // that would take the guests' and other firmware's data with it.
    ready_ = err == ESP_OK;
    std::printf("[Nvs] init=%s namespace=%s%s\n", esp_err_to_name(err), Namespace,
                ready_ ? "" : " (defaults only; partition left untouched)");
    return ready_;
}
bool NvsBackend::load(void* data, size_t& size) {
    if (!ready_) return false;
    nvs_handle_t handle = 0;
    if (nvs_open(Namespace, NVS_READONLY, &handle) != ESP_OK) return false;
    const auto err = nvs_get_blob(handle, Key, data, &size);
    nvs_close(handle);
    return err == ESP_OK;
}
bool NvsBackend::save(const void* data, size_t size) {
    if (!ready_) return false;
    nvs_handle_t handle = 0;
    if (nvs_open(Namespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    auto err = nvs_set_blob(handle, Key, data, size);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) std::printf("[Nvs] write failed: %s\n", esp_err_to_name(err));
    return err == ESP_OK;
}
}
