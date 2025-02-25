#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_spiffs.h"
#include "esp_timer.h"

#define WIFI_SSID "homewifi"
#define WIFI_PASS "pass123"
#define DOWNLOAD_URL "https://www.indianrail.gov.in/enquiry/PNR/PnrEnquiry.html?locale=en"
#define FILE_PATH "/spiffs/stuff.bin"
#define BUFFER_SIZE 16384

const char *tag ="dl";
int file_size;
uint16_t start_time;
FILE *file;

//wifi event + setup
void wifi_handler(void *arg, esp_event_base_t event, int32_t id, void *data){
    if (event == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
    if (event == IP_EVENT && id == IP_EVENT_STA_GOT_IP) ESP_LOGI(tag, "wifi up");
}
void wifi_setup(){
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_register( WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler, NULL);
    esp_event_handler_register( IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_handler, NULL);
    wifi_config_t wifi = { .sta={.ssid = WIFI_SSID, .password = WIFI_PASS}};
    esp_wifi_set_mode( WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi);
    esp_wifi_start();
}

//spiffs setup
void spiffs_setup(){
    esp_vfs_spiffs_conf_t conf = {
        .base_path ="/spiffs", .max_files = 5, .format_if_mount_failed = 1 };
        if (esp_vfs_spiffs_register(&conf)!= ESP_OK)
        ESP_LOGE(tag, "spiffs failed");
}

//http handler + download
esp_err_t http_handler(esp_http_client_event_t *evt){
    if(evt->event_id == HTTP_EVENT_ON_FINISH && file){
        fclose(file);
        file = NULL;
        float speed =(file_size/((esp_timer_get_time() - start_time)/100000))/1024;
        ESP_LOGI(tag, "done, speed %.1f KB/s", speed);

    }
    if(evt->event_id == HTTP_EVENT_ERROR && file){
        fclose(file);
        file= NULL;
    }
    return ESP_OK;
};

void download(void *param){
    vTaskDelay(5000/ portTICK_PERIOD_MS);
    esp_http_client_config_t cfg = {.url = DOWNLOAD_URL, .event_handler = http_handler, .buffer_size = BUFFER_SIZE, . timeout_ms = 15000};
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client && esp_http_client_perform(client) != ESP_OK) ESP_LOGE(tag, "download failed");
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

//main
void app_main(){
    nvs_flash_init();
    wifi_setup();
    spiffs_setup();
    xTaskCreate( download, "dl", 8192,NULL, 5, NULL);
}