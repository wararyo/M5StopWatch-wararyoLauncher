#include "InputWake.h"
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_sleep.h>
#include <cstdint>
#include <cstdio>
namespace launcher {
namespace {
// KEY.A, KEY.B and the touch controller's INT; all idle high and go low when active.
constexpr gpio_num_t Pins[] = {GPIO_NUM_2, GPIO_NUM_1, GPIO_NUM_13};
TaskHandle_t waiter = nullptr;

void IRAM_ATTR onLow(void* arg) {
    // A level interrupt would refire for as long as the pin stays low. Only a
    // notification here: I2C and drawing stay on the UI task (plan.md 6.2).
    gpio_intr_disable(static_cast<gpio_num_t>(reinterpret_cast<intptr_t>(arg)));
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(waiter, &woken);
    if (woken) portYIELD_FROM_ISR();
}
}
bool beginInputWake(TaskHandle_t task) {
    const auto err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        std::printf("[Input] wake interrupts unavailable: %s\n", esp_err_to_name(err));
        return false;
    }
    waiter = task;
    for (const auto pin : Pins) {
        // Low level rather than an edge: light-sleep GPIO wake-up only
        // supports levels and shares the pin's interrupt type.
        gpio_set_intr_type(pin, GPIO_INTR_LOW_LEVEL);
        gpio_isr_handler_add(pin, onLow, reinterpret_cast<void*>(static_cast<intptr_t>(pin)));
        gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
    }
    // The same pins end automatic light sleep (work 8-5); the interrupt then
    // notifies the UI task as it does from an ordinary wait.
    const auto wake = esp_sleep_enable_gpio_wakeup();
    if (wake != ESP_OK) std::printf("[Input] light-sleep wake-up unavailable: %s\n", esp_err_to_name(wake));
    rearmInputWake();
    return true;
}
void rearmInputWake() {
    if (!waiter) return;
    for (const auto pin : Pins)
        if (gpio_get_level(pin)) gpio_intr_enable(pin);
}
}
