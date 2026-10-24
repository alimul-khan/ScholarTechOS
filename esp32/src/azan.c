/* The Azan, as the Home Display app plays it (AzanService.java): a Fajr Azan for Fajr when one is loaded, the
 * regular Azan for the other prayers. The files are AAC (ADTS) in the data partition, uploaded like pictures
 * (/data/media/azan.aac and /data/media/azan-fajr.aac), decoded on the fly to the board's speaker (ES8311). */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "azan.h"
#include "bsp/esp-bsp.h"
#include "esp_audio_simple_dec.h"
#include "esp_audio_simple_dec_default.h"
#include "esp_audio_dec_default.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "schedule.h"

static const char *TAG = "azan";
#define AZAN_FILE "/data/media/azan.aac"
#define FAJR_FILE "/data/media/azan-fajr.aac"

static esp_codec_dev_handle_t speaker;
static volatile bool playing, stop;
static volatile int volume = 100, prayer = -1;

static bool exists(const char *path)
{
    struct stat s;
    return stat(path, &s) == 0 && s.st_size > 0;
}

bool azan_ready(int p) { return exists(p == FAJR && exists(FAJR_FILE) ? FAJR_FILE : AZAN_FILE); }
bool azan_playing(void) { return playing; }
int azan_prayer(void) { return playing ? prayer : -1; }
void azan_stop(void) { stop = true; }
void azan_set_volume(int v) { volume = v < 0 ? 0 : v > 100 ? 100 : v; }

static void play_task(void *arg)
{
    const char *path = prayer == FAJR && exists(FAJR_FILE) ? FAJR_FILE : AZAN_FILE;
    FILE *f = fopen(path, "rb");
    esp_audio_simple_dec_handle_t dec = NULL;
    esp_audio_simple_dec_cfg_t cfg = {.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_AAC};
    size_t in_cap = 4096, out_cap = 8192;
    uint8_t *in = heap_caps_malloc(in_cap, MALLOC_CAP_SPIRAM), *out = heap_caps_malloc(out_cap, MALLOC_CAP_SPIRAM);
    bool open = false;
    if (!f || !in || !out || esp_audio_simple_dec_open(&cfg, &dec) != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "cannot play %s", path);
        goto done;
    }
    ESP_LOGI(TAG, "playing %s at %d%%", path, volume);
    int set_volume = -1;
    size_t n;
    while (!stop && (n = fread(in, 1, in_cap, f)) > 0) {
        esp_audio_simple_dec_raw_t raw = {.buffer = in, .len = n, .eos = feof(f)};
        while (!stop && raw.len) {
            esp_audio_simple_dec_out_t frame = {.buffer = out, .len = out_cap};
            esp_audio_err_t err = esp_audio_simple_dec_process(dec, &raw, &frame);
            if (err == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
                uint8_t *grown = heap_caps_realloc(out, frame.needed_size, MALLOC_CAP_SPIRAM);
                if (!grown) goto done;
                out = grown;
                out_cap = frame.needed_size;
                continue;
            }
            if (err != ESP_AUDIO_ERR_OK) { ESP_LOGW(TAG, "decode error %d", err); goto done; }
            raw.buffer += raw.consumed;
            raw.len -= raw.consumed;
            if (!frame.decoded_size) continue;
            if (!open) {
                esp_audio_simple_dec_info_t info;
                esp_audio_simple_dec_get_info(dec, &info);
                esp_codec_dev_sample_info_t fs = {.sample_rate = info.sample_rate, .channel = info.channel, .bits_per_sample = info.bits_per_sample};
                if (esp_codec_dev_open(speaker, &fs) != ESP_CODEC_DEV_OK) { ESP_LOGE(TAG, "speaker"); goto done; }
                open = true;
                ESP_LOGI(TAG, "%lu Hz, %d channel(s)", (unsigned long)info.sample_rate, info.channel);
            }
            if (set_volume != volume) { set_volume = volume; esp_codec_dev_set_out_vol(speaker, set_volume); }
            esp_codec_dev_write(speaker, out, frame.decoded_size);
        }
    }
    ESP_LOGI(TAG, "%s", stop ? "stopped" : "finished");
done:
    if (open) esp_codec_dev_close(speaker);
    if (dec) esp_audio_simple_dec_close(dec);
    if (f) fclose(f);
    free(in);
    free(out);
    playing = false;
    vTaskDelete(NULL);
}

void azan_init(void)
{
    esp_audio_dec_register_default();
    esp_audio_simple_dec_register_default();
    speaker = bsp_audio_codec_speaker_init();
    if (!speaker) ESP_LOGE(TAG, "no speaker");
}

bool azan_play(int p)
{
    if (!speaker || playing || !azan_ready(p)) return false;
    prayer = p;
    stop = false;
    playing = true;
    if (xTaskCreatePinnedToCore(play_task, "azan", 8192, NULL, 6, NULL, 0) != pdPASS) { playing = false; return false; }
    return true;
}
