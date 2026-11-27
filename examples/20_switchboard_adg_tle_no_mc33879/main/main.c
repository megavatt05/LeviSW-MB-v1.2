#include "switchboard_app.h"

#include "esp_log.h"

static const char *TAG = "LEVISW_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "LeviSW-MB v1.2: D=ADG1608, E=TLE94112, MC33879=DISABLED");

    const esp_err_t err = switchboard_app_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Контроллер не запущен: %s", esp_err_to_name(err));
        (void)levisw_bsp_drivers_enable(false);
        (void)levisw_bsp_adg1608_select(0U, false);
        (void)levisw_bsp_power_set(false, false, false, false);
    }
}
