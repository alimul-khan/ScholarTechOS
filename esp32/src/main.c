/* Prayer display for the Waveshare ESP32-S3-Touch-LCD-4B: the same display as the Android TV app, on a 480x480 screen.
 * See README.md. */
#include "cJSON.h"
#include "config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "link.h"
#include "nvs_flash.h"
#include "places.h"
#include "storage.h"
#include "ui.h"
#include "web.h"
#include "wifi.h"

static void *psram_malloc(size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW("main", "NVS needs erasing (%s)", esp_err_to_name(err));
        nvs_flash_erase();
        nvs_flash_init();
    }
    cJSON_Hooks hooks = {.malloc_fn = psram_malloc, .free_fn = free};
    cJSON_InitHooks(&hooks);
    storage_init();
    link_init();
    config_init();
    /* Step 1: the prayer page only. Wi-Fi, the settings page and the server link are added one at a time. */
    ui_start();
    ESP_LOGI("main", "%s running, screen %s", FW_VERSION, link_id());
}
