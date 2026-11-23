#include "switchboard_app.h"

#include "esp_log.h"

static const char *TAG = "LEVISW_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Запуск программного контроллера LeviSW-MB v1.2");

    const esp_err_t err = switchboard_app_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Контроллер не запущен: %s", esp_err_to_name(err));
        (void)levisw_bsp_power_set(false, false, false, false);
        (void)levisw_bsp_drivers_enable(false);
    }
}
