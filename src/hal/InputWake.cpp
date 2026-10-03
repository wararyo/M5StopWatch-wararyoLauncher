#include "InputWake.h"
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_sleep.h>
#include <cstdint>
#include <cstdio>
#include <iterator>
namespace launcher {
namespace {
// KEY.A, KEY.B, the touch controller's INT and M5PM1's IRQ output (the IMU's
// wrist-wear wake-up and USB power, hal/Pm1Irq.h); all idle high and go low
// when active.
constexpr gpio_num_t Pins[] = {GPIO_NUM_2, GPIO_NUM_1, GPIO_NUM_13, GPIO_NUM_12};
// The IRQ output comes last, and is left out when its route was not set up.
size_t pinCount = 0;
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
bool beginInputWake(TaskHandle_t task, bool pm1Irq) {
    const auto err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        std::printf("[Input] wake interrupts unavailable: %s\n", esp_err_to_name(err));
        return false;
    }
    waiter = task;
    pinCount = pm1Irq ? std::size(Pins) : std::size(Pins) - 1;
    for (size_t i = 0; i < pinCount; ++i) {
        const auto pin = Pins[i];
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
    for (size_t i = 0; i < pinCount; ++i)
        if (gpio_get_level(Pins[i])) gpio_intr_enable(Pins[i]);
}
}
