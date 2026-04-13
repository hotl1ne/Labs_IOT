#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "protocol_examples_common.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mqtt_example";

// ==== Топіки ====
#define BUTTON_TOPIC "/topic/button/Kurytsia"
#define LED_TOPIC    "/topic/led/Kurytsia"

// ==== Піни ====
#define BUTTON_GPIO GPIO_NUM_0  
#define LED_GPIO    GPIO_NUM_2  

// ==== Функція публікації натиску кнопки ====
void publish_button_event(esp_mqtt_client_handle_t client, int pressed)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "button", pressed);
    cJSON_AddNumberToObject(root, "time", (int)esp_timer_get_time() / 1000000);

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string != NULL) {
        esp_mqtt_client_publish(client, BUTTON_TOPIC, json_string, 0, 1, 0);
        ESP_LOGI(TAG, "Sent Button JSON: %s", json_string);
        free(json_string);
    }
    cJSON_Delete(root);
}

// ==== Обробник подій MQTT ====
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            esp_mqtt_client_subscribe(client, LED_TOPIC, 1);
            break;

        case MQTT_EVENT_DATA:
            // Перевірка топіка
            if (event->topic_len == strlen(LED_TOPIC) && 
                strncmp(event->topic, LED_TOPIC, event->topic_len) == 0) {
                
                ESP_LOGI(TAG, "Received message: %.*s", event->data_len, event->data);

                cJSON *root = cJSON_ParseWithLength(event->data, event->data_len);
                if (root) {
                    // ШУКАЄМО КЛЮЧ "state"
                    cJSON *state = cJSON_GetObjectItem(root, "state");
                    if (cJSON_IsNumber(state)) {
                        int led_val = state->valueint;
                        gpio_set_level(LED_GPIO, led_val > 0 ? 1 : 0);
                        ESP_LOGI(TAG, "LED GPIO %d set to %d", LED_GPIO, led_val > 0 ? 1 : 0);
                    } else {
                        ESP_LOGW(TAG, "Key 'state' not found in JSON or not a number");
                    }
                    cJSON_Delete(root);
                }
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected");
            break;

        default:
            break;
    }
}

static esp_mqtt_client_handle_t mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://192.168.137.1", 
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    return client;
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");

    // Ініціалізація NVS та мережі
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    // Конфігурація кнопки
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_conf);

    // Конфігурація LED
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    esp_mqtt_client_handle_t client = mqtt_app_start();

    int last_button_state = 1; 
    while (1) {
        int current_state = gpio_get_level(BUTTON_GPIO);
        
        if (current_state == 0 && last_button_state == 1) { // Натиснуто
            publish_button_event(client, 1);
            vTaskDelay(50 / portTICK_PERIOD_MS); 
        } 
        else if (current_state == 1 && last_button_state == 0) { // Відпущено
            publish_button_event(client, 0);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        last_button_state = current_state;
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}