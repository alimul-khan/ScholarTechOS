/* Prayer display for the Waveshare ESP32-S3-Touch-LCD-4B: the same display as the Android TV app, on a 480x480 screen.
 * See README.md. */
#include "cJSON.h"
#include "config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "azan.h"
#include "link.h"
#include "nvs_flash.h"
#include <unistd.h>
#include "places.h"
#include "storage.h"
#include "ui.h"
#include "web.h"
#include "weather.h"
#include "wifi.h"

#ifdef FLASH_STRESS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_partition.h"
/* Temporary: hammers NVS, LittleFS and raw erase/write while the screen runs, to prove flash writes are safe. */
static void flash_stress(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    places_init();
    char *r = places_search_json("london", 2);
    ESP_LOGI("stress", "search: %.200s", r);
    free(r);
    nvs_handle_t h;
    nvs_open("stress", NVS_READWRITE, &h);
    static char buf[4096];
    for (int i = 0; i < (int)sizeof(buf); i++) buf[i] = i;
    const esp_partition_t *ota1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    for (int i = 1; i <= 300; i++) {
        nvs_set_i32(h, "n", i);
        nvs_set_blob(h, "b", buf, 1500);
        nvs_commit(h);
        storage_write("/data/stress.bin", buf, sizeof(buf));
        if (ota1) {
            esp_partition_erase_range(ota1, (i % 16) * 4096, 4096);
            esp_partition_write(ota1, (i % 16) * 4096, buf, sizeof(buf));
        }
        free(places_search_json("london", 8));
        if (i % 25 == 0) ESP_LOGI("stress", "%d rounds ok", i);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    unlink("/data/stress.bin");
    nvs_erase_all(h); nvs_commit(h); nvs_close(h);
    ESP_LOGI("stress", "DONE: 300 rounds of NVS + file + raw erase/write, no crash");
    vTaskDelete(NULL);
}
#endif

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
    places_init();     /* the city list for the location search */
    ui_start();
    azan_init();       /* the speaker; the Azan files are in /data/media */
    link_start();      /* prayerdisplay.online: register, pull settings, push local changes */
    wifi_start();      /* joins the saved network (the factory one on the first start) and sets the clock */
    web_start();       /* the settings page on port 80, and /api/screenshot */
    weather_start();   /* open-meteo for the place in the settings, every 30 minutes */
#ifdef FLASH_STRESS
    xTaskCreate(flash_stress, "stress", 6144, NULL, 3, NULL);
#endif
    ESP_LOGI("main", "%s running, screen %s", FW_VERSION, link_id());
}
